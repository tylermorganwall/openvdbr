cmake_minimum_required(VERSION 3.24)
project(openvdbr_backend LANGUAGES C CXX)
# tools/build.R stages this template in src for the duration of the build.
set(PROVIDER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
# Match OpenVDB's upstream CMake language level for this nested build.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# The provider alone owns OpenVDB and its dependencies. Keep oneTBB shared,
# under a package-specific filename, rather than embedding a scheduler in
# each consumer DLL. Its library is installed beside openvdbr.so.
set(TBB_TEST OFF CACHE BOOL "" FORCE)
set(TBB_STRICT OFF CACHE BOOL "" FORCE)
set(TBBMALLOC_BUILD OFF CACHE BOOL "" FORCE)
set(TBB_ENABLE_IPO OFF CACHE BOOL "" FORCE)
set(TBB_INSTALL OFF CACHE BOOL "" FORCE)
set(TBB_DISABLE_HWLOC_AUTOMATIC_SEARCH ON CACHE BOOL "" FORCE)
add_subdirectory("${PROVIDER_SOURCE_DIR}/vendor/onetbb" vendor/onetbb)
if(APPLE)
  # Use oneTBB's portable coroutine backend instead of macOS's deprecated
  # ucontext API. Propagate the same layout choice to OpenVDB's TBB headers.
  target_compile_definitions(tbb PUBLIC __TBB_RESUMABLE_TASKS_USE_THREADS=1)
endif()
set_target_properties(tbb PROPERTIES OUTPUT_NAME openvdbr_tbb
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime")
set_property(TARGET tbb PROPERTY VERSION)
set_property(TARGET tbb PROPERTY SOVERSION)
if(APPLE)
  set_target_properties(tbb PROPERTIES INSTALL_NAME_DIR "@rpath" BUILD_WITH_INSTALL_NAME_DIR ON)
endif()

# OpenVDB files commonly use Blosc; retain its bundled LZ4/Zstd codecs.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(HIDE_SYMBOLS ON CACHE BOOL "" FORCE)
set(DEACTIVATE_AVX2 ON CACHE BOOL "" FORCE)
set(DEACTIVATE_SSE2 ON CACHE BOOL "" FORCE)
set(PREFER_EXTERNAL_ZLIB ON CACHE BOOL "" FORCE)
add_subdirectory("${PROVIDER_SOURCE_DIR}/vendor/blosc" vendor/blosc)
set_target_properties(blosc_static PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_include_directories(blosc_static PRIVATE "${PROVIDER_SOURCE_DIR}"
  "${PROVIDER_SOURCE_DIR}/../inst/include")
find_package(ZLIB REQUIRED)

# Configure upstream's version header without building unrelated executables,
# Python bindings or explicit template instantiations for unused tools.
set(OpenVDB_MAJOR_VERSION 13)
set(OpenVDB_MINOR_VERSION 1)
set(OpenVDB_PATCH_VERSION 0)
set(OPENVDB_ABI_VERSION_NUMBER 13)
set(OPENVDB_PACKED_VERSION 218169344)
set(OPENVDB_NAMESPACE_SUFFIX _openvdbr)
set(OPENVDB_X86_INSTRSET 0)
set(OPENVDB_PACKAGE_URL "https://github.com/AcademySoftwareFoundation/openvdb")
set(OPENVDB_PACKAGE_REVISION "v13.1.0")
set(OPENVDB_USE_BLOSC ON)
set(OPENVDB_USE_ZLIB ON)
configure_file("${PROVIDER_SOURCE_DIR}/vendor/openvdb/version.h.in" include/openvdb/version.h)
set(VDB_SOURCES
  Grid.cc io/Archive.cc io/Compression.cc io/DelayedLoadMetadata.cc
  io/File.cc io/GridDescriptor.cc io/Queue.cc io/Stream.cc io/TempFile.cc
  math/Maps.cc math/Proximity.cc math/QuantizedUnitVec.cc math/Transform.cc
  Metadata.cc MetaMap.cc openvdb.cc Platform.cc points/AttributeArray.cc
  points/AttributeArrayString.cc points/AttributeGroup.cc points/AttributeSet.cc
  points/StreamCompression.cc points/points.cc util/Assert.cc util/Formats.cc)
list(TRANSFORM VDB_SOURCES PREPEND "${PROVIDER_SOURCE_DIR}/vendor/openvdb/")
add_library(openvdbr_core STATIC ${VDB_SOURCES}
  "${PROVIDER_SOURCE_DIR}/vendor/imath/openvdb/math/Half.cc"
  "${PROVIDER_SOURCE_DIR}/provider.cpp" "${PROVIDER_SOURCE_DIR}/provider_tools.cpp"
  "${PROVIDER_SOURCE_DIR}/diagnostics.cpp" "${PROVIDER_SOURCE_DIR}/volume_io.cpp")
target_compile_definitions(openvdbr_core PRIVATE OPENVDB_STATICLIB NDEBUG)
target_include_directories(openvdbr_core PRIVATE
  "${PROVIDER_SOURCE_DIR}"
  "${PROVIDER_SOURCE_DIR}/../inst/include" "${CMAKE_BINARY_DIR}/include" "${CMAKE_BINARY_DIR}/include/openvdb"
  "${PROVIDER_SOURCE_DIR}/vendor" "${PROVIDER_SOURCE_DIR}/vendor/imath"
  "${PROVIDER_SOURCE_DIR}/vendor/blosc/blosc")
target_link_libraries(openvdbr_core PUBLIC TBB::tbb blosc_static ZLIB::ZLIB)
set_source_files_properties("${PROVIDER_SOURCE_DIR}/volume_io.cpp" PROPERTIES
  COMPILE_DEFINITIONS "NANOVDB_USE_OPENVDB;NANOVDB_USE_TBB")
