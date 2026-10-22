#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#include "dimensity_upload.h"
#include "thread_pool.h"
#include "bc_decoder.h"

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "upload check failed at line %d\n", __LINE__); return 1; } } while (0)

int main(void) {
    VkMemoryType types[3] = {
        {.propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT},
        {.propertyFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT},
        {.propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT}
    };
    VkMemoryPropertyFlags required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    CHECK(dgSelectMemoryType(types, 3, 7, required) == 2);
    CHECK(dgSelectMemoryType(types, 3, 3, required) == UINT32_MAX);

    VkBufferImageCopy region = {0};
    region.bufferOffset = 5;
    region.bufferRowLength = region.bufferImageHeight = 12;
    region.imageExtent = (VkExtent3D){5, 5, 1};
    region.imageSubresource.layerCount = 2;
    DGBCUpload layout;
    CHECK(dgBCUploadLayout(&region, 8, 3, 120, &layout));
    CHECK(layout.offset == 8 && layout.rowBytes == 16 && layout.rowStride == 24 &&
          layout.sliceStride == 72 && layout.slices == 2 && layout.packedSize == 64 && layout.decodedSize == 200);
    uint8_t source[120], packed[80];
    memset(source, 0xee, sizeof(source));
    memset(packed, 0xcc, sizeof(packed));
    const unsigned offsets[] = {8, 32, 80, 104};
    for (unsigned i = 0; i < 4; i++) memset(source + offsets[i], 0x11 * (i + 1), 16);
    dgPackBCUpload(source, &layout, packed + 8);
    for (unsigned i = 0; i < sizeof(packed); i++)
        CHECK(packed[i] == (i < 8 || i >= 72 ? 0xcc : 0x11 * ((i - 8) / 16 + 1)));
    CHECK(!dgBCUploadLayout(&region, 8, 3, 119, &layout));
    CHECK(!dgBCUploadLayout(&region, 8, UINT64_MAX, UINT64_MAX, &layout));
    region.bufferRowLength = 6;
    CHECK(!dgBCUploadLayout(&region, 8, 3, sizeof(source), &layout));

    // Four worker chunks must start at successive 8-byte BC4 blocks.
    // Each block encodes a different constant red, independent of padding.
    uint8_t blocks[512] = {0}, pixels[4 * 128 * 4] = {0};
    for (unsigned i = 0; i < 32; i++) blocks[i * 8] = 17 + i;
    ThreadPool* pool = ThreadPool_init(4);
    BCDecoder_decode(blocks, pixels, 4, 128, 4, true, pool);
    ThreadPool_destroy(pool);
    for (unsigned y = 0; y < 128; y++)
        for (unsigned x = 0; x < 4; x++) CHECK(pixels[(y * 4 + x) * 4] == 17 + y / 4);
    puts("vortek_upload_result=0 (memory flags, padded array upload, BC4 worker offsets)");
    return 0;
}
