#ifndef OPENVDBR_INTERNAL_H
#define OPENVDBR_INTERNAL_H
#include "provider.h"
#include <openvdb/openvdb.h>
#include <openvdb/io/File.h>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

struct openvdbr_grid {
    openvdb::GridBase::Ptr grid;
    std::string name, type;
    explicit openvdbr_grid(openvdb::GridBase::Ptr value)
        : grid(std::move(value)), name(grid->getName()), type(grid->valueType()) {}
};
struct openvdbr_file {
    openvdb::io::File file;
    std::vector<std::string> names;
    explicit openvdbr_file(const char* path) : file(path) {}
};
struct openvdbr_mesh {
    std::vector<double> vertices;
    std::vector<int32_t> triangles;
};

namespace openvdbr {
class Failure : public std::runtime_error {
public:
    int32_t status;
    Failure(int32_t code, const char* text) : std::runtime_error(text), status(code) {}
};
inline void require(bool condition, const char* message) {
    if (!condition) throw Failure(OPENVDBR_INVALID, message);
}
inline void error_message(openvdbr_error* error, const char* message) noexcept {
    if (error) {
        std::strncpy(error->message, message, sizeof(error->message) - 1);
        error->message[sizeof(error->message) - 1] = '\0';
    }
}
// This is the exception boundary for every fallible C operation. No exceptions,
// R calls or provider-owned temporary allocations escape into a consumer.
template<class Function> int32_t guarded(openvdbr_error* error, Function function) noexcept {
    if (error) error->message[0] = '\0';
    try {
        function();
        return OPENVDBR_OK;
    } catch (const Failure& failure) {
        error_message(error, failure.what()); return failure.status;
    } catch (const openvdb::IoError& failure) {
        error_message(error, failure.what()); return OPENVDBR_IO;
    } catch (const std::bad_alloc&) {
        error_message(error, "OpenVDB allocation failed"); return OPENVDBR_MEMORY;
    } catch (const std::exception& failure) {
        error_message(error, failure.what()); return OPENVDBR_INTERNAL;
    } catch (...) {
        error_message(error, "Unknown OpenVDB error"); return OPENVDBR_INTERNAL;
    }
}
void initialize();
void validate_geometry(double voxel_size, double half_width, const char* name);
int32_t sphere(double, const double*, double, double, const char*, openvdbr_grid**, openvdbr_error*);
int32_t csg(const openvdbr_grid*, const openvdbr_grid*, int32_t, openvdbr_grid**, openvdbr_error*);
int32_t fog(const openvdbr_grid*, openvdbr_grid**, openvdbr_error*);
int32_t mesh_to_grid(uint64_t, const double*, uint64_t, const int32_t*, double,
                     double, const char*, openvdbr_grid**, openvdbr_error*);
int32_t grid_to_mesh(const openvdbr_grid*, double, double, openvdbr_mesh**, openvdbr_error*);
}
#endif
