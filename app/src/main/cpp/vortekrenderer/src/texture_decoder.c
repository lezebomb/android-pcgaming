#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "texture_decoder.h"
#include "vulkan_helper.h"
#include "bc_decoder.h"
#include "string_utils.h"
#include "file_utils.h"
#include "dimensity_upload.h"

#define CACHE_DIR vortekCacheDir
#define CACHE_MIN_IMAGE_WIDTH 1024

#define VK_FORMAT_BC_FLAG(f) (1 << (f - VK_FORMAT_BC1_RGB_UNORM_BLOCK))

bool TextureDecoder_canDecompressFormat(VkFormat format) {
    switch (format) {
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC4_UNORM_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
            return true;
        default:
            return false;
    }
}

VkFormat TextureDecoder_decodedFormat(VkFormat format) {
    switch (format) {
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK: return VK_FORMAT_B8G8R8A8_SRGB;
        case VK_FORMAT_BC4_SNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK: return VK_FORMAT_R8G8B8A8_SNORM;
        case VK_FORMAT_BC4_UNORM_BLOCK:
        case VK_FORMAT_BC5_UNORM_BLOCK: return VK_FORMAT_R8G8B8A8_UNORM;
        default: return DECOMPRESSED_FORMAT;
    }
}

static void getBCInfo(VkFormat format, int* bcN, int* blockSize, bool* isNoAlphaU) {
    *isNoAlphaU = false;
    switch (format) {
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            *isNoAlphaU = true;
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            *blockSize = 8;
            *bcN = 1;
            break;
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
            *blockSize = 16;
            *bcN = 2;
            break;
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
            *blockSize = 16;
            *bcN = 3;
            break;
        case VK_FORMAT_BC4_UNORM_BLOCK:
            *isNoAlphaU = true;
        case VK_FORMAT_BC4_SNORM_BLOCK:
            *blockSize = 8;
            *bcN = 4;
            break;
        case VK_FORMAT_BC5_UNORM_BLOCK:
            *isNoAlphaU = true;
        case VK_FORMAT_BC5_SNORM_BLOCK:
            *blockSize = 16;
            *bcN = 5;
            break;
        default:
            *blockSize = 0;
            *bcN = 0;
            break;
    }
}

static void internalDestroyImage(VkDevice device, TextureDecoder_Image* targetImage) {
    if (targetImage->image) vulkanWrapper.vkDestroyImage(device, targetImage->image, NULL);
    MEMFREE(targetImage);
}

static pthread_mutex_t cacheMutex = PTHREAD_MUTEX_INITIALIZER;

static bool cacheFilename(char* filename, size_t capacity, VkFormat format,
                           const VkBufferImageCopy* region, uint64_t hash) {
    int length = snprintf(filename, capacity, "%s/v2-%lx-%ux%ux%u-%u-%u.imd", CACHE_DIR, hash,
            region->imageExtent.width, region->imageExtent.height, region->imageExtent.depth,
            region->imageSubresource.layerCount, format);
    return length > 0 && length < capacity;
}

