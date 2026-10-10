test_that('level sets have world-space distances and fog has bounded density', {
  sphere = vdb_sphere(radius = 1, voxel_size = 0.1)
  expect_equal(vdb_info(sphere)$grid_class, 'level_set')
  expect_equal(vdb_info(sphere)$background, 0.3, tolerance = 1e-6)
  expect_equal(vdb_sample(sphere, c(1.2, 0, 0)), 0.2, tolerance = 1e-5)
  expect_lt(vdb_sample(sphere, c(0, 0, 0)), 0)
  fog = vdb_fog(sphere)
  expect_equal(vdb_info(fog)$grid_class, 'fog_volume')
  expect_equal(vdb_sample(fog, c(0, 0, 0)), 1)
  expect_equal(vdb_sample(fog, c(2, 0, 0)), 0)
  expect_equal(vdb_info(sphere)$grid_class, 'level_set')
  expect_error(vdb_fog(vdb_grid()), 'level set')
  expect_error(vdb_sphere(radius = -1), 'radius')
  expect_error(vdb_sphere(half_width = 0.5), 'half_width')
  expect_error(vdb_sphere(voxel_size = 1e-20), 'index range')
  expect_error(vdb_sphere(radius = 1e-300, voxel_size = 1e-300), 'voxel_size')
  expect_error(
    vdb_sphere(center = c(1e40, 0, 0), voxel_size = 1e35),
    'index range'
  )
})

test_that('CSG preserves both operands and validates grid compatibility', {
  a = vdb_sphere(voxel_size = 0.15)
  b = vdb_sphere(center = c(0.8, 0, 0), voxel_size = 0.15)
  before = c(vdb_sample(a, c(1.4, 0, 0)), vdb_sample(b, c(0, 0, 0)))
  expect_lt(vdb_sample(vdb_csg(a, b), c(1.4, 0, 0)), 0)
  expect_gt(vdb_sample(vdb_csg(a, b, 'intersection'), c(1.4, 0, 0)), 0)
  expect_gt(vdb_sample(vdb_csg(a, b, 'difference'), c(0, 0, 0)), 0)
  expect_equal(
    c(vdb_sample(a, c(1.4, 0, 0)), vdb_sample(b, c(0, 0, 0))),
    before
  )
  expect_error(vdb_csg(a, vdb_sphere(voxel_size = 0.2)), 'transforms')
  expect_error(
    vdb_csg(a, vdb_sphere(voxel_size = 0.15, half_width = 4)),
    'band widths'
  )
  expect_error(vdb_csg(a, vdb_grid()), 'level sets')
})

test_that('meshing produces a closed sphere with a consistent world-space scale', {
  sphere = vdb_sphere(radius = 2, center = c(3, 0, 0), voxel_size = 0.2)
  mesh = vdb_to_mesh(sphere)
  expect_gt(nrow(mesh$vertices), 100)
  expect_gt(nrow(mesh$triangles), 100)
  expect_true(all(mesh$triangles >= 1L & mesh$triangles <= nrow(mesh$vertices)))
  radii = sqrt(rowSums(sweep(mesh$vertices, 2, c(3, 0, 0))^2))
  expect_lt(max(abs(radii - 2)), 0.25)
  edges = rbind(
    mesh$triangles[, c(1, 2)],
    mesh$triangles[, c(2, 3)],
    mesh$triangles[, c(3, 1)]
  )
  edge_keys = apply(edges, 1, function(edge) paste(sort(edge), collapse = ':'))
  expect_true(all(table(edge_keys) == 2L))
  restored = vdb_from_mesh(mesh$vertices, mesh$triangles, voxel_size = 0.2)
  expect_lt(vdb_sample(restored, c(3, 0, 0)), 0)
  expect_gt(vdb_sample(restored, c(6, 0, 0)), 0)
  expect_error(vdb_from_mesh(mesh$vertices, matrix(0, 1, 3)), 'one-based')
  expect_error(vdb_to_mesh(sphere, adaptivity = 2), 'adaptivity')
})
