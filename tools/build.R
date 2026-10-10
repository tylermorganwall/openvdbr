# Build from src so CMake object paths stay relative, including in an
# Rcheck directory. Remove the generated CMake entry point on every exit.
build_backend = function(args) {
  if (
    !file.copy('backend/CMakeLists.txt', 'CMakeLists.txt', overwrite = TRUE)
  ) {
    stop('Could not stage the CMake entry point.')
  }
  on.exit(unlink('CMakeLists.txt'), add = TRUE)
  cmake = Sys.which('cmake')
  if (!nzchar(cmake)) {
    stop('CMake >= 3.24 is required to build openvdbr.')
  }
  Sys.setenv(CXX = args[1], CC = args[3])
  # This process controls the nested build's worker count itself. Inherited
  # jobserver descriptors are not available to the CMake subprocess.
  Sys.unsetenv(c('MAKEFLAGS', 'MFLAGS'))
  configure = c(
    '-S',
    '.',
    '-B',
    'build',
    '-G',
    'Unix Makefiles',
    '-DCMAKE_BUILD_TYPE=Release',
    # Make exports these separately so quotes in include/library paths survive
    # the shell. R's ALL_CXXFLAGS/ALL_CFLAGS do not contain CPPFLAGS or LDFLAGS.
    paste0('-DCMAKE_CXX_FLAGS=', args[2], ' ', Sys.getenv('OPENVDBR_CPPFLAGS')),
    paste0('-DCMAKE_C_FLAGS=', args[4], ' ', Sys.getenv('OPENVDBR_CPPFLAGS')),
    paste0('-DCMAKE_SHARED_LINKER_FLAGS=', Sys.getenv('OPENVDBR_LDFLAGS')),
    paste0('-DCMAKE_MODULE_LINKER_FLAGS=', Sys.getenv('OPENVDBR_LDFLAGS')),
    paste0('-DCMAKE_EXE_LINKER_FLAGS=', Sys.getenv('OPENVDBR_LDFLAGS')),
    '-DCMAKE_CXX_FLAGS_RELEASE=-DNDEBUG',
    '-DCMAKE_C_FLAGS_RELEASE=-DNDEBUG'
  )
  if (Sys.info()[['sysname']] == 'Darwin') {
    # R can target arm64 while CMake itself runs under Rosetta. Tell dependency
    # feature tests the target architecture, not the architecture of CMake.
    architectures = regmatches(args[1], gregexpr('-arch +[^ ]+', args[1]))[[1]]
    architectures = sub('-arch +', '', architectures)
    if (length(architectures)) {
      configure = c(
        configure,
        paste0(
          '-DCMAKE_OSX_ARCHITECTURES=',
          paste(architectures, collapse = ';')
        )
      )
    }
  }
  if (system2(cmake, shQuote(configure)) != 0L) {
    stop('OpenVDB configuration failed.')
  }
  # Use the caller's requested build parallelism. Default to two workers to
  # bound memory use when instantiating OpenVDB's large grid templates.
  jobs = Sys.getenv('CMAKE_BUILD_PARALLEL_LEVEL', '2')
  if (!grepl('^[1-9][0-9]*$', jobs)) {
    stop('Invalid CMAKE_BUILD_PARALLEL_LEVEL.')
  }
  if (system2(cmake, c('--build', 'build', '--parallel', jobs)) != 0L) {
    stop('OpenVDB build failed.')
  }
}

build_backend(commandArgs(trailingOnly = TRUE))
