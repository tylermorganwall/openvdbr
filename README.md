# openvdbr

`openvdbr` bundles OpenVDB **13.1.0** and exposes sparse volumes to R and to
compiled R packages through a versioned runtime C interface. It uses full
OpenVDB trees: grids can be edited, combined as level sets, converted to and
from triangle meshes, and saved in `.vdb` files.

Package version `13.1.0-2` combines the upstream version with this package's
revision number.

## Install

```r
install.packages('/path/to/openvdbr', repos = NULL, type = 'source')
```

A source build needs R's compiler toolchain, GNU make, CMake >= 3.24, and zlib.
OpenVDB, oneTBB and Blosc sources are included; the build does not download
anything or require a system OpenVDB installation. The default nested build
uses two compiler processes. Set `CMAKE_BUILD_PARALLEL_LEVEL` to change it.
OpenVDB's templates make the first build substantial.

oneTBB is built as a package-specific shared library and installed beside the
provider DLL. OpenVDB and Blosc are compiled into that provider only. Consumers
do not embed another copy of those libraries.

## R example

```r
library(openvdbr)

# Make a hollow moon, then extract its surface.
outer = vdb_sphere(radius = 1, voxel_size = 0.08)
cutout = vdb_sphere(radius = 0.9, center = c(0.4, 0.15, 0), voxel_size = 0.08)
moon = vdb_csg(outer, cutout, 'difference')
mesh = vdb_to_mesh(moon, adaptivity = 0.1)

# Sample a slice without creating a dense 3D array.
axis = seq(-1.3, 1.3, length.out = 100)
xyz = as.matrix(expand.grid(x = axis, y = axis, z = 0))
section = matrix(vdb_sample(moon, xyz), length(axis))
contour(axis, axis, section, levels = 0, asp = 1, drawlabels = FALSE)

file = tempfile(fileext = '.vdb')
vdb_write(moon, file)
restored = vdb_read(file)
vdb_info(restored)
unlink(file)
```

`vdb_grid()`, `vdb_set()` and `vdb_get()` work with exact voxel values and
activation flags. `vdb_sample()` supports nearest or trilinear interpolation in
world or index coordinates. `vdb_fog()` turns a level set into a density field.

Grid handles have reference semantics. `other = grid` shares storage;
`vdb_clone(grid)` makes an independent copy. CSG and conversion operations
return new grids, while `vdb_set()` edits its input. Use `vdb_close()` to release
large grids promptly, or let R's finalizer release them. Persist grids with
`vdb_write()`, not `saveRDS()`.

## Runtime interface for other packages

Consumer `DESCRIPTION`:

```
Imports: openvdbr
LinkingTo: openvdbr
```

Consumer `NAMESPACE` should import an R symbol, for example
`importFrom(openvdbr, vdb_version)`, so the provider is loaded. No OpenVDB,
Blosc or TBB linker flags are needed.

```cpp
#include <openvdbr/openvdbr_r.h>

// Run lookup on R's main thread after loading the provider.
const openvdbr_api_v1* api = openvdbr_api_get_v1();
```

The helper retrieves the table using:

```cpp
openvdbr_get_api_v1_fn getter = (openvdbr_get_api_v1_fn)
    R_GetCCallable("openvdbr", "openvdbr_get_api_v1");
const openvdbr_api_v1* api = getter(1);
```

The provider registers the getter with `R_RegisterCCallable()`. The ABI uses
opaque pointers, fixed-width integers and caller-owned error buffers. No
OpenVDB C++ types cross the package boundary. It works from C as well as C++.

See [the runtime contract](inst/runtime-api.md) for ownership, sampling,
threading and error handling. [dev/consumer](dev/consumer) is a complete small
consumer package, including a C compilation check and concurrent sampling test.

## Current scope

- Read, inspect, clone and write OpenVDB's registered grid types, preserving
  metadata and transforms. The scalar editing/sampling/meshing interface
  supports float and double grids; vector and point grids are passthrough only.
- Sphere, CSG, mesh-to-level-set and fog operations use float level sets.
- File listing reads metadata; reading a selected grid loads its voxel data
  fully. Delayed, memory-mapped loading is disabled, so loaded grids do not
  depend on the original file remaining available.
- This first interface does not wrap OpenVDB's complete tool collection,
  custom grid registration or GPU operations. A separate native volume API
  reads/converts float grids to NanoVDB for renderer integrations.

Bundled licenses and source provenance are recorded in
[inst/COPYRIGHTS](inst/COPYRIGHTS) and [src/vendor/README.md](src/vendor/README.md).

Library warnings are available from `vdb_messages()`. The provider buffers them
safely when they originate on worker threads; operation failures still return
native error statuses or raise R errors.
