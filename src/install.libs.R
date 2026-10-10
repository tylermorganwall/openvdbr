destination = file.path(
  Sys.getenv('R_PACKAGE_DIR'),
  paste0('libs', Sys.getenv('R_ARCH'))
)
dir.create(destination, recursive = TRUE, showWarnings = FALSE)
extension = .Platform$dynlib.ext
provider = paste0('openvdbr', extension)
runtime = list.files(
  'build/runtime',
  pattern = '\\.(so|dylib|dll)$',
  full.names = TRUE
)
if (!file.exists(provider) || length(runtime) == 0L) {
  stop('Missing provider or oneTBB runtime.')
}
if (!all(file.copy(c(provider, runtime), destination, overwrite = TRUE))) {
  stop('Could not install openvdbr runtime libraries.')
}
