test_that("runtime thread settings are validated without damaging grids", {
  previous = Sys.getenv("OPENVDBR_NUM_THREADS", unset = NA_character_)
  on.exit(
    if (is.na(previous)) {
      Sys.unsetenv("OPENVDBR_NUM_THREADS")
    } else {
      Sys.setenv(OPENVDBR_NUM_THREADS = previous)
    }
  )
  for (value in c("0", "-1", "abc", "2x", "99999999999999999999")) {
    Sys.setenv(OPENVDBR_NUM_THREADS = value)
    expect_error(vdb_grid(), "positive integer")
  }
  Sys.setenv(OPENVDBR_NUM_THREADS = "1")
  grid = vdb_grid()
  expect_true(vdb_info(grid)$empty)
  vdb_close(grid)
})

test_that("the provider loads and runs with RcppParallel in a fresh R session", {
  script = tempfile(fileext = ".R")
  error_file = tempfile()
  previous = Sys.getenv("R_TESTS", unset = NA_character_)
  on.exit({
    unlink(c(script, error_file))
    if (is.na(previous)) {
      Sys.unsetenv("R_TESTS")
    } else {
      Sys.setenv(R_TESTS = previous)
    }
  })
  # Inherit this setting: system2(env = ...) is not portable to Rscript on
  # Windows. A script file also avoids quoting multiline R code on its command line.
  Sys.setenv(R_TESTS = "")
  writeLines(
    c(
      paste0(".libPaths(", paste(deparse(.libPaths()), collapse = ""), ")"),
      "library(openvdbr)",
      "grid = vdb_sphere()",
      "stopifnot('RcppParallel' %in% loadedNamespaces())",
      "paths = vapply(getLoadedDLLs(), function(x) x[['path']], character(1))",
      "stopifnot(normalizePath(RcppParallel::tbbLibraryPath('tbb')) %in% normalizePath(paths, mustWork = FALSE))",
      "stopifnot(!vdb_info(grid)$empty)",
      "vdb_close(grid)",
      "cat('alive\\n')"
    ),
    script
  )
  executable = file.path(
    R.home("bin"),
    if (.Platform$OS.type == "windows") "Rscript.exe" else "Rscript"
  )
  result = system2(
    executable,
    c("--vanilla", shQuote(script)),
    stdout = TRUE,
    stderr = error_file
  )
  errors = readLines(error_file, warn = FALSE)
  expect_null(attr(result, "status"), info = paste(errors, collapse = "\n"))
  expect_identical(result, "alive")
  expect_identical(file.info(error_file)$size, 0)
})
