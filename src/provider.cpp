#include "provider_internal.h"
#include "diagnostics.h"
#include <openvdb/tools/Interpolation.h>
#include <algorithm>

namespace openvdbr {
void initialize() {
    static std::once_flag initialized;
    std::call_once(initialized, [] { openvdb::initialize(); });
}
void validate_geometry(double voxel_size, double half_width, const char* name) {
    require(std::isfinite(voxel_size) && voxel_size >= std::numeric_limits<float>::min() &&
            voxel_size <= std::numeric_limits<float>::max(), "voxel_size must be positive and finite");
    require(std::isfinite(half_width) && half_width >= 1 && half_width <= std::numeric_limits<float>::max() &&
            voxel_size * half_width <= std::numeric_limits<float>::max(),
            "half_width must be at least one voxel and have a representable width");
    require(name != nullptr, "name must not be NULL");
}
}

namespace {
using openvdbr::require;
using openvdbr::guarded;

template<class Function> void visit_scalar(const openvdbr_grid& handle, Function function) {
    if (handle.grid->isType<openvdb::FloatGrid>())
        function(static_cast<const openvdb::FloatGrid&>(*handle.grid));
    else if (handle.grid->isType<openvdb::DoubleGrid>())
        function(static_cast<const openvdb::DoubleGrid&>(*handle.grid));
    else throw openvdbr::Failure(OPENVDBR_UNSUPPORTED, "This operation requires a float or double scalar grid");
}
template<class Function> void visit_mutable_scalar(openvdbr_grid& handle, Function function) {
    if (handle.grid->isType<openvdb::FloatGrid>())
        function(static_cast<openvdb::FloatGrid&>(*handle.grid));
    else if (handle.grid->isType<openvdb::DoubleGrid>())
        function(static_cast<openvdb::DoubleGrid&>(*handle.grid));
    else throw openvdbr::Failure(OPENVDBR_UNSUPPORTED, "This operation requires a float or double scalar grid");
}
void validate_count(uint64_t count) {
    require(count <= std::numeric_limits<size_t>::max() / (3 * sizeof(double)), "Coordinate buffer is too large");
}
const char* version() { return OPENVDB_LIBRARY_VERSION_STRING; }
int32_t grid_create(int32_t type, double background, double voxel_size,
    const double* origin, const char* name, openvdbr_grid** out, openvdbr_error* error) {
    if (out) *out = nullptr;
    return guarded(error, [&] {
        require(out && origin, "Output and origin must not be NULL");
        openvdbr::validate_geometry(voxel_size, 1, name);
        require(std::isfinite(background), "background must be finite");
        for (int axis = 0; axis < 3; ++axis) require(std::isfinite(origin[axis]), "origin must be finite");
        openvdbr::initialize();
        openvdb::GridBase::Ptr grid;
        if (type == OPENVDBR_FLOAT) {
            require(std::abs(background) <= std::numeric_limits<float>::max(), "background exceeds float range");
            grid = openvdb::FloatGrid::create(static_cast<float>(background));
        } else if (type == OPENVDBR_DOUBLE) grid = openvdb::DoubleGrid::create(background);
        else throw openvdbr::Failure(OPENVDBR_UNSUPPORTED, "Creation supports float or double grids");
        auto transform = openvdb::math::Transform::createLinearTransform(voxel_size);
        transform->postTranslate(openvdb::Vec3d(origin[0], origin[1], origin[2]));
        grid->setTransform(transform);
        grid->setName(name);
        *out = new openvdbr_grid(std::move(grid));
    });
}
void grid_destroy(openvdbr_grid* grid) { delete grid; }
int32_t grid_clone(const openvdbr_grid* grid, openvdbr_grid** out, openvdbr_error* error) {
    if (out) *out = nullptr;
    return guarded(error, [&] {
        require(grid && out, "Grid and output must not be NULL");
        *out = new openvdbr_grid(grid->grid->deepCopyGrid());
    });
}
int32_t grid_info(const openvdbr_grid* grid, openvdbr_grid_info_v1* out, openvdbr_error* error) {
    if (out) *out = {};
    return guarded(error, [&] {
        require(grid && out, "Grid and output must not be NULL");
        out->name = grid->name.c_str(); out->value_type = grid->type.c_str();
        out->scalar_type = grid->grid->isType<openvdb::FloatGrid>() ? OPENVDBR_FLOAT :
            grid->grid->isType<openvdb::DoubleGrid>() ? OPENVDBR_DOUBLE : OPENVDBR_OTHER;
        out->grid_class = static_cast<int32_t>(grid->grid->getGridClass());
        out->active_voxels = grid->grid->activeVoxelCount();
        out->memory_bytes = grid->grid->memUsage();
        out->empty = out->active_voxels == 0;
        const auto bounds = grid->grid->evalActiveVoxelBoundingBox();
        const auto voxel_size = grid->grid->voxelSize();
        const auto origin = grid->grid->indexToWorld(openvdb::Vec3d(0));
        for (int axis = 0; axis < 3; ++axis) {
            if (!out->empty) { out->index_min[axis] = bounds.min()[axis]; out->index_max[axis] = bounds.max()[axis]; }
            out->voxel_size[axis] = voxel_size[axis]; out->origin[axis] = origin[axis];
        }
        out->background = std::numeric_limits<double>::quiet_NaN();
        if (out->scalar_type != OPENVDBR_OTHER)
            visit_scalar(*grid, [&](const auto& scalar) { out->background = scalar.background(); });
    });
}
int32_t grid_set(openvdbr_grid* grid, uint64_t count, const int32_t* xyz,
    const double* values, const uint8_t* active, openvdbr_error* error) {
    return guarded(error, [&] {
        require(grid && (count == 0 || (xyz && values)), "Grid and nonempty input buffers must not be NULL");
        validate_count(count);
        visit_mutable_scalar(*grid, [&](auto& scalar) {
            using Value = typename std::decay_t<decltype(scalar)>::ValueType;
            // Validate the entire batch before modifying the tree.
            for (uint64_t i = 0; i < count; ++i) {
                require(std::isfinite(values[i]) && std::abs(values[i]) <= std::numeric_limits<Value>::max(),
                        "Voxel values must be finite and representable by the grid type");
                require(!active || active[i] <= 1, "active must contain only zero or one");
            }
            auto accessor = scalar.getAccessor();
            for (uint64_t i = 0; i < count; ++i) {
                const openvdb::Coord coordinate(xyz[3*i], xyz[3*i+1], xyz[3*i+2]);
                if (!active || active[i]) accessor.setValueOn(coordinate, static_cast<Value>(values[i]));
                else accessor.setValueOff(coordinate, static_cast<Value>(values[i]));
            }
        });
    });
}
int32_t grid_get(const openvdbr_grid* grid, uint64_t count, const int32_t* xyz,
    double* values, uint8_t* active, openvdbr_error* error) {
    return guarded(error, [&] {
        require(grid && (count == 0 || (xyz && values)), "Grid and nonempty buffers must not be NULL");
        validate_count(count);
        visit_scalar(*grid, [&](const auto& scalar) {
            // An accessor belongs to this call, never to a shared grid handle.
            auto accessor = scalar.getConstAccessor();
            for (uint64_t i = 0; i < count; ++i) {
                const openvdb::Coord coordinate(xyz[3*i], xyz[3*i+1], xyz[3*i+2]);
                values[i] = accessor.getValue(coordinate);
                if (active) active[i] = accessor.isValueOn(coordinate);
            }
        });
    });
}
int32_t grid_sample(const openvdbr_grid* grid, uint64_t count, const double* xyz,
    int32_t world_space, int32_t interpolation, double* values, openvdbr_error* error) {
    return guarded(error, [&] {
        require(grid && (count == 0 || (xyz && values)), "Grid and nonempty buffers must not be NULL");
        validate_count(count);
        require(world_space == 0 || world_space == 1, "world_space must be zero or one");
        require(interpolation == OPENVDBR_NEAREST || interpolation == OPENVDBR_LINEAR, "Unknown interpolation mode");
        visit_scalar(*grid, [&](const auto& scalar) {
            auto accessor = scalar.getConstAccessor();
            for (uint64_t i = 0; i < count; ++i) {
                openvdb::Vec3d point(xyz[3*i], xyz[3*i+1], xyz[3*i+2]);
                for (int a = 0; a < 3; ++a) require(std::isfinite(point[a]), "Sample coordinates must be finite");
                if (world_space) point = scalar.worldToIndex(point);
                for (int a = 0; a < 3; ++a)
                    require(std::isfinite(point[a]) && point[a] >= double(INT32_MIN) + 2 && point[a] <= double(INT32_MAX) - 2,
                            "Sample coordinates exceed OpenVDB's index range");
                values[i] = interpolation == OPENVDBR_NEAREST ?
                    openvdb::tools::PointSampler::sample(accessor, point) :
                    openvdb::tools::BoxSampler::sample(accessor, point);
            }
        });
    });
}
int32_t file_open(const char* path, openvdbr_file** out, openvdbr_error* error) {
    if (out) *out = nullptr;
    return guarded(error, [&] {
        require(path && path[0] && out, "Path and output must not be empty");
        openvdbr::initialize();
        auto file = std::make_unique<openvdbr_file>(path);
        file->file.open(false);
        for (auto it = file->file.beginName(); it != file->file.endName(); ++it)
            file->names.push_back(it.gridName());
        *out = file.release();
    });
}
void file_destroy(openvdbr_file* file) { delete file; }
uint64_t file_count(const openvdbr_file* file) { return file ? file->names.size() : 0; }
int32_t file_name(const openvdbr_file* file, uint64_t index, const char** name, openvdbr_error* error) {
    if (name) *name = nullptr;
    return guarded(error, [&] {
        require(file && name, "File and name output must not be NULL");
        require(index < file->names.size(), "Grid index is outside the file");
        *name = file->names[index].c_str();
    });
}
int32_t file_read(openvdbr_file* file, uint64_t index, openvdbr_grid** out, openvdbr_error* error) {
    if (out) *out = nullptr;
    return guarded(error, [&] {
        require(file && out, "File and grid output must not be NULL");
        require(index < file->names.size(), "Grid index is outside the file");
        *out = new openvdbr_grid(file->file.readGrid(file->names[index]));
    });
}
int32_t file_write(const char* path, uint64_t count, const openvdbr_grid* const* grids, openvdbr_error* error) {
    return guarded(error, [&] {
        require(path && path[0] && count > 0 && grids, "Path and grid list must not be empty");
        validate_count(count);
        openvdb::GridCPtrVec values;
        for (uint64_t i = 0; i < count; ++i) {
            require(grids[i] != nullptr, "A grid handle is NULL");
            values.push_back(grids[i]->grid);
        }
        openvdb::io::File file(path);
        file.write(values);
    });
}
void mesh_destroy(openvdbr_mesh* mesh) { delete mesh; }
int32_t mesh_data(const openvdbr_mesh* mesh, uint64_t* vertices, const double** xyz,
    uint64_t* triangles, const int32_t** indices, openvdbr_error* error) {
    return guarded(error, [&] {
        require(mesh && vertices && xyz && triangles && indices, "Mesh and outputs must not be NULL");
        *vertices = mesh->vertices.size() / 3; *xyz = mesh->vertices.data();
        *triangles = mesh->triangles.size() / 3; *indices = mesh->triangles.data();
    });
}
}

const openvdbr_api_v1* openvdbr_provider_api() {
    static const openvdbr_api_v1 api = {
        1, sizeof(openvdbr_api_v1), version, grid_create, grid_destroy, grid_clone,
        grid_info, grid_set, grid_get, grid_sample, file_open, file_destroy,
        file_count, file_name, file_read, file_write, openvdbr::sphere, openvdbr::csg,
        openvdbr::fog, openvdbr::mesh_to_grid, openvdbr::grid_to_mesh, mesh_destroy, mesh_data,
        openvdbr_diagnostic_next
    };
    return &api;
}
