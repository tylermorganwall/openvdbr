#include <tbb/global_control.h>
#include <tbb/parallel_for.h>
#include <tbb/version.h>
#if TBB_INTERFACE_VERSION < 12000
#error oneTBB headers are required
#endif
int main() {
    tbb::global_control limit(tbb::global_control::max_allowed_parallelism, 2);
    int values[2] = {};
    tbb::parallel_for(0, 2, [&](int i) { values[i] = i; });
    return values[1] != 1;
}
