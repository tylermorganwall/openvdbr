test_that('the version and runtime handle are available', {
  expect_identical(vdb_version(), '13.1.0')
  expect_s3_class(vdb_api(), 'openvdbr_api_v1')
})

test_that('sparse values, activation, background and transforms are independent', {
  grid = vdb_grid(background = 0.25, voxel_size = 0.5, origin = c(10, -2, 3))
  expect_true(vdb_info(grid)$empty)
  expect_equal(
    vdb_info(grid)$index_bounds,
    matrix(0L, 2, 3, dimnames = list(c('min', 'max'), c('x', 'y', 'z')))
  )
  points = rbind(c(-1, 0, 0), c(2, 3, -4))
  expect_invisible(vdb_set(grid, points, c(2, -3), c(TRUE, FALSE)))
  expect_equal(
    vdb_get(grid, points),
    list(value = c(2, -3), active = c(TRUE, FALSE))
  )
  expect_equal(vdb_info(grid)$active_voxels, 1)
  expect_equal(
    unname(vdb_info(grid)$index_bounds),
    rbind(points[1, ], points[1, ])
  )
  expect_equal(vdb_sample(grid, c(9.5, -2, 3)), 2)
  expect_equal(vdb_sample(grid, c(-1, 0, 0), space = 'index'), 2)
  expect_equal(vdb_get(grid, c(100, 0, 0)), list(value = 0.25, active = FALSE))
  expect_lt(vdb_info(grid)$memory_bytes, 2e6)
})

test_that('trilinear interpolation uses the eight voxel corners', {
  corners = as.matrix(expand.grid(x = 0:1, y = 0:1, z = 0:1))
  grid = vdb_grid(voxel_size = 2, origin = c(1, 2, 3))
  vdb_set(grid, corners, rowSums(corners))
  expect_equal(
    vdb_sample(grid, c(0.2, 0.3, 0.4), space = 'index'),
    0.9,
    tolerance = 1e-6
  )
  expect_equal(vdb_sample(grid, c(1.4, 2.6, 3.8)), 0.9, tolerance = 1e-6)
  expect_equal(
    vdb_sample(
      grid,
      c(0.2, 0.3, 0.4),
      space = 'index',
      interpolation = 'nearest'
    ),
    0
  )
  empty = matrix(numeric(), 0, 3)
  expect_equal(vdb_sample(grid, empty), numeric())
  expect_equal(
    vdb_get(grid, empty),
    list(value = numeric(), active = logical())
  )
  expect_invisible(vdb_set(grid, empty, numeric(), logical()))
})

test_that('clones own independent storage and closed handles fail safely', {
  grid = vdb_grid()
  alias = grid
  clone = vdb_clone(grid)
  vdb_set(alias, c(0, 0, 0), 5)
  expect_equal(vdb_get(grid, c(0, 0, 0))$value, 5)
  expect_equal(vdb_get(clone, c(0, 0, 0))$value, 0)
  expect_invisible(vdb_close(alias))
  expect_no_error(vdb_close(grid))
  expect_error(vdb_info(grid), 'closed')
  expect_error(vdb_info(unserialize(serialize(clone, NULL))), 'serialized')
  expect_equal(vdb_get(clone, c(0, 0, 0))$value, 0)
  expect_error(vdb_info(1), 'Expected')
  expect_error(vdb_close(vdb_api()), 'Expected')
})

test_that('invalid edits leave existing values intact', {
  grid = vdb_grid()
  vdb_set(grid, c(0, 0, 0), 7)
  expect_error(
    vdb_set(grid, rbind(c(0, 0, 0), c(1, 0, 0)), c(1, 1e100)),
    'representable'
  )
  expect_equal(vdb_get(grid, c(0, 0, 0))$value, 7)
  expect_error(vdb_set(grid, c(0.5, 0, 0), 2), 'integers')
  expect_error(vdb_set(grid, c(0, 0, 0), NA_real_), 'finite')
  expect_error(vdb_set(grid, c(0, 0, 0), 2, NA), 'logical')
  expect_error(vdb_set(grid, c(0, 0, 0), c(1, 2)), 'one per')
  expect_error(vdb_sample(grid, c(Inf, 0, 0)), 'finite')
  expect_error(vdb_sample(grid, c(1e100, 0, 0)), 'index range')
  expect_error(vdb_grid(voxel_size = 0), 'voxel_size')
  expect_error(vdb_grid(background = 1e100), 'float range')
  expect_error(vdb_grid(name = NA_character_), 'character')
  expect_error(vdb_grid(origin = matrix(1, 2, 3)), 'one coordinate')
  expect_error(vdb_grid(background = c(1, 2)), 'background')
})

test_that('double precision and duplicate-coordinate ordering are retained', {
  grid = vdb_grid(type = 'double', background = 1e50)
  value = 1 + 1e-10
  vdb_set(grid, rbind(c(0, 0, 0), c(0, 0, 0)), c(2, value))
  expect_identical(vdb_get(grid, c(0, 0, 0))$value, value)
  expect_equal(vdb_get(grid, c(-10, 0, 0))$value, 1e50)
  expect_equal(vdb_info(grid)$value_type, 'double')
})
