// Modified for openvdbr by Tyler Morgan-Wall, 2026: R embedding; see src/vendor/README.md.
// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: Apache-2.0
///
/// @file Platform.h

#include "Assert.h"
#include "diagnostics.h"

#include <cstdio>
#include <cstdlib>

namespace openvdb {
OPENVDB_USE_VERSION_NAMESPACE
namespace OPENVDB_VERSION_NAME {

[[noreturn]] void assertAbort(
    const char *assertion,
    const char *file,
    const unsigned line,
    const char *function,
    const char* msg)
{
    char message[1024];
    std::snprintf(message, sizeof(message), "%s:%u: assertion '%s' in %s: %s",
        file, line, assertion, function, msg ? msg : "");
    openvdbr_fatal(message);
}

}
}
