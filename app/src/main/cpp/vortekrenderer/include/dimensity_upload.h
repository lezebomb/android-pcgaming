#ifndef DIMENSITY_VORTEK_UPLOAD_H
#define DIMENSITY_VORTEK_UPLOAD_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "vulkan/vulkan.h"

static inline uint32_t dgSelectMemoryType(const VkMemoryType* types, uint32_t count,
                                         uint32_t allowed, VkMemoryPropertyFlags required) {
    for (uint32_t i = 0; i < count && i < 32; i++)
        if ((allowed & (1u << i)) && (types[i].propertyFlags & required) == required) return i;
    return UINT32_MAX;
}

typedef struct DGBCUpload {
    uint64_t offset, rowBytes, rowStride, rows, sliceStride, slices;
    uint64_t packedSize, decodedSize;
} DGBCUpload;

static inline bool dgMul64(uint64_t a, uint64_t b, uint64_t* out) {
    if (a && b > UINT64_MAX / a) return false;
    *out = a * b;
    return true;
}

static inline bool dgBCUploadLayout(const VkBufferImageCopy* region, uint32_t blockBytes,
                                    uint64_t memoryOffset, uint64_t allocationSize, DGBCUpload* out) {
    uint64_t width = region->imageExtent.width, height = region->imageExtent.height;
    uint64_t depth = region->imageExtent.depth, layers = region->imageSubresource.layerCount;
    if (!width || !height || width > UINT16_MAX || height > UINT16_MAX || !depth || !layers ||
        (blockBytes != 8 && blockBytes != 16)) return false;
    if ((region->bufferRowLength && (region->bufferRowLength & 3)) ||
        (region->bufferImageHeight && (region->bufferImageHeight & 3))) return false;
    uint64_t rowLength = region->bufferRowLength ? region->bufferRowLength : width;
    uint64_t imageHeight = region->bufferImageHeight ? region->bufferImageHeight : height;
    if (rowLength < width || imageHeight < height || memoryOffset > allocationSize ||
        region->bufferOffset > allocationSize - memoryOffset) return false;
    out->offset = memoryOffset + region->bufferOffset;
    out->rows = (height + 3) / 4;
    if (!dgMul64((width + 3) / 4, blockBytes, &out->rowBytes) ||
        !dgMul64((rowLength + 3) / 4, blockBytes, &out->rowStride) ||
        !dgMul64((imageHeight + 3) / 4, out->rowStride, &out->sliceStride) ||
        !dgMul64(depth, layers, &out->slices) ||
        !dgMul64(out->rowBytes * out->rows, out->slices, &out->packedSize) ||
        !dgMul64(width * height * 4, out->slices, &out->decodedSize) ||
        out->decodedSize > UINT32_MAX) return false;
    uint64_t lastSlice, lastRow;
    if (!dgMul64(out->slices - 1, out->sliceStride, &lastSlice) ||
        !dgMul64(out->rows - 1, out->rowStride, &lastRow)) return false;
    uint64_t available = allocationSize - out->offset;
    if (lastSlice > available) return false;
    available -= lastSlice;
    if (lastRow > available) return false;
    return out->rowBytes <= available - lastRow;
}

static inline void dgPackBCUpload(const void* source, const DGBCUpload* layout, void* packed) {
    const uint8_t* input = (const uint8_t*)source + layout->offset;
    uint8_t* output = packed;
    for (uint64_t slice = 0; slice < layout->slices; slice++)
        for (uint64_t row = 0; row < layout->rows; row++) {
            memcpy(output, input + slice * layout->sliceStride + row * layout->rowStride, layout->rowBytes);
            output += layout->rowBytes;
        }
}

#endif
