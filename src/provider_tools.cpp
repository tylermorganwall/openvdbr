#include "provider_internal.h"
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/tools/Composite.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/VolumeToMesh.h>

namespace openvdbr {
int32_t sphere(double radius, const double* center, double voxel_size, double half_width,
    const char* name, openvdbr_grid** out, openvdbr_error* error) {
    if (out) *out = nullptr;
    return guarded(error, [&] {
        require(out && center, "Output and center must not be NULL");
        validate_geometry(voxel_size, half_width, name);
        require(std::isfinite(radius) && radius >= std::numeric_limits<float>::min() && radius <= std::numeric_limits<float>::max(),
                "radius must be positive and representable as a float");
        for (int a = 0; a < 3; ++a)
            require(std::isfinite(center[a]) && std::abs(center[a]) <= std::numeric_limits<float>::max() &&
                    (std::abs(center[a]) + radius) / voxel_size + half_width < INT32_MAX - 2,
                    "Sphere exceeds OpenVDB's index range");
        initialize();
        auto grid = openvdb::tools::createLevelSetSphere<openvdb::FloatGrid>(
            radius, openvdb::Vec3f(center[0], center[1], center[2]), voxel_size, half_width);
        grid->setName(name);
        *out = new openvdbr_grid(std::move(grid));
    });
}
int32_t csg(const openvdbr_grid* a, const openvdbr_grid* b, int32_t operation,
    openvdbr_grid** out, openvdbr_error* error) {
    if (out) *out = nullptr;
    return guarded(error, [&] {
        require(a && b && out, "Inputs and output must not be NULL");
        require(a->grid->isType<openvdb::FloatGrid>() && b->grid->isType<openvdb::FloatGrid>() &&
                a->grid->getGridClass() == openvdb::GRID_LEVEL_SET && b->grid->getGridClass() == openvdb::GRID_LEVEL_SET,
                "CSG requires two float level sets");
        require(a->grid->transform() == b->grid->transform(), "CSG grids must have identical transforms");
        require(operation >= OPENVDBR_UNION && operation <= OPENVDBR_DIFFERENCE, "Unknown CSG operation");
        const auto& source_a = static_cast<const openvdb::FloatGrid&>(*a->grid);
        const auto& source_b = static_cast<const openvdb::FloatGrid&>(*b->grid);
        require(source_a.background() == source_b.background(), "CSG grids must have identical band widths");
        // OpenVDB's in-place CSG consumes the second tree. Clone both operands
        // so R aliases and other consumers retain their original data.
        auto result = source_a.deepCopy(), other = source_b.deepCopy();
        if (operation == OPENVDBR_UNION) openvdb::tools::csgUnion(*result, *other);
        else if (operation == OPENVDBR_INTERSECTION) openvdb::tools::csgIntersection(*result, *other);
        else openvdb::tools::csgDifference(*result, *other);
        *out = new openvdbr_grid(std::move(result));
    });
}
int32_t fog(const openvdbr_grid* grid, openvdbr_grid** out, openvdbr_error* error) {
    if (out) *out = nullptr;
    return guarded(error, [&] {
        require(grid && out, "Grid and output must not be NULL");
        require(grid->grid->isType<openvdb::FloatGrid>() && grid->grid->getGridClass() == openvdb::GRID_LEVEL_SET,
                "Fog conversion requires a float level set");
        auto result = static_cast<const openvdb::FloatGrid&>(*grid->grid).deepCopy();
        openvdb::tools::sdfToFogVolume(*result);
        *out = new openvdbr_grid(std::move(result));
    });
}
int32_t mesh_to_grid(uint64_t vertices, const double* xyz, uint64_t triangles, const int32_t* indices,
    double voxel_size, double half_width, const char* name, openvdbr_grid** out, openvdbr_error* error) {
    if (out) *out = nullptr;
    return guarded(error, [&] {
        require(out && xyz && indices && vertices >= 3 && triangles > 0, "A nonempty triangle mesh is required");
        require(vertices <= INT32_MAX && triangles <= INT32_MAX, "Mesh exceeds the supported index range");
        validate_geometry(voxel_size, half_width, name);
        std::vector<openvdb::Vec3s> points(vertices);
        std::vector<openvdb::Vec3I> faces(triangles);
        for (uint64_t i = 0; i < vertices; ++i) for (int a = 0; a < 3; ++a) {
            double value = xyz[3*i+a];
            require(std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max() &&
                    std::abs(value / voxel_size) + half_width < INT32_MAX - 2, "Mesh vertices exceed the supported range");
            points[i][a] = static_cast<float>(value);
        }
        for (uint64_t i = 0; i < triangles; ++i) for (int a = 0; a < 3; ++a) {
            int32_t index = indices[3*i+a];
            require(index >= 0 && uint64_t(index) < vertices, "Triangle index is outside the vertex array");
            faces[i][a] = index;
        }
        initialize();
        auto transform = openvdb::math::Transform::createLinearTransform(voxel_size);
        auto result = openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(*transform, points, faces, half_width);
        result->setName(name);
        *out = new openvdbr_grid(std::move(result));
    });
}
int32_t grid_to_mesh(const openvdbr_grid* grid, double isovalue, double adaptivity,
    openvdbr_mesh** out, openvdbr_error* error) {
    if (out) *out = nullptr;
    return guarded(error, [&] {
        require(grid && out, "Grid and output must not be NULL");
        require(std::isfinite(isovalue), "isovalue must be finite");
        require(std::isfinite(adaptivity) && adaptivity >= 0 && adaptivity <= 1, "adaptivity must be between zero and one");
        std::vector<openvdb::Vec3s> points;
        std::vector<openvdb::Vec3I> triangles;
        std::vector<openvdb::Vec4I> quads;
        if (grid->grid->isType<openvdb::FloatGrid>())
            openvdb::tools::volumeToMesh(static_cast<const openvdb::FloatGrid&>(*grid->grid), points, triangles, quads, isovalue, adaptivity);
        else if (grid->grid->isType<openvdb::DoubleGrid>())
            openvdb::tools::volumeToMesh(static_cast<const openvdb::DoubleGrid&>(*grid->grid), points, triangles, quads, isovalue, adaptivity);
        else throw Failure(OPENVDBR_UNSUPPORTED, "Meshing requires a float or double scalar grid");
        require(points.size() <= INT32_MAX, "Mesh exceeds the supported index range");
        auto result = std::make_unique<openvdbr_mesh>();
        result->vertices.reserve(3 * points.size());
        result->triangles.reserve(3 * triangles.size() + 6 * quads.size());
        for (const auto& point : points) for (int a = 0; a < 3; ++a) result->vertices.push_back(point[a]);
        for (const auto& face : triangles) for (int a = 0; a < 3; ++a) result->triangles.push_back(face[a]);
        for (const auto& face : quads) for (int a : {0, 1, 2, 0, 2, 3}) result->triangles.push_back(face[a]);
        *out = result.release();
    });
}
}
