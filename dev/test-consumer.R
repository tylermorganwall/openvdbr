# Run after installing openvdbr and dev/consumer into the active library.
library(openvdbr)
library(openvdbrconsumer)
stopifnot(check_runtime() == 8000L)
grid = vdb_grid()
vdb_set(grid, c(0, 0, 0), 9)
table = vdb_api()
stopifnot(check_handle(table, grid) == 9)
stopifnot(inherits(try(check_handle(grid, grid), silent = TRUE), 'try-error'))
vdb_close(grid)
stopifnot(inherits(try(check_handle(table, grid), silent = TRUE), 'try-error'))
cat(
  'Consumer C ABI, R handle adapters and 8000 worker-thread samples passed.\n'
)
