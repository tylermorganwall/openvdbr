#include "diagnostics.h"
#include <tbb/task_group.h>
#include <tbb/global_control.h>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>
int main() {
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) workers.emplace_back([] {
        for (int j = 0; j < 50; ++j) openvdbr_logf("worker diagnostic %d", j);
    });
    for (auto& worker : workers) worker.join();
    openvdbr_error message{};
    int count = 0;
    bool overflow = false;
    while (openvdbr_diagnostic_next(&message)) {
        ++count;
        overflow = overflow || std::strstr(message.message, "72 additional") != nullptr;
    }
    if (count != 129 || !overflow) return 1;
    tbb::global_control limit(tbb::global_control::max_allowed_parallelism, 2);
    bool caught = false;
    try {
        tbb::task_group tasks;
        tasks.run([] { openvdbr_fatal("test failure"); });
        tasks.wait();
    } catch (const std::exception& error) {
        caught = std::strstr(error.what(), "test failure") != nullptr;
    }
    if (!caught) return 2;
    try { openvdbr_check_failure(); return 3; }
    catch (const std::exception& error) {
        if (!std::strstr(error.what(), "restart R")) return 4;
    }
    return 0;
}
