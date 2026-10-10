/* Copyright 2026 Tyler Morgan-Wall. SPDX-License-Identifier: Apache-2.0 */
#ifndef OPENVDBR_R_H
#define OPENVDBR_R_H
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include "openvdbr_api.h"
/* Main R thread only. Load openvdbr first (e.g. Imports); keep its namespace
 * loaded while any native handles or function pointers remain in use.
 * Explicit dyn.unload() with live consumers is unsupported.
 */
static inline const openvdbr_api_v1* openvdbr_api_get_v1(void) {
    openvdbr_get_api_v1_fn get_api = (openvdbr_get_api_v1_fn)
        R_GetCCallable("openvdbr", "openvdbr_get_api_v1");
    const openvdbr_api_v1* api = get_api(1);
    if (!api || api->abi_version != 1 || api->struct_size < OPENVDBR_API_V1_SIZE)
        Rf_error("Incompatible openvdbr runtime API");
    return api;
}
static inline const openvdbr_api_v1* openvdbr_api_from_R_v1(SEXP handle) {
    const openvdbr_api_v1* api;
    if (TYPEOF(handle) != EXTPTRSXP ||
        R_ExternalPtrTag(handle) != Rf_install("openvdbr.api.v1"))
        Rf_error("Expected an openvdbr::vdb_api() handle");
    api = (const openvdbr_api_v1*)R_ExternalPtrAddr(handle);
    if (!api || api->abi_version != 1 || api->struct_size < OPENVDBR_API_V1_SIZE)
        Rf_error("Incompatible or expired openvdbr runtime API");
    return api;
}
/* Borrow the grid owned by an R handle. Keep that SEXP protected/reachable
 * throughout use; never grid_destroy() this borrowed pointer. If it must
 * outlive the R object, create an owned copy with grid_clone(). */
static inline openvdbr_grid* openvdbr_grid_from_R_v1(SEXP handle) {
    openvdbr_grid* grid;
    if (TYPEOF(handle) != EXTPTRSXP ||
        R_ExternalPtrTag(handle) != Rf_install("openvdbr.grid.v1"))
        Rf_error("Expected an openvdbr grid handle");
    grid = (openvdbr_grid*)R_ExternalPtrAddr(handle);
    if (!grid) Rf_error("Grid is closed or was serialized");
    return grid;
}
#endif
