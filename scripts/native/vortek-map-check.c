#include <stdio.h>
#include "dimensity_mapping.h"
#define CHECK(x) do { if (!(x)) { printf("mapping_failed_line=%d\n",__LINE__); return 1; } } while(0)
int main(void) {
    DGMapRange r;
    const uint64_t threeGiB = 3ull << 30;
    CHECK(dgMapRange(threeGiB, 0, UINT64_MAX, 4096, &r));
    CHECK(r.size == threeGiB && r.length == threeGiB && r.delta == 0);
    CHECK(dgMapRange(threeGiB, 64, UINT64_MAX, 4096, &r));
    CHECK(r.size == threeGiB - 64 && r.length == threeGiB && r.fileOffset == 0 && r.delta == 64);
    CHECK(dgMapRange(32768, 16448, 64, 16384, &r));
    CHECK(r.fileOffset == 16384 && r.delta == 64 && r.length == 128);
    CHECK(!dgMapRange(32768, 32768, UINT64_MAX, 4096, &r));
    CHECK(!dgMapRange(32768, 32704, 128, 4096, &r));
    CHECK(!dgMapRange(32768, 0, 0, 4096, &r));
    CHECK(!dgMapRange(UINT64_MAX, UINT64_MAX - 1, 1, 4096, &r));
    CHECK(!dgMapRange(UINT64_MAX, 0, UINT64_MAX, 0, &r));
    puts("vortek_mapping_result=0 (3GiB, offset/whole, 4K/16K pages, bounds)");
    return 0;
}
