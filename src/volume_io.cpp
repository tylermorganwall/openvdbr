#include "provider_internal.h"
#include "diagnostics.h"
#include "openvdbr/openvdbr_volume_api.h"
#include <nanovdb/GridHandle.h>
#include <nanovdb/tools/GridValidator.h>
#include <nanovdb/tools/CreateNanoGrid.h>
#include <algorithm>
#include <fstream>
#include <blosc.h>
#include <zlib.h>

struct openvdbr_volume_buffer {
    nanovdb::HostBuffer buffer;
    explicit openvdbr_volume_buffer(nanovdb::HostBuffer value) : buffer(std::move(value)) {}
};
namespace {
using openvdbr::require;
struct GridLocation {
  uint64_t offset, size, encoded_size; nanovdb::io::Codec codec;
  std::string name;
};
uint64_t checked_offset(uint64_t base, int64_t offset, uint64_t size) {
  if (base > size)
    throw std::runtime_error("Invalid NanoVDB metadata offset.");
  if (offset < 0) {
    uint64_t magnitude = uint64_t(-(offset + 1)) + 1;
    if (magnitude > base)
      throw std::runtime_error("Invalid NanoVDB metadata offset.");
    return base - magnitude;
  }
  if (uint64_t(offset) > size - base)
    throw std::runtime_error("Invalid NanoVDB metadata offset.");
  return base + uint64_t(offset);
}
std::string raw_name(std::ifstream &f, uint64_t start, const nanovdb::GridData &grid) {
  if (!grid.mFlags.isMaskOn(nanovdb::GridFlags::HasLongGridName)) {
    if (!std::memchr(grid.mGridName, 0, sizeof(grid.mGridName)))
      throw std::runtime_error("Invalid NanoVDB grid name.");
    return grid.mGridName;
  }
  uint64_t offset = checked_offset(0, grid.mBlindMetadataOffset, grid.mGridSize);
  if (grid.mBlindMetadataCount > (grid.mGridSize - offset) / sizeof(nanovdb::GridBlindMetaData))
    throw std::runtime_error("Invalid NanoVDB name metadata.");
  for (uint32_t i = 0; i < grid.mBlindMetadataCount; ++i) {
    nanovdb::GridBlindMetaData m;
    uint64_t position = offset + i * sizeof(m);
    f.seekg(start + position);
    if (!f.read(reinterpret_cast<char *>(&m), sizeof(m)))
      throw std::runtime_error("Truncated NanoVDB name metadata.");
    if (m.mDataClass != nanovdb::GridBlindDataClass::GridName)
      continue;
    uint64_t data = checked_offset(position, m.mDataOffset, grid.mGridSize);
    if (m.mValueSize != 1 || m.mValueCount == 0 || m.mValueCount > 1048576 ||
        m.mValueCount > grid.mGridSize - data)
      throw std::runtime_error("Invalid NanoVDB long grid name.");
    std::vector<char> name(m.mValueCount);
    f.seekg(start + data);
    if (!f.read(name.data(), name.size()) || name.back() != 0)
      throw std::runtime_error("Invalid NanoVDB long grid name.");
    return name.data();
  }
  throw std::runtime_error("Missing NanoVDB long grid name.");
}
std::vector<GridLocation> preflight(const std::string &path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f || f.tellg() < 0)
    throw std::runtime_error("Cannot open NanoVDB file: " + path);
  uint64_t size = uint64_t(f.tellg());
  nanovdb::GridData first{};
  f.seekg(0);
  bool raw = bool(f.read(reinterpret_cast<char *>(&first), sizeof(first))) && first.isValid();
  f.clear();
  f.seekg(0);
  std::vector<GridLocation> grids;
  uint32_t raw_count = 0, raw_remaining = 0;
  while (uint64_t(f.tellg()) < size) {
    if (raw) {
      uint64_t start = uint64_t(f.tellg());
      nanovdb::GridData grid;
      if (!f.read(reinterpret_cast<char *>(&grid), sizeof(grid)) || !grid.isValid() ||
          !grid.mVersion.isCompatible() || grid.mGridCount == 0 ||
          grid.mGridIndex >= grid.mGridCount || grid.mGridSize < sizeof(grid) ||
          grid.mGridSize > size - start)
        throw std::runtime_error("Invalid or truncated raw NanoVDB grid.");
      if (raw_remaining == 0)
        raw_count = raw_remaining = grid.mGridCount;
      if (grid.mGridCount != raw_count || grid.mGridIndex != raw_count - raw_remaining)
        throw std::runtime_error("Inconsistent raw NanoVDB grid indices.");
      --raw_remaining;
      grids.push_back({start, grid.mGridSize, grid.mGridSize, nanovdb::io::Codec::NONE, raw_name(f, start, grid)});
      f.seekg(start + grid.mGridSize);
      continue;
    }
    nanovdb::io::FileHeader header;
    if (!f.read(reinterpret_cast<char *>(&header), sizeof(header)))
      throw std::runtime_error("Truncated NanoVDB header.");
    auto magic = nanovdb::toMagic(header.magic);
    if (magic != nanovdb::MagicType::NanoVDB && magic != nanovdb::MagicType::NanoFile)
      throw std::runtime_error("Expected a NanoVDB or OpenVDB file.");
    if (header.codec != nanovdb::io::Codec::NONE && header.codec != nanovdb::io::Codec::ZIP && header.codec != nanovdb::io::Codec::BLOSC)
      throw std::runtime_error("Unsupported NanoVDB compression codec.");
    if (!header.version.isCompatible() || header.gridCount == 0 ||
        header.gridCount > (size - uint64_t(f.tellg())) / sizeof(nanovdb::io::FileMetaData))
      throw std::runtime_error("Invalid or incompatible NanoVDB header.");
    uint64_t payload = 0;
    size_t first_grid = grids.size();
    for (uint32_t i = 0; i < header.gridCount; ++i) {
      nanovdb::io::FileMetaData m;
      if (!f.read(reinterpret_cast<char *>(&m), sizeof(m)))
        throw std::runtime_error("Truncated NanoVDB metadata.");
      if (m.codec != header.codec)
        throw std::runtime_error("Compressed NanoVDB codec disagrees with its container.");
      if (m.nameSize == 0 || m.nameSize > 1048576 || m.nameSize > size - uint64_t(f.tellg()) ||
          (m.codec == nanovdb::io::Codec::NONE && m.fileSize != m.gridSize) || m.gridSize < sizeof(nanovdb::GridData) ||
          m.fileSize > size - payload || m.fileSize == 0 || m.gridSize % 32 != 0 || m.gridSize > std::numeric_limits<size_t>::max())
        throw std::runtime_error("Invalid NanoVDB grid sizes.");
      std::vector<char> name(m.nameSize);
      if (!f.read(name.data(), m.nameSize) || name.back() != '\0')
        throw std::runtime_error("Invalid NanoVDB grid name.");
      grids.push_back({payload, m.gridSize, m.fileSize, m.codec, name.data()});
      payload += m.fileSize;
    }
    uint64_t start = uint64_t(f.tellg());
    if (payload > size - start)
      throw std::runtime_error("Truncated NanoVDB grid data.");
    for (size_t i = first_grid; i < grids.size(); ++i)
      grids[i].offset += start;
    f.seekg(payload, std::ios::cur);
  }
  if (raw_remaining)
    throw std::runtime_error("Truncated raw NanoVDB grid sequence.");
  if (grids.empty())
    throw std::runtime_error("NanoVDB file contains no grids.");
  return grids;
}
// Validate every pointer before upstream validation dereferences sparse nodes.
// The upstream validator assumes a trustworthy allocation and grid header.
template <class Node>
void check_node_storage(const Node *node, uintptr_t begin, uintptr_t end, size_t &remaining,
                        std::array<uint64_t, 3> &counts) {
  uintptr_t p = reinterpret_cast<uintptr_t>(node);
  if (p < begin || p > end || sizeof(Node) > end - p || p % NANOVDB_DATA_ALIGNMENT || !remaining--)
    throw std::runtime_error("Malformed NanoVDB sparse node offsets.");
  ++counts[Node::LEVEL];
  if constexpr (Node::LEVEL > 0)
    for (auto child = node->cbeginChild(); child; ++child)
      check_node_storage(&*child, begin, end, remaining, counts);
}
void check_storage(const nanovdb::FloatGrid *grid, uint64_t size) {
  using Tree = nanovdb::FloatGrid::TreeType;
  using Root = Tree::RootType;
  uintptr_t begin = reinterpret_cast<uintptr_t>(grid), end = begin + size;
  if (size < sizeof(nanovdb::FloatGrid) + sizeof(Tree) || grid->gridSize() != size)
    throw std::runtime_error("Malformed NanoVDB grid size.");
  const auto *root = static_cast<const Root *>(grid->tree().getRoot());
  uintptr_t p = reinterpret_cast<uintptr_t>(root);
  if (p < begin + sizeof(nanovdb::FloatGrid) + sizeof(Tree) || p > end || sizeof(Root) > end - p ||
      p % NANOVDB_DATA_ALIGNMENT)
    throw std::runtime_error("Malformed NanoVDB root offset.");
  if (root->memUsage() > end - p)
    throw std::runtime_error("Malformed NanoVDB root tile count.");
  const auto &tree = grid->tree();
  const uint64_t node_sizes[3] = {sizeof(Tree::Node0), sizeof(Tree::Node1), sizeof(Tree::Node2)};
  for (int level = 0; level < 3; ++level) {
    int64_t offset = tree.mNodeOffset[level];
    uint64_t count = tree.nodeCount(level);
    if (offset < 0 || uint64_t(offset) > size - sizeof(nanovdb::FloatGrid))
      throw std::runtime_error("Malformed NanoVDB node array offset.");
    uint64_t start = sizeof(nanovdb::FloatGrid) + uint64_t(offset);
    if (count && (start < sizeof(nanovdb::FloatGrid) + sizeof(Tree) || start % 32 ||
                  count > (size - start) / node_sizes[level]))
      throw std::runtime_error("Malformed NanoVDB node array size.");
  }
  size_t remaining = size / 32;
  std::array<uint64_t, 3> counts{};
  for (auto child = root->cbeginChild(); child; ++child)
    check_node_storage(&*child, begin, end, remaining, counts);
  for (int level = 0; level < 3; ++level)
    if (counts[level] != tree.nodeCount(level))
      throw std::runtime_error("NanoVDB sparse node counts do not match its tree.");
}
// NanoVDB ZIP encodes one length-prefixed block. BLOSC encodes blocks of up to
// 1 GiB. Bound every compressed read by the selected grid's recorded payload,
// and verify the decoded size before allowing access to sparse tree pointers.
nanovdb::HostBuffer decode_grid(const std::string& filename, const GridLocation& location) {
    auto buffer = nanovdb::HostBuffer::create(location.size);
    std::ifstream input(filename, std::ios::binary);
    input.seekg(location.offset);
    char* output = static_cast<char*>(buffer.data());
    uint64_t encoded = location.encoded_size, remaining = location.size;
    if (location.codec == nanovdb::io::Codec::NONE) {
        if (!input.read(output, remaining)) throw std::runtime_error("Truncated NanoVDB grid data.");
        return buffer;
    }
    while (remaining) {
        uint64_t bytes = 0;
        require(encoded >= sizeof(bytes) && bool(input.read(reinterpret_cast<char*>(&bytes), sizeof(bytes))),
                "Truncated compressed NanoVDB block header");
        encoded -= sizeof(bytes);
        require(bytes > 0 && bytes <= encoded && bytes <= std::numeric_limits<size_t>::max(),
                "Invalid compressed NanoVDB block size");
        std::vector<char> compressed(static_cast<size_t>(bytes));
        require(bool(input.read(compressed.data(), bytes)), "Truncated compressed NanoVDB block");
        encoded -= bytes;
        uint64_t decoded = 0;
        if (location.codec == nanovdb::io::Codec::ZIP) {
            require(remaining <= std::numeric_limits<uLongf>::max() && bytes <= std::numeric_limits<uLong>::max(),
                    "ZIP NanoVDB block exceeds this platform's zlib size range");
            uLongf size = static_cast<uLongf>(remaining);
            int status = uncompress(reinterpret_cast<Bytef*>(output), &size,
                                    reinterpret_cast<const Bytef*>(compressed.data()), static_cast<uLong>(bytes));
            require(status == Z_OK && size == remaining, "Invalid ZIP NanoVDB data or decoded size");
            decoded = size;
        } else {
            const uint64_t chunk = std::min(remaining, uint64_t(1) << 30);
            size_t size = 0;
            require(blosc_cbuffer_validate(compressed.data(), bytes, &size) == 0 && size == chunk,
                    "Invalid BLOSC NanoVDB block or decoded size");
            int count = blosc_decompress_ctx(compressed.data(), output, chunk, 1);
            require(count > 0 && uint64_t(count) == chunk, "BLOSC NanoVDB decompression failed");
            decoded = chunk;
        }
        remaining -= decoded;
        output += decoded;
    }
    require(encoded == 0, "Compressed NanoVDB grid contains trailing bytes");
    return buffer;
}
void validate_float_buffer(const nanovdb::HostBuffer& buffer) {
    require(buffer.size() >= sizeof(nanovdb::GridData), "Truncated NanoVDB grid header");
    const auto* header = reinterpret_cast<const nanovdb::GridData*>(buffer.data());
    require(header->isValid() && header->mVersion.isCompatible() && header->mGridCount > 0 &&
            header->mGridIndex < header->mGridCount && header->mGridSize == buffer.size(),
            "Invalid or incompatible NanoVDB grid header");
    require(header->mGridType == nanovdb::GridType::Float, "Requested volume is not a float grid");
    const auto* grid = reinterpret_cast<const nanovdb::FloatGrid*>(buffer.data());
    check_storage(grid, buffer.size());
    // Full upstream validation assumes nonempty leaf arrays, rejecting valid
    // tile-only trees. The bounded traversal above covers their sparse nodes.
    require(nanovdb::tools::isValid<float>(grid, nanovdb::CheckMode::Half, false) &&
            nanovdb::tools::validateChecksum(grid, nanovdb::CheckMode::Full),
            "Invalid NanoVDB grid or checksum");
}
int32_t read_float_grid(const char* path, const char* name, int32_t optional,
                       openvdbr_volume_buffer** out, openvdbr_error* error) {
    if (out) *out = nullptr;
    return openvdbr::guarded(error, [&] {
        require(path && path[0] && name && name[0] && out, "Path, grid name and output must not be empty");
        require(optional == 0 || optional == 1, "optional must be zero or one");
        std::ifstream input(path, std::ios::binary);
        uint32_t magic = 0;
        require(bool(input.read(reinterpret_cast<char*>(&magic), sizeof(magic))), "Cannot read NanoVDB/OpenVDB file header");
        nanovdb::HostBuffer buffer;
        if (magic == openvdb::OPENVDB_MAGIC) {
            openvdbr::initialize();
            openvdb::io::File file(path);
            file.open(false);
            if (!file.hasGrid(name)) {
                if (optional) return;
                throw std::runtime_error(std::string("OpenVDB grid '") + name + "' is missing.");
            }
            auto source = file.readGrid(name);
            require(source->isType<openvdb::FloatGrid>(), "Requested OpenVDB volume is not a float grid");
            require(source->transform().isLinear(), "Rendering requires an affine OpenVDB transform");
            auto handle = nanovdb::tools::createNanoGrid(static_cast<const openvdb::FloatGrid&>(*source));
            buffer = std::move(handle.buffer());
        } else {
            auto locations = preflight(path);
            auto found = std::find_if(locations.begin(), locations.end(),
                                      [&](const GridLocation& grid) { return grid.name == name; });
            if (found == locations.end()) {
                if (optional) return;
                throw std::runtime_error(std::string("NanoVDB grid '") + name + "' is missing.");
            }
            buffer = decode_grid(path, *found);
        }
        validate_float_buffer(buffer);
        *out = new openvdbr_volume_buffer(std::move(buffer));
    });
}
void destroy(openvdbr_volume_buffer* buffer) { delete buffer; }
int32_t data(const openvdbr_volume_buffer* buffer, const void** bytes, uint64_t* size,
             openvdbr_error* error) {
    return openvdbr::guarded(error, [&] {
        require(buffer && bytes && size, "Buffer and output pointers must not be NULL");
        *bytes = buffer->buffer.data();
        *size = buffer->buffer.size();
    });
}
}
const openvdbr_volume_api_v1* openvdbr_provider_volume_api() {
    static const openvdbr_volume_api_v1 api = {
        1, sizeof(openvdbr_volume_api_v1), NANOVDB_MAJOR_VERSION_NUMBER, read_float_grid, destroy, data
    };
    return &api;
}
