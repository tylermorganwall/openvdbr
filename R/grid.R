#' Create a sparse scalar volume
#'
#' Store values only where they differ from a shared background. This is useful
#' for smoke density, temperature fields, signed distances and other volumes
#' whose bounding box would waste memory as a dense R array.
#' @param background Default `0`. Value returned outside stored voxels.
#' @param voxel_size Default `1`. Positive world-space width of a voxel.
#' @param origin Default `c(0, 0, 0)`. World-space position of index `(0, 0, 0)`.
#' @param name Default `"density"`. Grid name used in VDB files.
#' @param type Default `"float"`. Storage precision: `"float"` or `"double"`.
#' @return An `openvdbr_grid` external pointer, released automatically by garbage
#'   collection or explicitly by [vdb_close()].
#' @details Voxel indices are zero-based spatial coordinates and can be negative.
#'   Handles have reference semantics: assigning a handle to another variable
#'   shares the same grid. Use [vdb_clone()] for an independent copy. Handles
#'   cannot be saved with `saveRDS()`; use [vdb_write()] to persist the data.
#' @examples
#' cloud = vdb_grid(voxel_size = 0.5)
#' vdb_set(cloud, rbind(c(-1, 0, 0), c(0, 0, 0), c(1, 0, 0)), c(0.2, 1, 0.2))
#' vdb_sample(cloud, c(0.25, 0, 0))
#' vdb_info(cloud)
#' @export
vdb_grid = function(
  background = 0,
  voxel_size = 1,
  origin = c(0, 0, 0),
  name = 'density',
  type = c('float', 'double')
) {
  background = vdb_number(background, 'background')
  voxel_size = vdb_number(voxel_size, 'voxel_size', minimum = 0, strict = TRUE)
  origin = vdb_xyz(origin)
  if (nrow(origin) != 1L) {
    stop('origin must contain one coordinate triple.')
  }
  name = vdb_string(name, 'name')
  type = match.arg(type)
  .Call(
    C_vdb_grid,
    match(type, c('float', 'double')),
    background,
    voxel_size,
    as.double(origin),
    name
  )
}

#' Inspect a sparse grid
#' @param grid An `openvdbr_grid` handle.
#' @return A list with the grid name, value type, class, active voxel count,
#'   estimated native memory in bytes, inclusive index bounds, voxel widths,
#'   world-space origin and scalar background. Empty grids have zero bounds;
#'   check `empty` before using them. Counts are doubles to support large grids.
#'   Non-scalar backgrounds are reported as `NaN`.
#' @export
vdb_info = function(grid) .Call(C_vdb_info, grid)

#' Copy or release a native grid
#' @param grid An `openvdbr_grid` handle.
#' @return `vdb_clone()` returns an independent deep copy. `vdb_close()` releases
#'   the native storage and invisibly returns `NULL`; repeated closes are safe.
#' @details Closing a handle also closes every R alias of that handle. The handle
#'   remains identifiable, but further operations report that it is closed.
#' @examples
#' original = vdb_grid()
#' copy = vdb_clone(original)
#' vdb_set(copy, c(0, 0, 0), 1)
#' vdb_get(original, c(0, 0, 0))
#' vdb_close(copy)
#' @export
vdb_clone = function(grid) .Call(C_vdb_clone, grid)

#' @rdname vdb_clone
#' @export
vdb_close = function(grid) invisible(.Call(C_vdb_close, grid))

#' Edit sparse voxel values
#' @param grid An `openvdbr_grid` handle with float or double values.
#' @param xyz A numeric vector of length three or a matrix with three columns.
#'   Each row is a zero-based integer coordinate `(x, y, z)` in index space.
#' @param values Finite voxel values, one per coordinate or a single value to repeat.
#' @param active Default `TRUE`. Logical activation flags, one per coordinate
#'   or a single flag to repeat.
#' @return The input handle, invisibly, after modifying it in place.
#' @details Inactive voxels can retain values different from the background.
#'   Level sets use this to represent their negative interiors compactly.
#'   Duplicate coordinates are applied in input order, so the last value wins.
#'   Validation occurs before editing; allocation failures may leave a partial edit.
#' @export
vdb_set = function(grid, xyz, values, active = TRUE) {
  xyz = vdb_xyz(xyz, integer = TRUE)
  count = nrow(xyz)
  if (
    !is.numeric(values) ||
      any(!is.finite(values)) ||
      !(length(values) %in% c(1L, count))
  ) {
    stop('values must contain one finite number or one per coordinate.')
  }
  if (
    !is.logical(active) || anyNA(active) || !(length(active) %in% c(1L, count))
  ) {
    stop('active must contain one nonmissing logical or one per coordinate.')
  }
  invisible(.Call(
    C_vdb_set,
    grid,
    xyz,
    rep(as.double(values), length.out = count),
    rep(active, length.out = count)
  ))
}

