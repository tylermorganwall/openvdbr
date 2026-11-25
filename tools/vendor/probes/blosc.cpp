#include <blosc.h>

// OpenVDB 13.1's minimum; Blosc2 is a separate API, not a substitute here.
#if BLOSC_VERSION_MAJOR != 1 || BLOSC_VERSION_MINOR < 17
#error Blosc 1.17.0 or newer in the 1.x series is required
#endif

int main()
{
    char input[32] = {};
    char compressed[32 + BLOSC_MAX_OVERHEAD] = {};
    char output[32] = {};
    size_t size = 0;
    int count = blosc_compress_ctx(5, BLOSC_SHUFFLE, 1, sizeof(input), input,
        compressed, sizeof(compressed), BLOSC_LZ4_COMPNAME, 0, 1);
    if (count <= 0) return 1;
    if (blosc_cbuffer_validate(compressed, count, &size) != 0) return 2;
    return blosc_decompress_ctx(compressed, output, sizeof(output), 1) < 0;
}
