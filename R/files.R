#' List or read grids in a VDB file
#'
#' VDB files can contain several named sparse volumes, each with its own value
#' type, metadata and spatial transform. Inspect their names before selecting a
#' density, temperature or signed-distance grid.
#' @param filename Path to a VDB file.
#' @param grid Default `1`. Grid name or one-based position in [vdb_grid_names()].
#' @return `vdb_grid_names()` returns character names without loading voxel data.
#'   `vdb_read()` returns a native grid handle. Its selected grid is fully loaded
#'   and independent of the source file, which can subsequently be moved or removed.
#' @details The bundled OpenVDB implementation includes Blosc and zlib compression.
#'   All upstream registered grid types can be read and written without conversion.
#'   Editing, scalar sampling and meshing currently support float and double
#'   scalar grids; vector and point grids can be inspected and passed through.
#' @examples
#' file = tempfile(fileext = '.vdb')
#' shape = vdb_sphere(radius = 1, voxel_size = 0.2, name = 'surface')
#' density = vdb_fog(shape)
#' vdb_write(shape, file)
#' vdb_grid_names(file)
#' restored = vdb_read(file, 'surface')
#' vdb_sample(restored, c(0, 0, 0))
#' unlink(file)
#' @export
vdb_read = function(filename, grid = 1) {
  filename = normalizePath(
    path.expand(vdb_string(filename, 'filename')),
    mustWork = TRUE
  )
  if (is.character(grid)) {
    grid = vdb_string(grid, 'grid')
  } else {
    grid = vdb_number(grid, 'grid', minimum = 1)
    if (grid != trunc(grid)) {
      stop('grid must be an integer position or grid name.')
    }
  }
  .Call(C_vdb_read, filename, grid)
}

#' @rdname vdb_read
#' @export
vdb_grid_names = function(filename) {
  filename = normalizePath(
    path.expand(vdb_string(filename, 'filename')),
    mustWork = TRUE
  )
  .Call(C_vdb_names, filename)
}

#' Write sparse grids to a VDB file
#' @param grids A native grid or a list of native grids. Each grid retains its
#'   own name, value type, metadata and transform.
#' @param filename Output path, including a `.vdb` extension if desired.
#' @param overwrite Default `FALSE`. Allow replacing an existing file.
#' @return The output filename, invisibly.
#' @export
vdb_write = function(grids, filename, overwrite = FALSE) {
  filename = path.expand(vdb_string(filename, 'filename'))
  if (!nzchar(filename)) {
    stop('filename must not be empty.')
  }
  if (!is.logical(overwrite) || length(overwrite) != 1L || is.na(overwrite)) {
    stop('overwrite must be TRUE or FALSE.')
  }
  if (file.exists(filename) && !overwrite) {
    stop('File already exists; use overwrite = TRUE to replace it.')
  }
  if (inherits(grids, 'openvdbr_grid')) {
    grids = list(grids)
  }
  if (!is.list(grids) || length(grids) == 0L) {
    stop('grids must contain at least one native grid.')
  }
  .Call(C_vdb_write, grids, filename)
  invisible(filename)
}
