#include "diagnostics.h"
#include <cstring>
#include <thread>
#include <vector>
int main() {
    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) workers.emplace_back([] {
        for (int i = 0; i < 100; ++i) openvdbr_logf("message %d", i);
    });
    for (auto& worker : workers) worker.join();
    int count = 0;
    openvdbr_error message{};
    while (openvdbr_diagnostic_next(&message)) {
        ++count;
        if (count <= 128 && std::strncmp(message.message, "message ", 8)) return 1;
        if (count == 129 && !std::strstr(message.message, "272 additional")) return 2;
    }
    if (count != 129 || message.message[0] != '\0') return 3;
    openvdbr_logf("%0600d", 1);
    if (!openvdbr_diagnostic_next(&message) || std::strlen(message.message) != 511) return 4;
    return 0;
}
