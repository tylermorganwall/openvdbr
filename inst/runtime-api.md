# openvdbr runtime C interface, version 1

## Loading and compatibility

Load the provider namespace before calling `openvdbr_api_get_v1()` from
`openvdbr/openvdbr_r.h`. That helper uses `R_GetCCallable()` on R's main thread
and verifies `abi_version` and `struct_size`. The registered name is
`openvdbr_get_api_v1`; its signature is `openvdbr_get_api_v1_fn`. Requesting an
unsupported version returns NULL. A future incompatible layout gets a new
version and getter name; fields and signatures in this version remain fixed.

`openvdbr/openvdbr_api.h` is independent of both R and OpenVDB. It can be
included in worker code and compiled as C. The table is immutable and remains
valid while the provider DLL is loaded. Consumers must keep the provider
namespace loaded for their entire lifetime; `Imports` and a NAMESPACE import
do this for normal R package use. Explicit `dyn.unload()` with live objects or
consumers is unsupported. Never serialize a function pointer or native handle.

`openvdbr::vdb_api()` returns an optional R external pointer to the same table;
`openvdbr_api_from_R_v1()` validates it. Its protected field retains the provider
namespace. It is not necessary when using the registered getter.

## Ownership

- Creation, clone, file-read and geometry calls return owned grid handles.
  Release each exactly once with `grid_destroy()` from the same table.
- `file_open()` returns an owned file handle; release it with `file_destroy()`.
  A successfully read grid is independent of that handle and source file.
- `grid_to_mesh()` returns an owned mesh; release it with `mesh_destroy()`.
  `mesh_data()` borrows immutable buffers valid until mesh destruction.
- `grid_info()` and `file_name()` borrow strings valid until the respective
  grid/file is destroyed. Copy them if they must survive that handle.
- All destroy functions accept NULL. Other dangling or foreign pointers are
  invalid; opaque handles cannot be validated after their destruction.
- `openvdbr_grid_from_R_v1()` borrows a grid from an R external pointer. Keep
  that R object reachable/protected throughout its use and do not destroy the
  borrowed pointer. Clone it for independent ownership. R adapters run only on
  R's main thread.

The provider allocates and frees its own objects; consumers never call `free`
or `delete` on them. Output handles are set to NULL before fallible operations.

## Errors and buffers

Fallible calls return an `openvdbr_status`. `OPENVDBR_OK` means success. Supply
an `openvdbr_error` for a terminated UTF-8 diagnostic of up to 511 bytes, or
NULL if no message is needed. The buffer is cleared on success. Errors and C++
exceptions stay inside the provider; workers receive a status and message,
not an R exception. Invalid inputs, I/O errors, unsupported operations,
allocation failures and other failures have distinct status codes.

The caller must provide buffers large enough for the specified counts.
Coordinates and vertices are interleaved triples `x0,y0,z0,x1,y1,z1,...`;
triangle indices are interleaved **zero-based** triples. R wrappers convert
column-major matrices and one-based mesh indices at their boundary. Voxel
coordinates are signed zero-based spatial indices, including negative values.

`grid_set()` validates the entire input batch before editing. A validation
error leaves the grid unchanged. Allocation failures during editing may leave
a partial edit. Duplicate coordinates are applied in input order. A NULL
activation array activates every supplied voxel; inactive values are retained,
not replaced by the background. `grid_get()` can omit its activation output.
For zero counts, data buffers may be NULL. Float grids reject values outside
finite float range; double grids preserve double precision.

`file_count(NULL)` returns zero. Other non-destruction operations require a
valid handle. File positions are zero-based; the R file selector is one-based.
`file_write()` can replace an existing path and is not atomic; R additionally
checks its `overwrite` argument.

## Coordinates and sampling

`grid_create()` uses uniform voxel widths and an origin giving the world
position of index `(0,0,0)`. Imported grids retain their complete transforms.
`grid_sample()` takes either continuous index coordinates (`world_space=0`) or
world coordinates (`world_space=1`); both nearest and trilinear interpolation
include stored inactive values. Each call constructs its own tree accessor,
so read-only callers do not share mutable accessor caches. Batch queries to
amortize dispatch and accessor construction.

`grid_info()` reports active bounds, not the extent of every stored inactive
value. Empty grids report zero bounds; check `empty`. Counts and native memory
estimates are unsigned 64-bit values. The scalar background is NaN for other
grid types. Names, types, transforms and metadata survive file round trips.

Geometry operations use world units. Level-set sphere and mesh conversion use
float precision and clamp distances outside the narrow band. CSG requires
float level sets with equal transforms and background widths; it deep-copies
both inputs because upstream in-place CSG consumes its second operand. Meshing
triangulates upstream quads without reversing their winding. Mesh-to-volume
expects a closed, consistently oriented mesh and does not repair bad topology.

## Threading

Resolve the table and borrow R handles on the main R thread. Provider calls in
the table do not use R's API. Concurrent read-only access to an unchanged grid
is supported with separate output/error buffers per caller. Grid mutation and
destruction require exclusive access. File handles also require exclusive
access. Operations on independent grids can run concurrently. The provider's
OpenVDB initialization is guarded with `std::call_once`.

Geometry tools may use oneTBB internally. The consumer test obtains the table
once, then makes 8,000 read-only samples across four worker threads. It links
only R and the system threading runtime, with no OpenVDB linker dependency.

## Library diagnostics

OpenVDB and Blosc diagnostic messages are buffered instead of being printed by
worker threads. `diagnostic_next()` removes one message into a caller-supplied
`openvdbr_error` and returns 1, or clears the buffer and returns 0 when empty.
`vdb_messages()` drains the same queue from R. The queue is global, protected
by a mutex, and holds the first 128 messages (511 bytes each). Overflow is
reported as a final dropped-message count. Consumers should drain it after
operations if they want to report or save library warnings. This queue does
not replace operation status codes or exception-to-error translation.

## Volume loading for renderers

`openvdbr_get_volume_api_v1`, registered with `R_RegisterCCallable()`, is a
separate versioned getter declared in `openvdbr/openvdbr_volume_api.h`. Resolve
it with `R_GetCCallable()` on the main R thread. This supplements the original
API without changing its table layout. Check `abi_version`, `struct_size` and
`nanovdb_major` against the consumer's NanoVDB headers before reading buffers.

`read_float_grid(path, name, optional, &buffer, &error)` accepts native OpenVDB
or raw/NONE/ZIP/BLOSC NanoVDB containers. Native VDB grids are converted once to
NanoVDB; affine transforms, tiles and stored inactive values survive conversion.
Only float grids are accepted. `optional=1` allows a missing grid name, returning
success with a NULL buffer. It does not suppress malformed-file errors.

`data()` borrows the aligned byte buffer and its size until `destroy()` releases
the owning handle. The bytes are immutable and can be sampled concurrently after
loading, without R calls or per-sample calls through this table. Destroy handles
only after all workers finish. Native VDB temporary trees are released after
conversion, and no grid-sized copy is required in a consumer. Container bounds,
compressed block lengths, sparse pointers and checksums are checked before the
buffer is returned. Applications still validate field-specific requirements
such as nonnegative density, zero background and grid class.
