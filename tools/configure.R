# All toolchain, dependency and platform choices live here. CMake only builds
# the selected vendored sources; R compiles and links the package interface.
configure_package = function() {
  wasm = any(grepl(
    "^--host=wasm32-.*emscripten$",
    commandArgs(trailingOnly = TRUE)
  ))
  windows = !wasm && .Platform$OS.type == "windows"
  macos = !wasm && Sys.info()[["sysname"]] == "Darwin"
  cmake = Sys.getenv("CMAKE", unname(Sys.which("cmake")))
  if (!nzchar(cmake)) {
    stop("CMake >= 3.24 is required; install it or set CMAKE.")
  }
  version = system2(cmake, "--version", stdout = TRUE)
  if (
    !is.null(attr(version, "status")) ||
      !length(version) ||
      !grepl("^cmake version [0-9]+[.][0-9]+", version[1L])
  ) {
    stop("Could not determine CMake version; check CMAKE.")
  }
  version = sub("^cmake version ([0-9.]+).*$", "\\1", version[1L])
  if (
    !is.null(attr(version, "status")) ||
      utils::compareVersion(version, "3.24") < 0L
  ) {
    stop("CMake >= 3.24 is required; install it or set CMAKE.")
  }
  use_system_blosc = tolower(Sys.getenv("OPENVDBR_USE_SYSTEM_BLOSC", "true"))
  if (!use_system_blosc %in% c("true", "false")) {
    stop("OPENVDBR_USE_SYSTEM_BLOSC must be true or false.")
  }
  jobs = Sys.getenv("CMAKE_BUILD_PARALLEL_LEVEL", "2")
  if (!grepl("^[1-9][0-9]*$", jobs)) {
    stop("Invalid CMAKE_BUILD_PARALLEL_LEVEL.")
  }
  variables = c(
    "CC",
    "CXX",
    "CFLAGS",
    "CXXFLAGS",
    "CPICFLAGS",
    "CXXPICFLAGS",
    "SHLIB_CFLAGS",
    "SHLIB_CXXFLAGS",
    "CPPFLAGS",
    "LDFLAGS"
  )
  config = setNames(vapply(variables, r_config, character(1)), variables)
  # CC/CXX can contain a launcher, architecture and standard-library switches.
  # CMake parses these complete commands from the environment on its first run.
  previous = Sys.getenv(
    c("CC", "CXX", "MAKEFLAGS", "MFLAGS"),
    unset = NA_character_
  )
  on.exit(
    {
      for (name in names(previous)) {
        if (is.na(previous[[name]])) {
          Sys.unsetenv(name)
        } else {
          do.call(Sys.setenv, setNames(list(previous[[name]]), name))
        }
      }
    },
    add = TRUE
  )
  Sys.setenv(CC = config[["CC"]], CXX = config[["CXX"]])
  Sys.unsetenv(c("MAKEFLAGS", "MFLAGS"))

  if (
    !requireNamespace("RcppParallel", quietly = TRUE) ||
      utils::packageVersion("RcppParallel") < "6.2.1"
  ) {
    stop("RcppParallel >= 6.2.1 is required to supply oneTBB.")
  }
  tbb_cppflags = paste(capture.output(RcppParallel::CxxFlags()), collapse = " ")
  tbb_libs = paste(
    capture.output(RcppParallel::RcppParallelLibs()),
    collapse = " "
  )
  # LinkingTo also supplies this include path to R, but not to the vendored build.
  tbb_cppflags = paste(
    tbb_cppflags,
    paste0(
      "-I",
      shQuote(
        system.file("include", package = "RcppParallel"),
        type = "sh"
      )
    )
  )
  probe_dir = tempfile("openvdbr-config-")
  dir.create(probe_dir)
  on.exit(unlink(probe_dir, recursive = TRUE), add = TRUE)
  log = normalizePath(".", winslash = "/")
  log = file.path(log, "config.log")
  writeLines("openvdbr dependency probes (R's configured C++ compiler)", log)
  # On Linux RcppParallel loads oneTBB into R. The standalone probe has to
  # link it explicitly, even when RcppParallelLibs() emits no flags for a DLL.
  tbb_probe_libs = tbb_libs
  if (wasm) {
    # webR loads RcppParallel before this side module and resolves its oneTBB
    # imports there. The R process running configure has host libraries.
    tbb_libs = ""
    tbb_probe_libs = ""
  } else if (Sys.info()[["sysname"]] == "Linux" && !nzchar(trimws(tbb_libs))) {
    tbb_library = RcppParallel::tbbLibraryPath("tbb")
    tbb_probe_libs = if (is.null(tbb_library)) {
      "-ltbb"
    } else {
      shQuote(tbb_library, type = "sh")
    }
  }
  if (
    !probe_library("tbb", tbb_cppflags, tbb_probe_libs, config, probe_dir, log)
  ) {
    stop(
      "RcppParallel's oneTBB headers and libraries are incompatible with R's compiler; see config.log."
    )
  }
  cat(
    "Using oneTBB from RcppParallel ",
    as.character(utils::packageVersion("RcppParallel")),
    "\n",
    sep = ""
  )

  zlib = pkg_config_flags("zlib", macos)
  if (is.null(zlib)) {
    zlib = list(cppflags = "", libs = "-lz")
  }
  if (
    !probe_library("zlib", zlib$cppflags, zlib$libs, config, probe_dir, log)
  ) {
    stop("A usable zlib development library is required; see config.log.")
  }
  blosc = NULL
  if (use_system_blosc == "true") {
    blosc = find_blosc(config, probe_dir, log, macos)
  }
  bundled_blosc = is.null(blosc)
  if (bundled_blosc) {
    cat("Using bundled Blosc 1.21.6\n")
    blosc = list(
      cppflags = "-Ivendor/blosc/blosc",
      libs = "build/libblosc.a"
    )
  }

  # Invalidate the old staged-src CMake build and any cache made with different
  # compiler commands, flags, dependency providers, or package locations.
  signature = list(
    layout = 5L,
    architecture = R.version$arch,
    root = normalizePath(".", winslash = "/"),
    cmake = cmake,
    config = config,
    tbb_cppflags = tbb_cppflags,
    tbb_libs = tbb_libs,
    zlib = zlib,
    blosc = blosc
  )
  stamp = "src/build/configuration.rds"
  if (
    dir.exists("src/build") &&
      (!file.exists(stamp) ||
        !identical(readRDS(stamp), signature))
  ) {
    unlink("src/build", recursive = TRUE)
  }
  dir.create(
    "src/build/include/openvdb",
    recursive = TRUE,
    showWarnings = FALSE
  )
  # Remove only the obsolete entry point produced by the previous configure.
  unlink("src/CMakeLists.txt")
  # Stage the recipe inside the vendored source boundary so CMake uses short,
  # relative object paths. Its maintained template stays with the build tools.
  if (
    !file.copy(
      "tools/vendor/CMakeLists.txt",
      "src/vendor/CMakeLists.txt",
      overwrite = TRUE
    )
  ) {
    stop("Could not stage the vendored library build recipe.")
  }
  configure_version_header()

  vdb_sources = c(
    "Grid.cc",
    "io/Archive.cc",
    "io/Compression.cc",
    "io/DelayedLoadMetadata.cc",
    "io/File.cc",
    "io/GridDescriptor.cc",
    "io/Queue.cc",
    "io/Stream.cc",
    "io/TempFile.cc",
    "math/Maps.cc",
    "math/Proximity.cc",
    "math/QuantizedUnitVec.cc",
    "math/Transform.cc",
    "Metadata.cc",
    "MetaMap.cc",
    "openvdb.cc",
    "Platform.cc",
    "points/AttributeArray.cc",
    "points/AttributeArrayString.cc",
    "points/AttributeGroup.cc",
    "points/AttributeSet.cc",
    "points/StreamCompression.cc",
    "points/points.cc",
    "util/Assert.cc",
    "util/Formats.cc"
  )
  vdb_sources = c(
    file.path("src/vendor/openvdb", vdb_sources),
    "src/vendor/imath/openvdb/math/Half.cc"
  )
  blosc_sources = character()
  blosc_includes = character()
  if (bundled_blosc) {
    codec_root = "src/vendor/blosc/internal-complibs"
    zstd = file.path(codec_root, "zstd-1.5.6")
    blosc_sources = c(
      file.path(
        "src/vendor/blosc/blosc",
        c(
          "blosc.c",
          "blosclz.c",
          "fastcopy.c",
          "shuffle-generic.c",
          "bitshuffle-generic.c",
          "shuffle.c"
        )
      ),
      list.files(
        file.path(codec_root, "lz4-1.9.4"),
        "[.]c$",
        full.names = TRUE
      ),
      list.files(
        file.path(zstd, c("common", "compress", "decompress", "dictBuilder")),
        "[.]c$",
        full.names = TRUE
      )
    )
    blosc_includes = c(
      "src/vendor/blosc/blosc",
      file.path(codec_root, "lz4-1.9.4"),
      zstd,
      file.path(zstd, "common")
    )
  }
  includes = c(
    "src",
    "inst/include",
    "src/build/include",
    "src/build/include/openvdb",
    "src/vendor",
    "src/vendor/imath"
  )
  settings = list(
    OPENVDB_SOURCES = normalizePath(vdb_sources, winslash = "/"),
    OPENVDB_DEFINITIONS = "OPENVDB_STATICLIB",
    OPENVDB_INCLUDES = normalizePath(includes, winslash = "/"),
    BLOSC_SOURCES = normalizePath(blosc_sources, winslash = "/"),
    BLOSC_INCLUDES = normalizePath(
      c("src", "inst/include", blosc_includes),
      winslash = "/"
    ),
    BLOSC_DEFINITIONS = c(
      "HAVE_LZ4",
      "HAVE_ZLIB",
      "HAVE_ZSTD",
      "ZSTD_DISABLE_ASM"
    )
  )
  # Bracket quoting keeps spaces, backslashes and dollar signs literal in paths.
  cmake_settings = unlist(lapply(names(settings), function(name) {
    c(paste0("set(", name), paste0("  [==[", settings[[name]], "]==]"), ")")
  }))
  writeLines(cmake_settings, "src/build/vendor-config.cmake")
  cppflags = paste(
    tbb_cppflags,
    zlib$cppflags,
    if (bundled_blosc) {
      paste0(
        "-I",
        shQuote(
          normalizePath("src/vendor/blosc/blosc", winslash = "/"),
          type = "sh"
        )
      )
    } else {
      blosc$cppflags
    }
  )
  args = c(
    "-S",
    "src/vendor",
    "-B",
    "src/build",
    "-G",
    "Unix Makefiles",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
    "-DCMAKE_C_VISIBILITY_PRESET=hidden",
    "-DCMAKE_CXX_VISIBILITY_PRESET=hidden",
    "-DCMAKE_VISIBILITY_INLINES_HIDDEN=ON",
    paste0(
      "-DCMAKE_CXX_FLAGS=",
      paste(
        config[c("CXXPICFLAGS", "SHLIB_CXXFLAGS", "CXXFLAGS")],
        collapse = " "
      ),
      " ",
      cppflags,
      " ",
      config[["CPPFLAGS"]]
    ),
    paste0(
      "-DCMAKE_C_FLAGS=",
      paste(
        config[c("CPICFLAGS", "SHLIB_CFLAGS", "CFLAGS")],
        collapse = " "
      ),
      " ",
      zlib$cppflags,
      " ",
      config[["CPPFLAGS"]]
    ),
    paste0("-DCMAKE_EXE_LINKER_FLAGS=", config[["LDFLAGS"]]),
    "-DCMAKE_CXX_FLAGS_RELEASE=-DNDEBUG",
    "-DCMAKE_C_FLAGS_RELEASE=-DNDEBUG"
  )
  if (macos) {
    arch = regmatches(
      config[["CXX"]],
      gregexpr("-arch +[^ ]+", config[["CXX"]])
    )[[1L]]
    if (!length(arch)) {
      arch = if (R.version$arch == "aarch64") "arm64" else R.version$arch
    }
    if (length(arch)) {
      args = c(
        args,
        paste0(
          "-DCMAKE_OSX_ARCHITECTURES=",
          paste(sub("-arch +", "", arch), collapse = ";")
        )
      )
    }
  }
  if (system2(cmake, shQuote(args)) != 0L) {
    stop("Vendored OpenVDB configuration failed.")
  }
  saveRDS(signature, stamp)

  exports = if (windows || wasm) {
    # Windows picks up src/openvdbr-win.def; webR links an Emscripten side module.
    ""
  } else if (macos) {
    "-Wl,-exported_symbols_list,openvdbr.exports"
  } else {
    "-Wl,--version-script=openvdbr.map"
  }
  values = c(
    CMAKE = shQuote(cmake, type = "sh"),
    BUILD_JOBS = jobs,
    NATIVE_CPPFLAGS = paste(
      "-DOPENVDB_STATICLIB -DNANOVDB_USE_OPENVDB -DNANOVDB_USE_TBB",
      "-I. -I../inst/include -Ibuild/include -Ibuild/include/openvdb -Ivendor -Ivendor/imath",
      tbb_cppflags,
      zlib$cppflags,
      blosc$cppflags
    ),
    BLOSC_LIBS = blosc$libs,
    ZLIB_LIBS = zlib$libs,
    TBB_LIBS = tbb_libs,
    EXPORT_FLAGS = exports
  )
  target = if (windows) "src/Makevars.win" else "src/Makevars"
  template = readLines(paste0(target, ".in"))
  for (name in names(values)) {
    template = gsub(
      paste0("@", name, "@"),
      make_escape(values[[name]]),
      template,
      fixed = TRUE
    )
  }
  if (any(grepl("@[A-Z_]+@", template))) {
    stop("Unresolved Makevars variable.")
  }
  writeLines(template, target)
  normalize_build_line_endings()
  cat(
    "Configured ",
    target,
    "; R compiles all package interface sources.\n",
    sep = ""
  )
}

