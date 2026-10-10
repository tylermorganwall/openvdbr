// Regenerate with the package's built core and its private OpenVDB headers.
// This uses upstream C++ APIs directly, independently of the provider wrappers.
#include <openvdb/openvdb.h>
#include <openvdb/io/File.h>
#include <cmath>
int main(int argc, char** argv) {
    if (argc != 2) return 1;
    openvdb::initialize();
    auto scalar = openvdb::FloatGrid::create(0);
    scalar->setName("rotated");
    auto transform = openvdb::math::Transform::createLinearTransform(.5);
    transform->postRotate(std::acos(-1.) / 2, openvdb::math::Z_AXIS);
    transform->postTranslate(openvdb::Vec3d(3, 4, 5));
    scalar->setTransform(transform);
    scalar->tree().setValue(openvdb::Coord(2, 0, 0), 7);
    scalar->insertMeta("description", openvdb::StringMetadata("direct C++ fixture"));
    auto vector = openvdb::Vec3fGrid::create(openvdb::Vec3f(1, 2, 3));
    vector->setName("velocity");
    vector->tree().setValue(openvdb::Coord(0, 0, 0), openvdb::Vec3f(4, 5, 6));
    openvdb::GridPtrVec grids{scalar, vector};
    openvdb::io::File file(argv[1]);
    file.write(grids);
    return 0;
}
