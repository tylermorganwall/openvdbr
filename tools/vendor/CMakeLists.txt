cmake_minimum_required(VERSION 3.24)
project(openvdbr_vendor LANGUAGES C CXX)

# tools/configure.R selects sources, dependencies, flags and platform settings.
# These targets contain vendored code only. R builds the package's own sources
# and links its final shared library using the generated Makevars.
include("${CMAKE_BINARY_DIR}/vendor-config.cmake")
add_library(openvdb STATIC ${OPENVDB_SOURCES})
target_compile_definitions(openvdb PRIVATE ${OPENVDB_DEFINITIONS})
target_include_directories(openvdb PRIVATE ${OPENVDB_INCLUDES})

if(BLOSC_SOURCES)
  add_library(blosc STATIC ${BLOSC_SOURCES})
  target_compile_definitions(blosc PRIVATE ${BLOSC_DEFINITIONS})
  target_include_directories(blosc PRIVATE ${BLOSC_INCLUDES})
endif()
