# Bundled source provenance

The package contains the core source needed to build offline, with upstream
license notices retained. Tests, examples, Python bindings, documentation and
unused build-system integrations are omitted where they are not needed by the
selected CMake targets.

| Component | Source | License |
| --- | --- | --- |
| OpenVDB | AcademySoftwareFoundation/openvdb, tag v13.1.0, openvdb/openvdb | Apache-2.0 |
| builtin half | Same tag, ext/imath | BSD-3-Clause |
| oneTBB | uxlfoundation/oneTBB, tag v2023.1.0 | Apache-2.0 and retained notices |
| c-blosc | Blosc/c-blosc, tag v1.21.6 | BSD-3-Clause and codec notices |
| LZ4 / Zstd | Included by c-blosc: 1.9.4 / 1.5.6 | BSD-2-Clause / BSD-3-Clause |

Local upstream-source changes in this revision:

- `blosc/blosc/shuffle.c`: the generic ARM shuffle fallback is expected, so its
  compile-time warning is replaced by a comment. An explicit void use avoids
  an unused-variable warning when both SIMD implementations are disabled.
- `blosc/blosc/blosc.c`: remove the unused compressed-byte accumulator from
  `blosc_d()`; decompression and input bounds checks are unchanged.

Build choices live in `src/backend/CMakeLists.txt`, not patched OpenVDB headers. They
select the private OpenVDB namespace `v13_1_openvdbr`, a package-specific oneTBB
shared-library filename, builtin half, zlib/Blosc compression, and eager grid
loading. SIMD Blosc backends are disabled for a portable baseline. No external
Imath, Boost, Python, NanoVDB or OpenVDB executables are built.

The core and Blosc archives are intermediate build products only. They are
linked into the provider DLL; consumers receive the C ABI headers and use
R's registered callable table. No vendored C++ headers are installed.

R embedding adjustments:

- OpenVDB's fallback warning/error logging and Blosc diagnostics go to a bounded,
  thread-safe provider queue, exposed through `vdb_messages()` and the C table.
  No R API is called from worker threads. Blosc's version formatting uses
  `snprintf` with the actual buffer length.
- `openvdb/io/Archive.cc` attaches its stream-state registry to a private string
  stream instead of `std::cout`. C++ streams never cross this provider's C ABI,
  so state sharing with a different OpenVDB DLL is neither needed nor desirable.
  The file serialization format and per-stream metadata slots are unchanged.
- The provider DLL exports only its R initializer; codec and C++ symbols stay
  private. Native functions remain accessible through the registered table.

Revision 13.1.0-2 also bundles NanoVDB headers from the same OpenVDB v13.1.0
source tree (NanoVDB file ABI 32, API 32.9.2). Only CPU float conversion and
validation are compiled. GridStats' inactive-root diagnostic is routed to the
provider queue. The volume reader uses bounded container/chunk reads, zlib and
Blosc decoding, followed by sparse-pointer/checksum validation; no external
conversion executable or dense grid expansion is needed.

R package check adjustments: warning-suppression pragmas are removed from
the bundled headers and sources so compiler diagnostics remain enabled.
NanoVDB explicit metadata padding is marked maybe_unused without changing ABI.
On macOS, oneTBB uses its upstream thread-based resumable-task backend
instead of the deprecated ucontext API; compiler warnings remain enabled.

Packaging corrections: CMake preserves caller diagnostics and forwards R preprocessor
and linker settings. Modified upstream files carry local change notices.
