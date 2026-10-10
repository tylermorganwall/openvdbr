#include "diagnostics.h"
#include <array>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <limits>

namespace {
// A fixed-size queue keeps diagnostics from becoming an unbounded allocation
// on error paths. Preserve the first messages and report any overflow on drain.
struct Diagnostics {
    std::mutex mutex;
    std::array<openvdbr_error, 128> messages{};
    size_t first = 0, count = 0;
    uint64_t dropped = 0;
};
Diagnostics& diagnostics() { static Diagnostics state; return state; }
}
extern "C" int openvdbr_logf(const char* format, ...) {
    openvdbr_error message{};
    va_list args;
    va_start(args, format);
    int written = std::vsnprintf(message.message, sizeof(message.message), format, args);
    va_end(args);
    try {
        auto& state = diagnostics();
        std::lock_guard<std::mutex> lock(state.mutex);
        if (state.count < state.messages.size()) {
            state.messages[(state.first + state.count) % state.messages.size()] = message;
            ++state.count;
        } else if (state.dropped < std::numeric_limits<uint64_t>::max()) ++state.dropped;
    } catch (...) {
        // Logging must never turn a recoverable library error into an exception
        // crossing the C ABI (or call into R from a worker thread).
    }
    return written;
}
extern "C" int32_t openvdbr_diagnostic_next(openvdbr_error* message) {
    if (!message) return 0;
    message->message[0] = '\0';
    try {
        auto& state = diagnostics();
        std::lock_guard<std::mutex> lock(state.mutex);
        if (state.count) {
            *message = state.messages[state.first];
            state.first = (state.first + 1) % state.messages.size();
            --state.count;
            return 1;
        }
        if (state.dropped) {
            std::snprintf(message->message, sizeof(message->message),
                "%llu additional library diagnostics were dropped (queue capacity 128).",
                static_cast<unsigned long long>(state.dropped));
            state.dropped = 0;
            return 1;
        }
    } catch (...) {}
    return 0;
}