#' @param name R configuration variable.
#' @return Its expanded value, including site and user Makevars settings.
#' @keywords internal
r_config = function(name) {
  r = file.path(
    R.home("bin"),
    if (.Platform$OS.type == "windows") "R.exe" else "R"
  )
  value = system2(r, c("CMD", "config", name), stdout = TRUE)
  if (!is.null(attr(value, "status"))) {
    stop("Could not read R configuration: ", name)
  }
  paste(value, collapse = " ")
}

#' @param name Name of the C++ probe under tools/vendor/probes.
#' @param cppflags Header/compiler flags for the selected dependency.
#' @param libs Matching linker flags.
#' @param config R's compiler settings.
#' @param directory Temporary directory for the executable and Makefile.
#' @param log File receiving compiler diagnostics.
#' @return Whether the dependency compiles and links with R's full toolchain.
#' @keywords internal
probe_library = function(name, cppflags, libs, config, directory, log) {
  source = normalizePath(
    file.path("tools/vendor/probes", paste0(name, ".cpp")),
    winslash = "/"
  )
  binary = file.path(directory, "probe.exe")
  makefile = file.path(directory, "Makefile")
  flags = c(
    config[c("CXX", "CPPFLAGS", "CXXFLAGS", "LDFLAGS")],
    PKG_CPPFLAGS = cppflags,
    PKG_LIBS = libs
  )
  lines = paste0(names(flags), " = ", vapply(flags, make_escape, character(1)))
  lines = c(
    lines,
    "all:",
    paste0(
      "\t$(CXX) $(PKG_CPPFLAGS) $(CPPFLAGS) $(CXXFLAGS) ",
      make_escape(shQuote(source, type = "sh")),
      " -o ",
      make_escape(shQuote(binary, type = "sh")),
      " $(LDFLAGS) $(PKG_LIBS)"
    )
  )
  writeLines(lines, makefile)
  output = suppressWarnings(system2(
    Sys.getenv("MAKE", "make"),
    c("-f", shQuote(makefile)),
    stdout = TRUE,
    stderr = TRUE
  ))
  cat(
    paste(c(paste0("\nProbe: ", name), output), collapse = "\n"),
    "\n",
    file = log,
    append = TRUE
  )
  is.null(attr(output, "status"))
}

