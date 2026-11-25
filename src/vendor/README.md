# Bundled source provenance

The package contains the core source needed to build offline, with upstream
license notices retained. Tests, examples, Python bindings, documentation and
unused build-system integrations are omitted where they are not needed by the
selected CMake targets.

| Component | Source | License |
| --- | --- | --- |
| OpenVDB | AcademySoftwareFoundation/openvdb, tag v13.1.0, openvdb/openvdb | Apache-2.0 |
| builtin half | Same tag, ext/imath | BSD-3-Clause |
| c-blosc | Blosc/c-blosc, tag v1.21.6 | BSD-3-Clause and codec notices |
| LZ4 / Zstd | Included by c-blosc: 1.9.4 / 1.5.6 | BSD-2-Clause / BSD-3-Clause |

Local upstream-source changes in this revision:

- `blosc/blosc/shuffle.c`: the generic ARM shuffle fallback is expected, so its
  compile-time warning is replaced by a comment. An explicit void use avoids
  an unused-variable warning when both SIMD implementations are disabled.
- `blosc/blosc/blosc.c`: remove the unused compressed-byte accumulator from
  `blosc_d()`; decompression and input bounds checks are unchanged.

Build choices live in `tools/configure.R`. The small recipe under
`tools/vendor` is staged in `src/vendor` during configuration and builds only
the vendored sources stored there. These choices
select the private OpenVDB namespace `v13_1_openvdbr`, builtin half, zlib/Blosc
compression, and eager grid loading. TBB headers and shared libraries come from
RcppParallel (>= 6.2.1), using its public compiler and linker flags. SIMD Blosc
backends are disabled for a portable baseline. No external
Imath, Boost, Python, NanoVDB or OpenVDB executables are built.

The OpenVDB and fallback Blosc archives are intermediate build products only. They are
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

Packaging corrections: CMake preserves caller diagnostics and forwards R preprocessor
and linker settings. Modified upstream files carry local change notices.

Native diagnostic corrections:

- OpenVDB and Blosc diagnostics use the provider's bounded thread-safe queue.
  OpenVDB assertion failures throw a C++ exception and invalidate subsequent
  provider operations. R errors are raised after native unwinding at the
  main-thread wrapper. RcppParallel owns the shared TBB runtime and its diagnostics.
- Provider operations default to at most two TBB threads. Set
  `OPENVDBR_NUM_THREADS` to a positive integer to request another limit.
- The optional log4cplus branch no longer suppresses Intel compiler diagnostics.
- OpenVDB file UUIDs retain upstream entropy generation: these identify files,
  do not generate statistical samples, and must not share R's RNG state.

- OpenVDB's explicit assertion handler shares the provider failure boundary.
- Blosc's Windows condition-variable initialization releases partial resources
  and returns an error; the caller checks it before creating workers. The old
  `die()` process-exit helper is removed. Unavailable AVX2 entry points use the
  existing generic shuffle implementation.

Blosc avoids ELF visibility attributes and unused POSIX state on Windows.
The package-owned CMake recipe requires 3.24; unused upstream build systems are omitted.

Blosc's Windows pthread shim uses blosc_internal_ linker names, avoiding
collisions and incompatible thread-object layouts with Rtools' winpthreads.

Blosc skips instruction-set detection when both SSE2 and AVX2 are explicitly
disabled, avoiding an irrelevant compiler-detection warning in portable builds.

The Blosc tree is a fallback. System Blosc 1.x >= 1.17.0 is preferred after
compiling and linking a probe for the context compression/decompression and
buffer-validation APIs. Headers and link flags always come from the same
selected provider. System Blosc retains its own diagnostic behavior.

Configuration and dependency selection:

- RcppParallel supplies oneTBB. No private TBB sources or runtime are bundled.
- CRAN's blosc package is an R interface to a system library, not a native
  header/library provider. Configuration probes the system Blosc 1.x API with
  R's compiler, preferring pkg-config and falling back to the bundled sources.
- R compiles provider.cpp, provider_tools.cpp, volume_io.cpp, diagnostics.cpp
  and r_api.cpp. CMake builds only OpenVDB/builtin half and fallback Blosc.
- Unused Blosc build/development files, Zstd legacy/deprecated/DLL-example
  sources, disabled SIMD/assembly implementations, and NanoVDB CUDA/Python
  headers are omitted. Required codec headers, sources and notices are retained.
