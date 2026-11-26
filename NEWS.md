# openvdbr 13.1.0-6

## Bugfixes

- Overall: Builds with C23 compilers when using bundled Blosc.

# openvdbr 13.1.0-5

- Fixed Windows installation from a source tarball by shipping the DLL export
  file under the name R's linker expects.
- Fixed the Linux configure check for oneTBB when RcppParallel supplies the
  library at runtime.

# openvdbr 13.1.0-4

## Other

- Overall: Centralizes native dependency and compiler configuration in
  `tools/configure.R`. R compiles and links the package interface; CMake builds
  only the required library sources under `src/vendor`, using R's architecture and full
  compiler commands. Unused upstream build tooling,
  Python/CUDA bindings, examples and legacy codec sources are omitted.

- Overall: Prefers a compatible system Blosc and falls back to the bundled
  library when the required headers or functions cannot be compiled and linked.

# openvdbr 13.1.0-3

## Bugfixes

- Overall: Uses RcppParallel (>= 6.2.1) for the shared oneTBB runtime instead
  of building and installing a private copy.

- Overall: Uses R's standard native-library installation, preserving its automatic
  object-symbol checks without a custom installer.

- Overall: Keeps Blosc's Windows thread functions separate from Rtools' pthread
  runtime to prevent duplicate-symbol linker errors.

- Overall: Supports Windows builds with Rtools headers missing a loader constant
  and corrects bundled-library portability diagnostics.

- Overall: Buffers OpenVDB and Blosc diagnostics without writing to the process
  console and contains their invariant failures at the provider boundary.
- Overall: Limits runtime parallelism to two threads by default; respects an
  explicit `OPENVDBR_NUM_THREADS` setting.

## Other

- Overall: Preserves caller warning settings and forwards R preprocessor and
  linker flags to nested builds, including quoted paths.

- Overall: Keeps compiler diagnostics enabled in bundled libraries.

# openvdbr 13.1.0-2

## New features

- Overall: Adds a registered volume-loading C interface for native OpenVDB and
  raw, uncompressed, ZIP or BLOSC NanoVDB float grids. The provider returns
  immutable aligned sparse buffers for direct sampling by renderers, preserving
  affine transforms and stored inactive values without making dense R arrays.

## Other

- Overall: Validates container sizes, compressed blocks, sparse-node offsets and
  checksums before exposing loaded grids; missing optional grids are explicit.

# openvdbr 13.1.0-1

## New features

- Provides sparse grids, VDB file I/O, interpolation, level sets, fog volumes,
  constructive solid geometry and mesh conversion using OpenVDB 13.1.0.
- Exposes an opaque, versioned C runtime interface for other R packages.

## Other

- Bundles OpenVDB, oneTBB and compression libraries for offline source builds.
- Buffers library diagnostics for retrieval with `vdb_messages()`, including
  messages generated on native worker threads.