#' @param module pkg-config module name.
#' @param static Whether to include transitive static-link dependencies.
#' @return Matching compile/link flags, or NULL when the module is unavailable.
#' @keywords internal
pkg_config_flags = function(module, static) {
  pkg_config = Sys.getenv("PKG_CONFIG", unname(Sys.which("pkg-config")))
  if (!nzchar(pkg_config)) {
    return(NULL)
  }
  flags = lapply(c("--cflags", "--libs"), function(option) {
    suppressWarnings(system2(
      pkg_config,
      shQuote(c(if (static) "--static", option, module)),
      stdout = TRUE,
      stderr = FALSE
    ))
  })
  if (any(vapply(flags, function(x) !is.null(attr(x, "status")), logical(1)))) {
    return(NULL)
  }
  list(
    cppflags = paste(flags[[1L]], collapse = " "),
    libs = paste(flags[[2L]], collapse = " ")
  )
}

#' @param config R's compiler settings.
#' @param directory Temporary probe directory.
#' @param log File receiving compiler diagnostics.
#' @param macos Whether to include macOS static-link dependencies.
#' @return A usable system Blosc's flags, or NULL to select the vendored fallback.
#' @keywords internal
find_blosc = function(config, directory, log, macos) {
  inc = Sys.getenv("BLOSC_INCLUDE_DIR")
  lib = Sys.getenv("BLOSC_LIB_DIR")
  candidates = list()
  if (nzchar(paste0(inc, lib))) {
    candidates[[1L]] = list(
      cppflags = if (nzchar(inc)) {
        paste0("-I", shQuote(inc, type = "sh"))
      } else {
        ""
      },
      libs = paste(
        if (nzchar(lib)) paste0("-L", shQuote(lib, type = "sh")),
        "-lblosc"
      )
    )
  } else {
    # Follow CRAN blosc's pkg-config approach without downloading dependencies
    # or invoking its R API from OpenVDB's native worker threads.
    candidates = list(pkg_config_flags("blosc >= 1.17.0", macos))
    prefixes = unique(c(
      strsplit(
        Sys.getenv("CMAKE_PREFIX_PATH"),
        .Platform$path.sep,
        fixed = TRUE
      )[[1L]],
      if (macos) {
        c(
          file.path(
            "/opt/R",
            if (R.version$arch == "aarch64") "arm64" else R.version$arch
          ),
          "/opt/homebrew"
        )
      },
      "/usr/local",
      "/usr"
    ))
    for (prefix in prefixes[nzchar(prefixes)]) {
      if (!file.exists(file.path(prefix, "include/blosc.h"))) {
        next
      }
      for (libdir in file.path(prefix, c("lib", "lib64"))) {
        libraries = file.path(
          libdir,
          c("libblosc.dylib", "libblosc.so", "libblosc.dll.a", "libblosc.a")
        )
        libraries = libraries[file.exists(libraries)]
        if (!length(libraries)) {
          next
        }
        candidates[[length(candidates) + 1L]] = list(
          cppflags = paste0(
            "-I",
            shQuote(file.path(prefix, "include"), type = "sh")
          ),
          libs = shQuote(libraries[1L], type = "sh")
        )
      }
    }
    # Rtools and configured toolchains may already provide the right search paths.
    candidates[[length(candidates) + 1L]] = list(
      cppflags = "",
      libs = "-lblosc"
    )
  }
  for (candidate in candidates) {
    if (
      !is.null(candidate) &&
        probe_library(
          "blosc",
          candidate$cppflags,
          candidate$libs,
          config,
          directory,
          log
        )
    ) {
      cat("Using system Blosc: ", candidate$libs, "\n", sep = "")
      return(candidate)
    }
  }
  NULL
}

