#include <Rcpp.h>
#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h>
#include "provider.h"
#include <memory>
#include <vector>

namespace {
const openvdbr_api_v1* api() { return openvdbr_provider_api(); }
void check(int32_t status, const openvdbr_error& error) {
    if (status != OPENVDBR_OK) Rcpp::stop("%s", error.message);
}
openvdbr_grid* grid_pointer(SEXP handle) {
    if (TYPEOF(handle) != EXTPTRSXP || R_ExternalPtrTag(handle) != Rf_install("openvdbr.grid.v1"))
        Rcpp::stop("Expected an openvdbr grid");
    auto* grid = static_cast<openvdbr_grid*>(R_ExternalPtrAddr(handle));
    if (!grid) Rcpp::stop("Grid is closed or was serialized; use vdb_read() to restore data");
    return grid;
}
void finalize_grid(SEXP handle) {
    api()->grid_destroy(static_cast<openvdbr_grid*>(R_ExternalPtrAddr(handle)));
    R_ClearExternalPtr(handle);
}
template<class Factory> SEXP grid_handle(Factory create) {
    // Root the provider namespace for the lifetime of this external pointer.
    // Allocate the R handle before creating the native resource.
    Rcpp::Environment owner = Rcpp::Environment::namespace_env("openvdbr");
    Rcpp::RObject handle(R_MakeExternalPtr(nullptr, Rf_install("openvdbr.grid.v1"), owner));
    handle.attr("class") = "openvdbr_grid";
    R_RegisterCFinalizerEx(handle, finalize_grid, TRUE);
    openvdbr_grid* grid = nullptr;
    openvdbr_error error{};
    check(create(&grid, &error), error);
    R_SetExternalPtrAddr(handle, grid);
    return handle;
}
template<int Type> std::vector<typename Rcpp::traits::storage_type<Type>::type>
interleave(SEXP value) {
    Rcpp::Matrix<Type> matrix(value);
    if (matrix.ncol() != 3) Rcpp::stop("Expected a matrix with three columns");
    using Value = typename Rcpp::traits::storage_type<Type>::type;
    std::vector<Value> result(size_t(matrix.nrow()) * 3);
    for (int i = 0; i < matrix.nrow(); ++i)
        for (int a = 0; a < 3; ++a) result[3*size_t(i)+a] = matrix(i, a);
    return result;
}
using File = std::unique_ptr<openvdbr_file, decltype(openvdbr_api_v1::file_destroy)>;
File open_file(SEXP path) {
    openvdbr_file* file = nullptr;
    openvdbr_error error{};
    check(api()->file_open(Rcpp::as<std::string>(path).c_str(), &file, &error), error);
    return File(file, api()->file_destroy);
}
SEXP r_version() { BEGIN_RCPP return Rcpp::wrap(api()->version()); END_RCPP }
SEXP r_messages() {
    BEGIN_RCPP
    std::vector<std::string> messages;
    openvdbr_error message{};
    while (api()->diagnostic_next(&message)) messages.emplace_back(message.message);
    return Rcpp::wrap(messages);
    END_RCPP
}
SEXP r_api(SEXP owner) {
    BEGIN_RCPP
    Rcpp::RObject result(R_MakeExternalPtr(const_cast<openvdbr_api_v1*>(api()), Rf_install("openvdbr.api.v1"), owner));
    result.attr("class") = "openvdbr_api_v1";
    return result;
    END_RCPP
}
SEXP r_create(SEXP type, SEXP background, SEXP voxel_size, SEXP origin, SEXP name) {
    BEGIN_RCPP
    Rcpp::NumericVector center(origin);
    if (center.size() != 3) Rcpp::stop("origin must have length three");
    return grid_handle([&](openvdbr_grid** out, openvdbr_error* error) {
        return api()->grid_create(Rcpp::as<int>(type), Rcpp::as<double>(background),
            Rcpp::as<double>(voxel_size), center.begin(), Rcpp::as<std::string>(name).c_str(), out, error);
    });
    END_RCPP
}
SEXP r_sphere(SEXP radius, SEXP center, SEXP voxel_size, SEXP half_width, SEXP name) {
    BEGIN_RCPP
    Rcpp::NumericVector position(center);
    if (position.size() != 3) Rcpp::stop("center must have length three");
    return grid_handle([&](openvdbr_grid** out, openvdbr_error* error) {
        return api()->level_set_sphere(Rcpp::as<double>(radius), position.begin(),
            Rcpp::as<double>(voxel_size), Rcpp::as<double>(half_width),
            Rcpp::as<std::string>(name).c_str(), out, error);
    });
    END_RCPP
}
SEXP r_clone(SEXP handle) {
    BEGIN_RCPP
    auto* grid = grid_pointer(handle);
    return grid_handle([&](openvdbr_grid** out, openvdbr_error* error) { return api()->grid_clone(grid, out, error); });
    END_RCPP
}
SEXP r_close(SEXP handle) {
    BEGIN_RCPP
    if (TYPEOF(handle) != EXTPTRSXP || R_ExternalPtrTag(handle) != Rf_install("openvdbr.grid.v1"))
        Rcpp::stop("Expected an openvdbr grid");
    finalize_grid(handle);
    return R_NilValue;
    END_RCPP
}
SEXP r_info(SEXP handle) {
    BEGIN_RCPP
    openvdbr_grid_info_v1 info{}; openvdbr_error error{};
    check(api()->grid_info(grid_pointer(handle), &info, &error), error);
    Rcpp::IntegerMatrix bounds(2, 3);
    for (int a = 0; a < 3; ++a) { bounds(0,a) = info.index_min[a]; bounds(1,a) = info.index_max[a]; }
    bounds.attr("dimnames") = Rcpp::List::create(Rcpp::CharacterVector::create("min", "max"),
                                               Rcpp::CharacterVector::create("x", "y", "z"));
    const char* classes[] = {"unknown", "level_set", "fog_volume", "staggered"};
    return Rcpp::List::create(
        Rcpp::Named("name") = info.name, Rcpp::Named("value_type") = info.value_type,
        Rcpp::Named("grid_class") = classes[info.grid_class >= 0 && info.grid_class <= 3 ? info.grid_class : 0],
        Rcpp::Named("active_voxels") = double(info.active_voxels),
        Rcpp::Named("memory_bytes") = double(info.memory_bytes),
        Rcpp::Named("empty") = bool(info.empty), Rcpp::Named("index_bounds") = bounds,
        Rcpp::Named("voxel_size") = Rcpp::NumericVector(info.voxel_size, info.voxel_size + 3),
        Rcpp::Named("origin") = Rcpp::NumericVector(info.origin, info.origin + 3),
        Rcpp::Named("background") = info.background);
    END_RCPP
}
SEXP r_set(SEXP handle, SEXP xyz, SEXP values, SEXP active) {
    BEGIN_RCPP
    auto points = interleave<INTSXP>(xyz);
    Rcpp::NumericVector numbers(values); Rcpp::LogicalVector states(active);
    const size_t count = points.size() / 3;
    if (numbers.size() != R_xlen_t(count) || states.size() != R_xlen_t(count)) Rcpp::stop("Input lengths differ");
    std::vector<uint8_t> flags(count);
    for (size_t i = 0; i < count; ++i) {
        if (states[i] == NA_LOGICAL) Rcpp::stop("active cannot contain NA");
        flags[i] = states[i];
    }
    openvdbr_error error{};
    check(api()->grid_set(grid_pointer(handle), count, points.data(), numbers.begin(), flags.data(), &error), error);
    return handle;
    END_RCPP
}
SEXP r_get(SEXP handle, SEXP xyz) {
    BEGIN_RCPP
    auto points = interleave<INTSXP>(xyz);
    const size_t count = points.size() / 3;
    Rcpp::NumericVector values(count); Rcpp::LogicalVector active(count);
    std::vector<uint8_t> flags(count); openvdbr_error error{};
    check(api()->grid_get(grid_pointer(handle), count, points.data(), values.begin(), flags.data(), &error), error);
    for (size_t i = 0; i < count; ++i) active[i] = flags[i];
    return Rcpp::List::create(Rcpp::Named("value") = values, Rcpp::Named("active") = active);
    END_RCPP
}
SEXP r_sample(SEXP handle, SEXP xyz, SEXP world, SEXP interpolation) {
    BEGIN_RCPP
    auto points = interleave<REALSXP>(xyz);
    Rcpp::NumericVector values(points.size() / 3); openvdbr_error error{};
    check(api()->grid_sample(grid_pointer(handle), values.size(), points.data(),
        Rcpp::as<int>(world), Rcpp::as<int>(interpolation), values.begin(), &error), error);
    return values;
    END_RCPP
}
SEXP r_names(SEXP path) {
    BEGIN_RCPP
    auto file = open_file(path); openvdbr_error error{};
    const uint64_t count = api()->file_count(file.get());
    if (count > INT_MAX) Rcpp::stop("Too many grids in file");
    Rcpp::CharacterVector names(count);
    for (uint64_t i = 0; i < count; ++i) {
        const char* name = nullptr;
        check(api()->file_name(file.get(), i, &name, &error), error); names[i] = name;
    }
    return names;
    END_RCPP
}
SEXP r_read(SEXP path, SEXP selector) {
    BEGIN_RCPP
    auto file = open_file(path);
    const uint64_t count = api()->file_count(file.get());
    uint64_t index = count;
    if (TYPEOF(selector) == STRSXP) {
        const std::string wanted = Rcpp::as<std::string>(selector);
        for (uint64_t i = 0; i < count; ++i) {
            const char* name = nullptr; openvdbr_error error{};
            check(api()->file_name(file.get(), i, &name, &error), error);
            if (wanted == name) { index = i; break; }
        }
    } else {
        const double value = Rcpp::as<double>(selector);
        if (std::isfinite(value) && value >= 1 && value <= double(count) && value == std::floor(value))
            index = static_cast<uint64_t>(value - 1);
    }
    if (index >= count) Rcpp::stop("Requested grid was not found in the VDB file");
    return grid_handle([&](openvdbr_grid** out, openvdbr_error* error) { return api()->file_read(file.get(), index, out, error); });
    END_RCPP
}
SEXP r_write(SEXP handles, SEXP path) {
    BEGIN_RCPP
    Rcpp::List list(handles); std::vector<const openvdbr_grid*> grids(list.size());
    for (R_xlen_t i = 0; i < list.size(); ++i) grids[i] = grid_pointer(list[i]);
    openvdbr_error error{};
    check(api()->file_write(Rcpp::as<std::string>(path).c_str(), grids.size(), grids.data(), &error), error);
    return R_NilValue;
    END_RCPP
}
SEXP r_csg(SEXP first, SEXP second, SEXP operation) {
    BEGIN_RCPP
    auto* a = grid_pointer(first); auto* b = grid_pointer(second);
    return grid_handle([&](openvdbr_grid** out, openvdbr_error* error) {
        return api()->level_set_csg(a, b, Rcpp::as<int>(operation), out, error);
    });
    END_RCPP
}
SEXP r_fog(SEXP handle) {
    BEGIN_RCPP
    auto* grid = grid_pointer(handle);
    return grid_handle([&](openvdbr_grid** out, openvdbr_error* error) { return api()->level_set_fog(grid, out, error); });
    END_RCPP
}
SEXP r_from_mesh(SEXP xyz, SEXP indices, SEXP voxel_size, SEXP half_width, SEXP name) {
    BEGIN_RCPP
    auto points = interleave<REALSXP>(xyz); auto faces = interleave<INTSXP>(indices);
    return grid_handle([&](openvdbr_grid** out, openvdbr_error* error) {
        return api()->mesh_to_level_set(points.size()/3, points.data(), faces.size()/3, faces.data(),
            Rcpp::as<double>(voxel_size), Rcpp::as<double>(half_width), Rcpp::as<std::string>(name).c_str(), out, error);
    });
    END_RCPP
}
SEXP r_to_mesh(SEXP handle, SEXP isovalue, SEXP adaptivity) {
    BEGIN_RCPP
    openvdbr_mesh* result = nullptr; openvdbr_error error{};
    check(api()->grid_to_mesh(grid_pointer(handle), Rcpp::as<double>(isovalue), Rcpp::as<double>(adaptivity), &result, &error), error);
    std::unique_ptr<openvdbr_mesh, decltype(openvdbr_api_v1::mesh_destroy)> mesh(result, api()->mesh_destroy);
    uint64_t vertices = 0, triangles = 0; const double* xyz = nullptr; const int32_t* indices = nullptr;
    check(api()->mesh_data(mesh.get(), &vertices, &xyz, &triangles, &indices, &error), error);
    if (vertices > INT_MAX || triangles > INT_MAX) Rcpp::stop("Mesh is too large for an R matrix");
    Rcpp::NumericMatrix points(vertices, 3); Rcpp::IntegerMatrix faces(triangles, 3);
    for (uint64_t i = 0; i < vertices; ++i) for (int a = 0; a < 3; ++a) points(i,a) = xyz[3*i+a];
    for (uint64_t i = 0; i < triangles; ++i) for (int a = 0; a < 3; ++a) faces(i,a) = indices[3*i+a] + 1;
    return Rcpp::List::create(Rcpp::Named("vertices") = points, Rcpp::Named("triangles") = faces);
    END_RCPP
}
const openvdbr_api_v1* get_api_v1(uint32_t version) { return version == 1 ? api() : nullptr; }
const openvdbr_volume_api_v1* get_volume_api_v1(uint32_t version) { return version == 1 ? openvdbr_provider_volume_api() : nullptr; }
}
extern "C" void attribute_visible R_init_openvdbr(DllInfo* dll) {
    const R_CallMethodDef methods[] = {
        {"C_vdb_version", (DL_FUNC)&r_version, 0}, {"C_vdb_api", (DL_FUNC)&r_api, 1},
        {"C_vdb_messages", (DL_FUNC)&r_messages, 0},
        {"C_vdb_grid", (DL_FUNC)&r_create, 5}, {"C_vdb_sphere", (DL_FUNC)&r_sphere, 5},
        {"C_vdb_clone", (DL_FUNC)&r_clone, 1}, {"C_vdb_close", (DL_FUNC)&r_close, 1},
        {"C_vdb_info", (DL_FUNC)&r_info, 1}, {"C_vdb_set", (DL_FUNC)&r_set, 4},
        {"C_vdb_get", (DL_FUNC)&r_get, 2}, {"C_vdb_sample", (DL_FUNC)&r_sample, 4},
        {"C_vdb_names", (DL_FUNC)&r_names, 1}, {"C_vdb_read", (DL_FUNC)&r_read, 2},
        {"C_vdb_write", (DL_FUNC)&r_write, 2}, {"C_vdb_csg", (DL_FUNC)&r_csg, 3},
        {"C_vdb_fog", (DL_FUNC)&r_fog, 1}, {"C_vdb_from_mesh", (DL_FUNC)&r_from_mesh, 5},
        {"C_vdb_to_mesh", (DL_FUNC)&r_to_mesh, 3}, {nullptr, nullptr, 0}
    };
    R_registerRoutines(dll, nullptr, methods, nullptr, nullptr);
    R_useDynamicSymbols(dll, FALSE);
    R_forceSymbols(dll, TRUE);
    R_RegisterCCallable("openvdbr", "openvdbr_get_api_v1", (DL_FUNC)&get_api_v1);
    R_RegisterCCallable("openvdbr", "openvdbr_get_volume_api_v1", (DL_FUNC)&get_volume_api_v1);
}
