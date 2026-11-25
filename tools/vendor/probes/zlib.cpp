#include <zlib.h>
int main() {
    unsigned char input[16] = {}, output[64] = {};
    uLongf size = sizeof(output);
    return compress(output, &size, input, sizeof(input)) != Z_OK;
}
