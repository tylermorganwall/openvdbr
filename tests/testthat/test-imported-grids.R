test_that('imported transforms and nonscalar grids survive file I/O', {
  file = test_path('fixtures', 'upstream.vdb')
  grid = vdb_read(file, 'rotated')
  expect_equal(vdb_sample(grid, c(3, 5, 5)), 7, tolerance = 1e-6)
  expect_equal(vdb_sample(grid, c(2, 0, 0), space = 'index'), 7)
  expect_equal(vdb_info(grid)$origin, c(3, 4, 5))
  vector = vdb_read(file, 'velocity')
  expect_equal(vdb_info(vector)$active_voxels, 1)
  expect_true(is.nan(vdb_info(vector)$background))
  expect_error(vdb_sample(vector, c(0, 0, 0)), 'float or double')
  output = tempfile(fileext = '.vdb')
  on.exit(unlink(output))
  vdb_write(list(grid, vector), output)
  expect_equal(
    vdb_sample(vdb_read(output, 'rotated'), c(3, 5, 5)),
    7,
    tolerance = 1e-6
  )
  expect_equal(
    vdb_info(vdb_read(output, 'velocity'))$value_type,
    vdb_info(vector)$value_type
  )
})