#' @return Writes OpenVDB's version header for the package's private ABI.
#' @keywords internal
configure_version_header = function() {
  values = c(
    OpenVDB_MAJOR_VERSION = 13,
    OpenVDB_MINOR_VERSION = 1,
    OpenVDB_PATCH_VERSION = 0,
    OPENVDB_ABI_VERSION_NUMBER = 13,
    OPENVDB_PACKED_VERSION = 218169344,
    OPENVDB_NAMESPACE_SUFFIX = "_openvdbr",
    OPENVDB_X86_INSTRSET = 0,
    OPENVDB_PACKAGE_URL = "https://github.com/AcademySoftwareFoundation/openvdb",
    OPENVDB_PACKAGE_REVISION = "v13.1.0",
    OPENVDB_REAL_TREE_INSTANTIATIONS = "",
    OPENVDB_NUMERIC_TREE_INSTANTIATIONS = "",
    OPENVDB_VOLUME_TREE_INSTANTIATIONS = "",
    OPENVDB_ALL_TREE_INSTANTIATIONS = ""
  )
  header = readLines("src/vendor/openvdb/version.h.in")
  for (name in names(values)) {
    header = gsub(paste0("${", name, "}"), values[[name]], header, fixed = TRUE)
    header = gsub(paste0("@", name, "@"), values[[name]], header, fixed = TRUE)
  }
  enabled = c("OPENVDB_USE_BLOSC", "OPENVDB_USE_ZLIB")
  for (i in grep("^#cmakedefine ", header)) {
    name = sub("^#cmakedefine +([A-Za-z0-9_]+).*$", "\\1", header[i])
    header[i] = if (name %in% enabled) {
      paste("#define", name)
    } else {
      paste0("/* #undef ", name, " */")
    }
  }
  if (any(grepl("\\$\\{|@[A-Za-z_]+@|#cmakedefine", header))) {
    stop("Unresolved version header variable.")
  }
  writeLines(header, "src/build/include/openvdb/version.h")
}

