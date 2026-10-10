/* Copyright 2026 Tyler Morgan-Wall. SPDX-License-Identifier: Apache-2.0 */
#ifndef OPENVDBR_VOLUME_API_H
#define OPENVDBR_VOLUME_API_H
#include "openvdbr_api.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct openvdbr_volume_buffer openvdbr_volume_buffer;
typedef struct {
    uint32_t abi_version, struct_size, nanovdb_major;
    /* Read a float grid from OpenVDB or raw/NONE/ZIP/BLOSC NanoVDB. Native VDB
     * grids are converted once, preserving their affine transform and sparsity.
     * optional=1 permits a missing name, returning OK with *out=NULL.
     * Returned storage is aligned, immutable and owned by the provider. */
    int32_t (*read_float_grid)(const char* path, const char* name, int32_t optional,
        openvdbr_volume_buffer** out, openvdbr_error* error);
    void (*destroy)(openvdbr_volume_buffer* buffer);
    int32_t (*data)(const openvdbr_volume_buffer* buffer, const void** bytes,
        uint64_t* size, openvdbr_error* error);
} openvdbr_volume_api_v1;
typedef const openvdbr_volume_api_v1* (*openvdbr_get_volume_api_v1_fn)(uint32_t version);
#ifdef __cplusplus
}
#endif
#endif
