#ifndef DIMENSITY_MAPPING_H
#define DIMENSITY_MAPPING_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Vulkan allows an offset smaller than an OS page; mmap does not. The guest
// pointer is inside the aligned mapping, whose base/length must be unmapped.
typedef struct DGMapRange {
    uint64_t fileOffset;
    size_t length;
    size_t delta;
    uint64_t size;
} DGMapRange;

static inline bool dgMapRange(uint64_t allocation, uint64_t offset, uint64_t size,
        uint64_t pageSize, DGMapRange* out) {
    if (!out || !pageSize || !allocation || offset >= allocation) return false;
    uint64_t remaining = allocation - offset;
    if (size == UINT64_MAX) size = remaining;
    if (!size || size > remaining) return false;
    uint64_t delta = offset % pageSize;
    uint64_t fileOffset = offset - delta;
    if (fileOffset > INT64_MAX || delta > SIZE_MAX || size > SIZE_MAX - delta) return false;
    *out = (DGMapRange){fileOffset, (size_t)(size + delta), (size_t)delta, size};
    return true;
}
#endif