#' Read exact voxel values and activation flags
#' @param grid An `openvdbr_grid` handle with float or double values.
#' @param xyz A coordinate triple or three-column matrix of zero-based integer
#'   voxel indices. Negative indices are allowed.
#' @return A list containing numeric `value` and logical `active` vectors.
#' @export
vdb_get = function(grid, xyz) {
  .Call(C_vdb_get, grid, vdb_xyz(xyz, integer = TRUE))
}

#' Sample a sparse volume at arbitrary positions
#' @param grid An `openvdbr_grid` handle with float or double values.
#' @param xyz A coordinate triple or three-column numeric matrix.
#' @param space Default `"world"`. Use `"world"` coordinates transformed by the
#'   grid, or continuous `"index"` coordinates.
#' @param interpolation Default `"linear"`. Trilinear interpolation or `"nearest"`.
#' @return A numeric vector with one sampled value per row.
#' @details Sampling includes stored inactive values, which are significant for
#'   signed distance fields. Trilinear interpolation uses eight neighboring voxels.
#'   The complete transform of an imported grid is respected, including rotations.
#' @export
vdb_sample = function(
  grid,
  xyz,
  space = c('world', 'index'),
  interpolation = c('linear', 'nearest')
) {
  space = match.arg(space)
  interpolation = match.arg(interpolation)
  .Call(
    C_vdb_sample,
    grid,
    vdb_xyz(xyz),
    as.integer(space == 'world'),
    as.integer(interpolation == 'linear')
  )
}

#' @export
print.openvdbr_grid = function(x, ...) {
  info = vdb_info(x)
  cat(
    '<openvdbr_grid> ',
    info$name,
    ' (',
    info$value_type,
    ', ',
    info$grid_class,
    ')\n',
    '  ',
    format(info$active_voxels, scientific = FALSE),
    ' active voxels; ',
    format(round(info$memory_bytes / 1024^2, 3)),
    ' MiB native storage\n',
    sep = ''
  )
  invisible(x)
}

#' @param value Caller-supplied numeric scalar.
#' @param name Argument name used in errors.
#' @param minimum Default `-Inf`. Lower limit.
#' @param maximum Default `Inf`. Upper limit.
#' @param strict Default `FALSE`. Whether the lower limit is exclusive.
#' @return A finite double scalar within the specified bounds.
#' @keywords internal
#' @noRd
vdb_number = function(
  value,
  name,
  minimum = -Inf,
  maximum = Inf,
  strict = FALSE
) {
  if (
    !is.numeric(value) ||
      length(value) != 1L ||
      !is.finite(value) ||
      value < minimum ||
      value > maximum ||
      (strict && value == minimum)
  ) {
    stop(name, ' must be a finite number in the supported range.')
  }
  as.double(value)
}

#' @param value Caller-supplied coordinate vector or matrix.
#' @param integer Default `FALSE`. Require representable integer indices.
#' @return A numeric or integer matrix with three columns.
#' @keywords internal
#' @noRd
vdb_xyz = function(value, integer = FALSE) {
  if (is.numeric(value) && is.null(dim(value)) && length(value) == 3L) {
    value = matrix(value, nrow = 1L)
  }
  if (
    !is.matrix(value) ||
      !is.numeric(value) ||
      ncol(value) != 3L ||
      any(!is.finite(value))
  ) {
    stop(
      'Coordinates must be a finite numeric triple or a matrix with three columns.'
    )
  }
  if (
    integer && any(value != trunc(value) | abs(value) > .Machine$integer.max)
  ) {
    stop('Voxel and triangle indices must be representable R integers.')
  }
  storage.mode(value) = if (integer) 'integer' else 'double'
  value
}

#' @param value Caller-supplied string.
#' @param name Argument name used in errors.
#' @return One nonmissing UTF-8 string.
#' @keywords internal
#' @noRd
vdb_string = function(value, name) {
  if (!is.character(value) || length(value) != 1L || is.na(value)) {
    stop(name, ' must be one nonmissing character string.')
  }
  enc2utf8(value)
}
