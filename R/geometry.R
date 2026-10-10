#' Create a sphere as a signed distance field
#'
#' A level set stores distance to a surface in a narrow band. Values are negative
#' inside and positive outside, with distant values clamped to the band width.
#' This supports smooth surface operations without allocating a dense volume.
#' @param radius Default `1`. Positive sphere radius in world units.
#' @param center Default `c(0, 0, 0)`. Sphere center in world coordinates.
#' @param voxel_size Default `0.1`. Positive voxel width in world units.
#' @param half_width Default `3`. Narrow-band half width in voxels, at least one.
#' @param name Default `"surface"`. Name of the resulting grid.
#' @return A float-valued level-set grid.
#' @examples
#' # A snowman silhouette made from two sparse level sets.
#' body = vdb_sphere(1, voxel_size = 0.15)
#' head = vdb_sphere(0.6, center = c(0, 1.2, 0), voxel_size = 0.15)
#' snowman = vdb_csg(body, head)
#' axis = seq(-1.5, 2, length.out = 60)
#' points = as.matrix(expand.grid(x = axis, y = axis, z = 0))
#' section = matrix(vdb_sample(snowman, points), length(axis))
#' graphics::contour(axis, axis, section, levels = 0, asp = 1, drawlabels = FALSE)
#' @export
vdb_sphere = function(
  radius = 1,
  center = c(0, 0, 0),
  voxel_size = 0.1,
  half_width = 3,
  name = 'surface'
) {
  radius = vdb_number(radius, 'radius', minimum = 0, strict = TRUE)
  center = vdb_xyz(center)
  if (nrow(center) != 1L) {
    stop('center must contain one coordinate triple.')
  }
  voxel_size = vdb_number(voxel_size, 'voxel_size', minimum = 0, strict = TRUE)
  half_width = vdb_number(half_width, 'half_width', minimum = 1)
  .Call(
    C_vdb_sphere,
    radius,
    as.double(center),
    voxel_size,
    half_width,
    vdb_string(name, 'name')
  )
}

#' Combine level-set surfaces
#' @param a First float-valued level-set grid.
#' @param b Second float-valued level-set grid with the same transform and band width.
#' @param operation Default `"union"`. One of `"union"`, `"intersection"` or
#'   `"difference"` (subtract `b` from `a`).
#' @return A new level-set grid. Both inputs remain unchanged.
#' @details These are OpenVDB's level-set CSG operations. They combine signed
#'   distances; they do not resample mismatched grids or rebuild distance bands.
#' @export
vdb_csg = function(a, b, operation = c('union', 'intersection', 'difference')) {
  operation = match.arg(operation)
  .Call(
    C_vdb_csg,
    a,
    b,
    match(operation, c('union', 'intersection', 'difference')) - 1L
  )
}

#' Convert a signed distance field into a fog volume
#' @param grid A float-valued level-set grid.
#' @return A new fog-volume grid with density zero outside and density approaching
#'   one inside. The transition spans the input's interior narrow band.
#' @details This produces a bounded density field suitable for volumetric
#'   rendering. It retains the source transform and name and leaves the input intact.
#' @export
vdb_fog = function(grid) .Call(C_vdb_fog, grid)

#' Convert a triangle mesh into a sparse level set
#' @param vertices Numeric matrix with three columns of world-space vertex coordinates.
#' @param triangles Integer matrix with three columns of one-based vertex indices.
#' @param voxel_size Default `0.1`. Positive voxel width in world units.
#' @param half_width Default `3`. Narrow-band half width in voxels, at least one.
#' @param name Default `"surface"`. Name of the resulting grid.
#' @return A float-valued level-set grid.
#' @details Use a closed, consistently oriented surface for meaningful interior
#'   signs. OpenVDB accepts world-space vertices at float precision; this function
#'   does not repair nonmanifold or self-intersecting geometry.
#' @examples
#' vertices = rbind(c(-1, -1, -1), c(1, -1, -1), c(0, 1, -1), c(0, 0, 1))
#' triangles = rbind(c(1, 3, 2), c(1, 2, 4), c(2, 3, 4), c(3, 1, 4))
#' pyramid = vdb_from_mesh(vertices, triangles, voxel_size = 0.2)
#' vdb_sample(pyramid, c(0, 0, 0))
#' @export
vdb_from_mesh = function(
  vertices,
  triangles,
  voxel_size = 0.1,
  half_width = 3,
  name = 'surface'
) {
  vertices = vdb_xyz(vertices)
  triangles = vdb_xyz(triangles, integer = TRUE)
  if (
    nrow(vertices) < 3L ||
      nrow(triangles) == 0L ||
      any(triangles < 1L | triangles > nrow(vertices))
  ) {
    stop('triangles must index a nonempty mesh using one-based vertex indices.')
  }
  voxel_size = vdb_number(voxel_size, 'voxel_size', minimum = 0, strict = TRUE)
  half_width = vdb_number(half_width, 'half_width', minimum = 1)
  .Call(
    C_vdb_from_mesh,
    vertices,
    triangles - 1L,
    voxel_size,
    half_width,
    vdb_string(name, 'name')
  )
}

#' Extract a triangle mesh from a scalar volume
#' @param grid A float or double scalar grid.
#' @param isovalue Default `0`. Surface value; zero for a level set, often `0.5` for fog.
#' @param adaptivity Default `0`. Surface simplification between zero and one.
#' @return A list of `vertices` (three-column world-space numeric matrix) and
#'   `triangles` (three-column, one-based integer indices). OpenVDB quads are
#'   triangulated with their original winding preserved.
#' @examples
#' surface = vdb_to_mesh(vdb_sphere(voxel_size = 0.2), adaptivity = 0.2)
#' graphics::plot(surface$vertices[, c(1, 2)], asp = 1, pch = '.',
#'                xlab = 'x', ylab = 'y')
#' @export
vdb_to_mesh = function(grid, isovalue = 0, adaptivity = 0) {
  .Call(
    C_vdb_to_mesh,
    grid,
    vdb_number(isovalue, 'isovalue'),
    vdb_number(adaptivity, 'adaptivity', minimum = 0, maximum = 1)
  )
}
