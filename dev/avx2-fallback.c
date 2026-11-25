#include "shuffle-avx2.h"
#include "shuffle-generic.h"
#include <string.h>
int main(void) {
    unsigned char input[103], shuffled[103], generic[103], restored[103];
    const size_t sizes[] = {1, 2, 3, 8};
    for (size_t i = 0; i < sizeof(input); ++i) input[i] = (unsigned char)(3*i + 17);
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); ++i) {
        blosc_internal_shuffle_avx2(sizes[i], sizeof(input), input, shuffled);
        blosc_internal_shuffle_generic(sizes[i], sizeof(input), input, generic);
        if (memcmp(shuffled, generic, sizeof(input))) return 1;
        blosc_internal_unshuffle_avx2(sizes[i], sizeof(input), shuffled, restored);
        if (memcmp(input, restored, sizeof(input))) return 2;
    }
    return 0;
}