#' @param value Literal value to insert into a Makefile assignment or recipe.
#' @return Value with Make's dollar-sign and comment syntax escaped.
#' @keywords internal
make_escape = function(value) {
  gsub("#", "\\#", gsub("$", "$$", value, fixed = TRUE), fixed = TRUE)
}

#' @return Converts generated native text to LF without changing dependency timestamps.
#' @keywords internal
normalize_build_line_endings = function() {
  paths = c(
    list.files("src/build", recursive = TRUE, full.names = TRUE),
    "src/Makevars",
    "src/Makevars.win"
  )
  paths = paths[
    file.exists(paths) &
      grepl(
        "^(Makefile|Makevars([.]win)?)$|[.](make|cmake|c|cpp|h)$",
        basename(paths)
      )
  ]
  for (path in paths) {
    info = file.info(path)
    bytes = readBin(path, "raw", n = info$size)
    if (length(bytes) < 2L || any(bytes == as.raw(0))) {
      next
    }
    crlf = which(bytes[-length(bytes)] == as.raw(13) & bytes[-1L] == as.raw(10))
    if (!length(crlf)) {
      next
    }
    writeBin(bytes[-crlf], path)
    Sys.setFileTime(path, info$mtime)
  }
  invisible(NULL)
}

configure_package()
