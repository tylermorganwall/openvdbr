#define R_NO_REMAP
#include <openvdbr/openvdbr_r.h>
/* A C translation unit verifies that neither C++ nor OpenVDB headers leak
 * through the public ABI. Lookup must happen on the R thread. */
int openvdbr_c_client(void) {
    openvdbr_get_api_v1_fn getter = (openvdbr_get_api_v1_fn)
        R_GetCCallable("openvdbr", "openvdbr_get_api_v1");
    const openvdbr_api_v1* api = openvdbr_api_get_v1();
    return api && api == getter(1) && getter(2) == NULL &&
        api->struct_size >= OPENVDBR_API_V1_SIZE && api->grid_sample && api->mesh_data;
}
