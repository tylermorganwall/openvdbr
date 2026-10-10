/* Copyright 2026 Tyler Morgan-Wall. SPDX-License-Identifier: Apache-2.0
 * Pure C ABI: no OpenVDB, STL or R objects cross this boundary.
 * See runtime-api documentation for ownership, layouts and threading rules.
 */
#ifndef OPENVDBR_API_H
#define OPENVDBR_API_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct openvdbr_grid openvdbr_grid;
typedef struct openvdbr_file openvdbr_file;
typedef struct openvdbr_mesh openvdbr_mesh;
typedef struct { char message[512]; } openvdbr_error;
enum openvdbr_status {
    OPENVDBR_OK = 0, OPENVDBR_INVALID = 1, OPENVDBR_IO = 2,
    OPENVDBR_UNSUPPORTED = 3, OPENVDBR_MEMORY = 4, OPENVDBR_INTERNAL = 5
};
enum openvdbr_value_type { OPENVDBR_OTHER = 0, OPENVDBR_FLOAT = 1, OPENVDBR_DOUBLE = 2 };
enum openvdbr_interpolation { OPENVDBR_NEAREST = 0, OPENVDBR_LINEAR = 1 };
enum openvdbr_csg { OPENVDBR_UNION = 0, OPENVDBR_INTERSECTION = 1, OPENVDBR_DIFFERENCE = 2 };
typedef struct {
    /* Strings belong to the grid handle; valid until its destruction. */
    const char* name;
    const char* value_type;
    int32_t scalar_type;
    int32_t grid_class; /* OpenVDB: 0 unknown, 1 level set, 2 fog, 3 staggered. */
    uint64_t active_voxels, memory_bytes;
    int32_t empty;
    int32_t index_min[3], index_max[3]; /* inclusive; zero when empty */
    double voxel_size[3], origin[3], background;
} openvdbr_grid_info_v1;
typedef struct {
    uint32_t abi_version, struct_size;
    const char* (*version)(void);
    int32_t (*grid_create)(int32_t type, double background, double voxel_size,
        const double origin[3], const char* name, openvdbr_grid** out, openvdbr_error* error);
    void (*grid_destroy)(openvdbr_grid* grid);
    int32_t (*grid_clone)(const openvdbr_grid* grid, openvdbr_grid** out, openvdbr_error* error);
    int32_t (*grid_info)(const openvdbr_grid* grid, openvdbr_grid_info_v1* out, openvdbr_error* error);
    /* Coordinates are interleaved x,y,z triples in zero-based index space.
     * active may be NULL: all values are activated on set, ignored on get.
     * Setting inactive values preserves them (e.g. negative level-set interiors).
     */
    int32_t (*grid_set)(openvdbr_grid* grid, uint64_t count, const int32_t* xyz,
        const double* values, const uint8_t* active, openvdbr_error* error);
    int32_t (*grid_get)(const openvdbr_grid* grid, uint64_t count, const int32_t* xyz,
        double* values, uint8_t* active, openvdbr_error* error);
    int32_t (*grid_sample)(const openvdbr_grid* grid, uint64_t count, const double* xyz,
        int32_t world_space, int32_t interpolation, double* values, openvdbr_error* error);
    int32_t (*file_open)(const char* path, openvdbr_file** out, openvdbr_error* error);
    void (*file_destroy)(openvdbr_file* file);
    uint64_t (*file_count)(const openvdbr_file* file);
    int32_t (*file_name)(const openvdbr_file* file, uint64_t index,
        const char** name, openvdbr_error* error);
    int32_t (*file_read)(openvdbr_file* file, uint64_t index,
        openvdbr_grid** out, openvdbr_error* error);
    int32_t (*file_write)(const char* path, uint64_t count,
        const openvdbr_grid* const* grids, openvdbr_error* error);
    int32_t (*level_set_sphere)(double radius, const double center[3],
        double voxel_size, double half_width, const char* name,
        openvdbr_grid** out, openvdbr_error* error);
    int32_t (*level_set_csg)(const openvdbr_grid* a, const openvdbr_grid* b,
        int32_t operation, openvdbr_grid** out, openvdbr_error* error);
    int32_t (*level_set_fog)(const openvdbr_grid* grid,
        openvdbr_grid** out, openvdbr_error* error);
    /* Mesh vertices are interleaved world-space triples; triangle indices are
     * zero-based. Mesh buffers remain provider-owned and immutable.
     */
    int32_t (*mesh_to_level_set)(uint64_t vertices, const double* xyz,
        uint64_t triangles, const int32_t* indices, double voxel_size,
        double half_width, const char* name, openvdbr_grid** out, openvdbr_error* error);
    int32_t (*grid_to_mesh)(const openvdbr_grid* grid, double isovalue,
        double adaptivity, openvdbr_mesh** out, openvdbr_error* error);
    void (*mesh_destroy)(openvdbr_mesh* mesh);
    int32_t (*mesh_data)(const openvdbr_mesh* mesh, uint64_t* vertices,
        const double** xyz, uint64_t* triangles, const int32_t** indices,
        openvdbr_error* error);
    /* Drain one buffered upstream diagnostic: 1 if available, 0 otherwise.
     * Thread-safe, but messages are global to the provider, not per grid. */
    int32_t (*diagnostic_next)(openvdbr_error* message);
} openvdbr_api_v1;

typedef const openvdbr_api_v1* (*openvdbr_get_api_v1_fn)(uint32_t version);
#define OPENVDBR_API_V1_SIZE ((uint32_t)sizeof(openvdbr_api_v1))

#ifdef __cplusplus
}
#endif
#endif
