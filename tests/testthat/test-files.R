test_that('VDB files round-trip multiple grids without losing precision or metadata', {
  file = tempfile(fileext = '.vdb')
  on.exit(unlink(file))
  a = vdb_grid(name = 'density', voxel_size = 0.25, origin = c(-3, 1, 4))
  b = vdb_grid(name = 'temperature', type = 'double', background = 273.15)
  vdb_set(a, c(-2, 3, 4), 0.5)
  vdb_set(b, c(0, 0, 0), 273.15 + 1e-10)
  expect_invisible(vdb_write(list(a, b), file))
  expect_setequal(vdb_grid_names(file), c('density', 'temperature'))
  restored = vdb_read(file, 'density')
  info = vdb_info(restored)
  expect_equal(info$name, 'density')
  expect_equal(info$voxel_size, rep(0.25, 3))
  expect_equal(info$origin, c(-3, 1, 4))
  expect_equal(vdb_get(restored, c(-2, 3, 4))$value, 0.5)
  expect_identical(
    vdb_get(vdb_read(file, 'temperature'), c(0, 0, 0))$value,
    273.15 + 1e-10
  )
  expect_equal(vdb_info(vdb_read(file))$name, vdb_grid_names(file)[1])
  expect_error(vdb_read(file, 'missing'), 'not found')
  expect_error(vdb_read(file, 3), 'not found')
  expect_error(vdb_write(a, file), 'already exists')
  expect_no_error(vdb_write(a, file, overwrite = TRUE))
  unlink(file)
  expect_equal(vdb_get(restored, c(-2, 3, 4))$value, 0.5)
})

test_that('malformed files and invalid file arguments fail safely', {
  file = tempfile(fileext = '.vdb')
  on.exit(unlink(file))
  writeLines('not a VDB file', file)
  expect_error(vdb_grid_names(file))
  expect_error(vdb_read(file))
  expect_error(vdb_write(list(), tempfile()), 'at least one')
  expect_error(vdb_write(1, tempfile()), 'native grid')
  expect_error(vdb_write(list(1), tempfile()), 'Expected')
  expect_error(vdb_write(vdb_grid(), '', overwrite = TRUE), 'empty')
  expect_error(
    vdb_write(vdb_grid(), tempfile(), overwrite = NA),
    'TRUE or FALSE'
  )
})
