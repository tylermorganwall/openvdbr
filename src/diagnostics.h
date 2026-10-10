#ifndef OPENVDBR_DIAGNOSTICS_H
#define OPENVDBR_DIAGNOSTICS_H
#include <stdint.h>
#include <openvdbr/openvdbr_api.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Worker-safe logging for bundled libraries; never calls R or writes stdio. */
int openvdbr_logf(const char* format, ...);
int32_t openvdbr_diagnostic_next(openvdbr_error* message);
#ifdef __cplusplus
}
#endif
#endif
