# openvdbr 13.1.0-2.9000

## Other

- Overall: Preserves caller warning settings and forwards R preprocessor and
  linker flags to nested builds, including quoted paths.

- Overall: Keeps compiler diagnostics enabled in bundled libraries and uses the
  portable oneTBB resumable-task backend on macOS instead of deprecated APIs.

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