static bool readCachedImage(VkFormat format, const VkBufferImageCopy* region,
                            uint64_t hash, void* result, size_t size) {
    if (!result) return false;
    char filename[PATH_MAX];
    struct stat info;
    if (!cacheFilename(filename, sizeof(filename), format, region, hash) ||
        stat(filename, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size != size) return false;
    return fileGetContents(filename, result, &size) != NULL;
}

static void writeImageToCache(TextureDecoder* decoder, VkFormat format,
                              const VkBufferImageCopy* region, uint64_t hash, const void* data, size_t size) {
    uint64_t maximum = decoder->imageCacheSize > 0 ? (uint64_t)decoder->imageCacheSize << 20 : 0;
    if (!maximum || size > maximum) return;
    char filename[PATH_MAX], temporary[PATH_MAX];
    if (!cacheFilename(filename, sizeof(filename), format, region, hash) ||
        snprintf(temporary, sizeof(temporary), "%s.part", filename) >= sizeof(temporary)) return;
    pthread_mutex_lock(&cacheMutex);
    char* content = fileGetContents(vortekCacheSizePath, NULL, NULL);
    uint64_t current = content ? strtoull(content, NULL, 10) : 0;
    MEMFREE(content);
    struct stat existing;
    if (stat(filename, &existing) == 0)
        current = current > existing.st_size ? current - existing.st_size : 0;
    while (current > maximum - size) {
        FindFileInfo oldest = {0};
        if (!findFirstFile(CACHE_DIR, &oldest) || remove(oldest.path) != 0) goto done;
        current = current > oldest.size ? current - oldest.size : 0;
    }
    if (filePutContents(temporary, (void*)data, size) && rename(temporary, filename) == 0) {
        char value[32];
        snprintf(value, sizeof(value), "%lu", current + size);
        filePutContents(vortekCacheSizePath, value, strlen(value));
    }
    remove(temporary);
done:
    pthread_mutex_unlock(&cacheMutex);
}

static uint32_t getDeviceSupportedFormats(VkPhysicalDevice physicalDevice) {
    const VkFormat formats[] = {
        VK_FORMAT_BC1_RGB_UNORM_BLOCK,
        VK_FORMAT_BC1_RGB_SRGB_BLOCK,
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
        VK_FORMAT_BC2_UNORM_BLOCK,
        VK_FORMAT_BC2_SRGB_BLOCK,
        VK_FORMAT_BC3_UNORM_BLOCK,
        VK_FORMAT_BC3_SRGB_BLOCK,
        VK_FORMAT_BC4_UNORM_BLOCK,
        VK_FORMAT_BC4_SNORM_BLOCK,
        VK_FORMAT_BC5_UNORM_BLOCK,
        VK_FORMAT_BC5_SNORM_BLOCK
    };

    uint32_t flags = 0;
    for (int i = 0; i < ARRAY_SIZE(formats); i++) {
        VkFormatProperties formatProperties = {0};
        vulkanWrapper.vkGetPhysicalDeviceFormatProperties(physicalDevice, formats[i], &formatProperties);
        if (formatProperties.optimalTilingFeatures > 0) flags |= VK_FORMAT_BC_FLAG(formats[i]);
    }
    return flags;
}

static void destroyUpload(TextureDecoder* decoder, TextureDecoder_BufferImageCopy* copy) {
    if (copy->stagingData) vulkanWrapper.vkUnmapMemory(decoder->device, copy->stagingMemory);
    if (copy->stagingBuffer) vulkanWrapper.vkDestroyBuffer(decoder->device, copy->stagingBuffer, NULL);
    if (copy->stagingMemory) {
        vulkanWrapper.vkFreeMemory(decoder->device, copy->stagingMemory, NULL);
        decoder->stagingBytes -= copy->stagingSize;
    }
    free(copy);
}

static VkResult createStaging(TextureDecoder* decoder, TextureDecoder_BufferImageCopy* copy, uint32_t size) {
    VkBufferCreateInfo bufferInfo = {0};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkResult result = vulkanWrapper.vkCreateBuffer(decoder->device, &bufferInfo, NULL, &buffer);
    if (result != VK_SUCCESS) return result;
    copy->stagingBuffer = buffer;
    VkMemoryRequirements requirements = {0};
    vulkanWrapper.vkGetBufferMemoryRequirements(decoder->device, copy->stagingBuffer, &requirements);
    // Staging has its own bound on this 8 GB target, in addition to image memory.
    const uint64_t maximumStaging = 256ull << 20;
    if (requirements.size > maximumStaging || decoder->stagingBytes > maximumStaging - requirements.size)
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    VkMemoryAllocateInfo allocation = {0};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = getMemoryTypeIndex(requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (allocation.memoryTypeIndex == UINT32_MAX)
        allocation.memoryTypeIndex = getMemoryTypeIndex(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    if (allocation.memoryTypeIndex == UINT32_MAX) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    copy->memoryCoherent = (getMemoryPropertyFlags(allocation.memoryTypeIndex) & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    result = vulkanWrapper.vkAllocateMemory(decoder->device, &allocation, NULL, &memory);
    if (result != VK_SUCCESS) return result;
    copy->stagingMemory = memory;
    copy->stagingSize = requirements.size;
    decoder->stagingBytes += requirements.size;
    result = vulkanWrapper.vkBindBufferMemory(decoder->device, copy->stagingBuffer, copy->stagingMemory, 0);
    if (result != VK_SUCCESS) return result;
    return vulkanWrapper.vkMapMemory(decoder->device, copy->stagingMemory, 0,
            copy->stagingSize, 0, &copy->stagingData);
}

TextureDecoder* TextureDecoder_create(VkContext* context, VkPhysicalDevice physicalDevice, VkDevice device, VkPhysicalDeviceFeatures* supportedFeatures) {
    if (supportedFeatures->textureCompressionBC) return NULL;
    TextureDecoder* decoder = calloc(1, sizeof(TextureDecoder));
    if (!decoder) return NULL;
    decoder->device = device;
    decoder->imageCacheSize = context->imageCacheSize;
    decoder->threadPool = context->threadPool;
    decoder->deviceSupportedFormats = getDeviceSupportedFormats(physicalDevice);
    return decoder;
}

void TextureDecoder_destroy(TextureDecoder* decoder) {
    for (int i = 0; i < decoder->bufferImageCopies.size; i++)
        destroyUpload(decoder, decoder->bufferImageCopies.elements[i]);
    ArrayList_free(&decoder->bufferImageCopies, false);
    ArrayList_free(&decoder->commandLinks, true);
    SparseArray64_free(&decoder->commandPools, false);
    SparseArray64_free(&decoder->images, true);
    SparseArray64_free(&decoder->boundBuffers, true);
    free(decoder);
}

static VkResult decodeUpload(TextureDecoder* decoder, TextureDecoder_BufferImageCopy* copy) {
    if (!copy->srcBuffer || !copy->dstImage || !copy->srcBuffer->memory) return VK_ERROR_DEVICE_LOST;
    TextureDecoder_BoundBuffer* source = copy->srcBuffer;
    VkBufferImageCopy* region = &copy->region;
    int blockSize, bcN;
    bool noAlphaUnsigned;
    getBCInfo(copy->dstImage->format, &bcN, &blockSize, &noAlphaUnsigned);
    DGBCUpload layout;
    if (!dgBCUploadLayout(region, blockSize, source->memoryOffset, source->memory->allocationSize, &layout))
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    void* mapping = mmap(NULL, source->memory->allocationSize, PROT_READ, MAP_SHARED, source->memory->fd, 0);
    if (mapping == MAP_FAILED) return VK_ERROR_OUT_OF_HOST_MEMORY;
    VkResult result = VK_SUCCESS;
    void* packed = NULL;
    const void* imageData = (uint8_t*)mapping + layout.offset;
    if (layout.rowStride != layout.rowBytes || layout.sliceStride != layout.rowBytes * layout.rows) {
        packed = malloc(layout.packedSize);
        if (!packed) { result = VK_ERROR_OUT_OF_HOST_MEMORY; goto done; }
        dgPackBCUpload(mapping, &layout, packed);
        imageData = packed;
    }
    // A command buffer may be submitted again while an earlier execution is
    // still pending (SIMULTANEOUS_USE). Do not write its GPU-read staging if
    // the source content is unchanged. Changing the source remains subject
    // to the application's original buffer-copy synchronization contract.
    uint64_t contentHash[2] = {
        murmurHash64(imageData, layout.packedSize, copy->dstImage->format),
        murmurHash64(imageData, layout.packedSize, copy->dstImage->format ^ 0x5bd1e995)
    };
    if (copy->hasDecodedContent && memcmp(copy->contentHash, contentHash, sizeof(contentHash)) == 0)
        goto done;
    uint64_t hash = 0;
    bool cached = false;
    if (decoder->imageCacheSize > 0 && region->imageExtent.width >= CACHE_MIN_IMAGE_WIDTH &&
            region->imageExtent.height >= CACHE_MIN_IMAGE_WIDTH) {
        hash = contentHash[0];
        cached = readCachedImage(copy->dstImage->format, region, hash, copy->stagingData, layout.decodedSize);
    }
    if (!cached) {
        uint64_t packedSlice = layout.rowBytes * layout.rows;
        uint64_t decodedSlice = (uint64_t)region->imageExtent.width * region->imageExtent.height * 4;
        memset(copy->stagingData, 0, layout.decodedSize);
        if (bcN == 4 || bcN == 5) {
            uint8_t alpha = noAlphaUnsigned ? 255 : 127;
            for (uint64_t i = 3; i < layout.decodedSize; i += 4) ((uint8_t*)copy->stagingData)[i] = alpha;
        }
        for (uint64_t slice = 0; slice < layout.slices; slice++)
            BCDecoder_decode((uint8_t*)imageData + slice * packedSlice,
                    (uint8_t*)copy->stagingData + slice * decodedSlice,
                    region->imageExtent.width, region->imageExtent.height,
                    bcN, noAlphaUnsigned, decoder->threadPool);
        if (hash) writeImageToCache(decoder, copy->dstImage->format, region, hash,
                copy->stagingData, layout.decodedSize);
    }
    if (!copy->memoryCoherent) {
        VkMappedMemoryRange range = {0};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = copy->stagingMemory;
        range.size = VK_WHOLE_SIZE;
        result = vulkanWrapper.vkFlushMappedMemoryRanges(decoder->device, 1, &range);
    }
    if (result == VK_SUCCESS) {
        memcpy(copy->contentHash, contentHash, sizeof(contentHash));
        copy->hasDecodedContent = true;
    }
done:
    free(packed);
    munmap(mapping, source->memory->allocationSize);
    return result;
}

void TextureDecoder_registerCommand(TextureDecoder* decoder, VkCommandBuffer command, VkCommandPool pool) {
    SparseArray64_put(&decoder->commandPools, (int64_t)command, (void*)pool);
}

void TextureDecoder_resetCommand(TextureDecoder* decoder, VkCommandBuffer command, bool freeCommand) {
    for (int i = decoder->bufferImageCopies.size - 1; i >= 0; i--) {
        TextureDecoder_BufferImageCopy* copy = decoder->bufferImageCopies.elements[i];
        if (copy->commandBuffer == command) {
            ArrayList_removeAt(&decoder->bufferImageCopies, i);
            destroyUpload(decoder, copy);
        }
    }
    for (int i = decoder->commandLinks.size - 1; i >= 0; i--) {
        TextureDecoder_CommandLink* link = decoder->commandLinks.elements[i];
        if (link->parent == command || (freeCommand && link->secondary == command)) {
            free(link);
            ArrayList_removeAt(&decoder->commandLinks, i);
        }
    }
    if (freeCommand) SparseArray64_remove(&decoder->commandPools, (int64_t)command);
}

void TextureDecoder_resetPool(TextureDecoder* decoder, VkCommandPool pool, bool freePool) {
    for (int i = decoder->commandPools.size - 1; i >= 0; i--)
        if (decoder->commandPools.entries[i].value == (void*)pool)
            TextureDecoder_resetCommand(decoder, (VkCommandBuffer)decoder->commandPools.entries[i].key, freePool);
}

VkResult TextureDecoder_linkCommands(TextureDecoder* decoder, VkCommandBuffer parent, uint32_t count, const VkCommandBuffer* commands) {
    for (uint32_t i = 0; i < count; i++) {
        bool found = false;
        for (int j = 0; j < decoder->commandLinks.size; j++) {
            TextureDecoder_CommandLink* link = decoder->commandLinks.elements[j];
            if (link->parent == parent && link->secondary == commands[i]) { found = true; break; }
        }
        if (found) continue;
        TextureDecoder_CommandLink* link = calloc(1, sizeof(*link));
        if (!link) return VK_ERROR_OUT_OF_HOST_MEMORY;
        link->parent = parent;
        link->secondary = commands[i];
        ArrayList_add(&decoder->commandLinks, link);
    }
    return VK_SUCCESS;
}

static VkResult decodeCommandRecursive(TextureDecoder* decoder, VkCommandBuffer command, bool* used, int depth) {
    if (depth > 16) return VK_ERROR_DEVICE_LOST;
    for (int i = 0; i < decoder->bufferImageCopies.size; i++) {
        TextureDecoder_BufferImageCopy* copy = decoder->bufferImageCopies.elements[i];
        if (copy->commandBuffer != command) continue;
        *used = true;
        VkResult result = decodeUpload(decoder, copy);
        if (result != VK_SUCCESS) return result;
    }
    for (int i = 0; i < decoder->commandLinks.size; i++) {
        TextureDecoder_CommandLink* link = decoder->commandLinks.elements[i];
        if (link->parent != command) continue;
        VkResult result = decodeCommandRecursive(decoder, link->secondary, used, depth + 1);
        if (result != VK_SUCCESS) return result;
    }
    return VK_SUCCESS;
}

VkResult TextureDecoder_decodeCommand(TextureDecoder* decoder, VkCommandBuffer command, bool* used) {
    return decodeCommandRecursive(decoder, command, used, 0);
}

VkResult TextureDecoder_copyBufferToImage(TextureDecoder* decoder, VkCommandBuffer command, VkBuffer srcBuffer,
        VkImage dstImage, VkImageLayout layout, uint32_t count, VkBufferImageCopy* regions) {
    TextureDecoder_BoundBuffer* source = SparseArray64_get(&decoder->boundBuffers, (int64_t)srcBuffer);
    TextureDecoder_Image* destination = SparseArray64_get(&decoder->images, (int64_t)dstImage);
    VkCommandPool pool = (VkCommandPool)SparseArray64_get(&decoder->commandPools, (int64_t)command);
    if (!source || !source->memory || !destination || !pool) return VK_ERROR_DEVICE_LOST;
    for (uint32_t i = 0; i < count; i++) {
        TextureDecoder_BufferImageCopy* copy = calloc(1, sizeof(*copy));
        if (!copy) return VK_ERROR_OUT_OF_HOST_MEMORY;
        copy->srcBuffer = source;
        copy->dstImage = destination;
        copy->region = regions[i];
        copy->commandBuffer = command;
        copy->commandPool = pool;
        int blockSize, bcN;
        bool unused;
        getBCInfo(destination->format, &bcN, &blockSize, &unused);
        DGBCUpload upload;
        if (!dgBCUploadLayout(&copy->region, blockSize, source->memoryOffset, source->memory->allocationSize, &upload)) {
            free(copy);
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }
        VkResult result = createStaging(decoder, copy, (uint32_t)upload.decodedSize);
        if (result != VK_SUCCESS) { destroyUpload(decoder, copy); return result; }
        ArrayList_add(&decoder->bufferImageCopies, copy);
        VkBufferImageCopy decoded = copy->region;
        decoded.bufferOffset = 0;
        decoded.bufferRowLength = 0;
        decoded.bufferImageHeight = 0;
        // Preserve the application's barriers, command ordering and layout.
        // Only the source buffer changes; its bytes are filled before submit.
        vulkanWrapper.vkCmdCopyBufferToImage(command, copy->stagingBuffer, dstImage, layout, 1, &decoded);
    }
    return VK_SUCCESS;
}

VkResult TextureDecoder_copyBufferToImage2(TextureDecoder* decoder, VkCommandBuffer command, VkBuffer srcBuffer,
        VkImage dstImage, VkImageLayout layout, VkCopyBufferToImageInfo2* info) {
    for (uint32_t i = 0; i < info->regionCount; i++) {
        const VkBufferImageCopy2* source = &info->pRegions[i];
        VkBufferImageCopy region = {0};
        region.bufferOffset = source->bufferOffset;
        region.bufferRowLength = source->bufferRowLength;
        region.bufferImageHeight = source->bufferImageHeight;
        region.imageSubresource = source->imageSubresource;
        region.imageOffset = source->imageOffset;
        region.imageExtent = source->imageExtent;
        VkResult result = TextureDecoder_copyBufferToImage(decoder, command, srcBuffer, dstImage, layout, 1, &region);
        if (result != VK_SUCCESS) return result;
    }
    return VK_SUCCESS;
}

bool TextureDecoder_containsImage(TextureDecoder* textureDecoder, VkImage image) {
    return SparseArray64_indexOfKey(&textureDecoder->images, (int64_t)image) >= 0;
}

VkResult TextureDecoder_createImage(TextureDecoder* textureDecoder, VkImageCreateInfo* imageInfo, VkImage* pImage) {
    *pImage = VK_NULL_HANDLE;
    VkResult result;
    if (!TextureDecoder_canDecompressFormat(imageInfo->format) ||
        imageInfo->imageType != VK_IMAGE_TYPE_2D || imageInfo->tiling != VK_IMAGE_TILING_OPTIMAL ||
        imageInfo->samples != VK_SAMPLE_COUNT_1_BIT) return VK_ERROR_FORMAT_NOT_SUPPORTED;

    TextureDecoder_Image* newImage = calloc(1, sizeof(TextureDecoder_Image));
    if (!newImage) return VK_ERROR_OUT_OF_HOST_MEMORY;
    newImage->format = imageInfo->format;
    newImage->width = imageInfo->extent.width;
    newImage->height = imageInfo->extent.height;

    imageInfo->format = TextureDecoder_decodedFormat(newImage->format);
    imageInfo->flags &= ~VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT;
    imageInfo->flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

    VkImageFormatListCreateInfo* formatListInfo = findNextVkStructure(imageInfo->pNext, VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO);
    if (formatListInfo && formatListInfo->pViewFormats) {
        VkFormat* viewFormats = (VkFormat*)formatListInfo->pViewFormats;
        uint32_t count = 0;
        for (uint32_t i = 0; i < formatListInfo->viewFormatCount; i++) {
            VkFormat decoded = TextureDecoder_decodedFormat(viewFormats[i]);
            bool duplicate = false;
            for (uint32_t j = 0; j < count; j++) if (viewFormats[j] == decoded) duplicate = true;
            if (!duplicate) viewFormats[count++] = decoded;
        }
        formatListInfo->viewFormatCount = count;
    }

    VkImage image;
    result = vulkanWrapper.vkCreateImage(textureDecoder->device, imageInfo, NULL, &image);
    if (result != VK_SUCCESS) goto error;;

    newImage->image = image;
    SparseArray64_put(&textureDecoder->images, (int64_t)image, newImage);

    *pImage = image;
    return result;

error:
    internalDestroyImage(textureDecoder->device, newImage);
    return result;
}

void TextureDecoder_destroyImage(TextureDecoder* textureDecoder, VkImage image) {
    TextureDecoder_Image* targetImage = SparseArray64_get(&textureDecoder->images, (int64_t)image);
    if (targetImage) {
        for (int i = 0; i < textureDecoder->bufferImageCopies.size; i++) {
            TextureDecoder_BufferImageCopy* copy = textureDecoder->bufferImageCopies.elements[i];
            if (copy->dstImage == targetImage) copy->dstImage = NULL;
        }
        SparseArray64_remove(&textureDecoder->images, (int64_t)image);
        internalDestroyImage(textureDecoder->device, targetImage);
    }
}

void TextureDecoder_addBoundBuffer(TextureDecoder* textureDecoder, ResourceMemory* memory, VkBuffer buffer, VkDeviceSize memoryOffset) {
    if (memory->fd <= 0) return;
    TextureDecoder_removeBoundBuffer(textureDecoder, buffer);

    TextureDecoder_BoundBuffer* boundBuffer = calloc(1, sizeof(TextureDecoder_BoundBuffer));
    boundBuffer->buffer = buffer;
    boundBuffer->memoryOffset = memoryOffset;
    boundBuffer->memory = memory;

    SparseArray64_put(&textureDecoder->boundBuffers, (int64_t)buffer, boundBuffer);
}

void TextureDecoder_removeBoundBuffer(TextureDecoder* textureDecoder, VkBuffer buffer) {
    TextureDecoder_BoundBuffer* boundBuffer = SparseArray64_get(&textureDecoder->boundBuffers, (int64_t)buffer);
    if (boundBuffer) {
        for (int i = 0; i < textureDecoder->bufferImageCopies.size; i++) {
            TextureDecoder_BufferImageCopy* copy = textureDecoder->bufferImageCopies.elements[i];
            if (copy->srcBuffer == boundBuffer) copy->srcBuffer = NULL;
        }
        SparseArray64_remove(&textureDecoder->boundBuffers, (int64_t)buffer);
        MEMFREE(boundBuffer);
    }
}

void TextureDecoder_forgetMemory(TextureDecoder* decoder, ResourceMemory* memory) {
    for (int i = 0; i < decoder->boundBuffers.size; i++) {
        TextureDecoder_BoundBuffer* bound = decoder->boundBuffers.entries[i].value;
        if (bound->memory == memory) bound->memory = NULL;
    }
}

bool TextureDecoder_isFormatSupported(TextureDecoder* textureDecoder, VkFormat format) {
    if (!TextureDecoder_canDecompressFormat(format)) return false;
    if (textureDecoder->deviceSupportedFormats > 0) {
        bool result = textureDecoder->deviceSupportedFormats & VK_FORMAT_BC_FLAG(format);
        if (result) return false;
    }
    return true;
}

bool isCompressedFormat(VkFormat format) {
    switch(format) {
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC4_UNORM_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
        case VK_FORMAT_BC6H_UFLOAT_BLOCK:
        case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
        case VK_FORMAT_EAC_R11_UNORM_BLOCK:
        case VK_FORMAT_EAC_R11_SNORM_BLOCK:
        case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
        case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
            return true;
        default:
            return false;
    }
}

VkResult getCompressedImageFormatProperties(VkFormat format, VkImageFormatProperties* pImageFormatProperties) {
    if (TextureDecoder_canDecompressFormat(format)) {
        pImageFormatProperties->maxExtent.width = 16384;
        pImageFormatProperties->maxExtent.height = 16384;
        pImageFormatProperties->maxExtent.depth = 1;
        pImageFormatProperties->maxMipLevels = 15;
        pImageFormatProperties->maxArrayLayers = 2048;
        pImageFormatProperties->sampleCounts = VK_SAMPLE_COUNT_1_BIT;
        pImageFormatProperties->maxResourceSize = 1u << 31;
        return VK_SUCCESS;
    }
    else return VK_ERROR_FORMAT_NOT_SUPPORTED;
}
