#define R_NO_REMAP
#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h>
#include <openvdbr/openvdbr_r.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <thread>
#include <vector>

extern "C" int openvdbr_c_client(void);
namespace {
SEXP check_handle(SEXP table_handle, SEXP grid_handle) {
    const auto* api = openvdbr_api_from_R_v1(table_handle);
    auto* grid = openvdbr_grid_from_R_v1(grid_handle);
    const double point[] = {0, 0, 0};
    double value = 0;
    openvdbr_error error{};
    if (api->grid_sample(grid, 1, point, 0, OPENVDBR_NEAREST, &value, &error))
        Rf_error("%s", error.message);
    return Rf_ScalarReal(value);
}
SEXP check_runtime() {
    const auto* api = openvdbr_api_get_v1();
    if (!openvdbr_c_client()) Rf_error("C ABI negotiation failed");
    openvdbr_error error{};
    openvdbr_grid* grid = nullptr;
    const double origin[] = {2, 3, 4};
    int status = api->grid_create(OPENVDBR_FLOAT, 0, .5, origin, "density", &grid, &error);
    if (status) Rf_error("%s", error.message);
    int failures = 0;
    const int32_t indices[] = {0, 0, 0, 1, 0, 0};
    const double values[] = {2, 4};
    if (api->grid_set(grid, 2, indices, values, nullptr, &error)) ++failures;
    const double position[] = {2.25, 3, 4};
    double sample = 0;
    if (api->grid_sample(grid, 1, position, 1, OPENVDBR_LINEAR, &sample, &error) || sample != 3) ++failures;

    // A failed batch must not change its first, otherwise valid, element.
    const double invalid[] = {9, INFINITY};
    if (api->grid_set(grid, 2, indices, invalid, nullptr, &error) != OPENVDBR_INVALID ||
        !std::strlen(error.message)) ++failures;
    double readback[2]{};
    if (api->grid_get(grid, 2, indices, readback, nullptr, &error) || readback[0] != 2 || readback[1] != 4 ||
        error.message[0] != '\0') ++failures;
    if (api->grid_sample(nullptr, 1, position, 1, OPENVDBR_LINEAR, &sample, &error) != OPENVDBR_INVALID) ++failures;
    if (api->grid_sample(grid, 1, position, 1, 99, &sample, &error) != OPENVDBR_INVALID) ++failures;
    if (api->grid_sample(grid, 0, nullptr, 1, OPENVDBR_LINEAR, nullptr, &error)) ++failures;

    // Obtain all function pointers on the R thread; workers only call the C ABI.
    std::atomic<int> thread_errors{0};
    std::vector<std::thread> threads;
    try {
        for (int t = 0; t < 4; ++t) threads.emplace_back([&] {
            openvdbr_error local{};
            for (int i = 0; i < 2000; ++i) {
                double result = 0;
                if (api->grid_sample(grid, 1, position, 1, OPENVDBR_LINEAR, &result, &local) ||
                    result != 3 || local.message[0] != '\0') ++thread_errors;
            }
        });
    } catch (...) { ++thread_errors; }
    for (auto& thread : threads) thread.join();
    failures += thread_errors.load();

    openvdbr_grid* copy = nullptr;
    if (api->grid_clone(grid, &copy, &error)) ++failures;
    api->grid_destroy(grid);
    if (copy) {
        if (api->grid_sample(copy, 1, position, 1, OPENVDBR_LINEAR, &sample, &error) || sample != 3) ++failures;
        api->grid_destroy(copy);
    }
    api->grid_destroy(nullptr);
    if (failures) Rf_error("Native consumer reported %d failures", failures);
    return Rf_ScalarInteger(8000);
}
}
extern "C" void attribute_visible R_init_openvdbrconsumer(DllInfo* dll) {
    const R_CallMethodDef methods[] = {
        {"C_check_runtime", (DL_FUNC)&check_runtime, 0},
        {"C_check_handle", (DL_FUNC)&check_handle, 2}, {nullptr, nullptr, 0}
    };
    R_registerRoutines(dll, nullptr, methods, nullptr, nullptr);
    R_useDynamicSymbols(dll, FALSE);
    R_forceSymbols(dll, TRUE);
}
