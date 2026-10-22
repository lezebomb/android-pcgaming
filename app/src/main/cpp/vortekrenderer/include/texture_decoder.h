#ifndef VORTEK_TEXTURE_DECODER_H
#define VORTEK_TEXTURE_DECODER_H

#include "vortek.h"

typedef struct TextureDecoder_Image {
    VkImage image;
    VkFormat format;
    short width;
    short height;
} TextureDecoder_Image;

typedef struct TextureDecoder_BoundBuffer {
    VkBuffer buffer;
    VkDeviceSize memoryOffset;
    ResourceMemory* memory;
} TextureDecoder_BoundBuffer;

typedef struct TextureDecoder_BufferImageCopy {
    TextureDecoder_BoundBuffer* srcBuffer;
    TextureDecoder_Image* dstImage;
    VkImageLayout dstImageLayout;
    VkBufferImageCopy region;
    VkCommandBuffer commandBuffer;
    VkCommandPool commandPool;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VkDeviceSize stagingSize;
    void* stagingData;
    bool memoryCoherent;
    bool hasDecodedContent;
    uint64_t contentHash[2];
} TextureDecoder_BufferImageCopy;

typedef struct TextureDecoder_CommandLink {
    VkCommandBuffer parent;
    VkCommandBuffer secondary;
} TextureDecoder_CommandLink;

typedef struct TextureDecoder {
    SparseArray64 images;
    SparseArray64 boundBuffers;
    ArrayList bufferImageCopies;
    ArrayList commandLinks;
    SparseArray64 commandPools;
    uint64_t stagingBytes;
    short imageCacheSize;
    ThreadPool* threadPool;
    uint32_t deviceSupportedFormats;

    VkDevice device;
} TextureDecoder;

extern TextureDecoder* TextureDecoder_create(VkContext* context, VkPhysicalDevice physicalDevice, VkDevice device, VkPhysicalDeviceFeatures* supportedFeatures);
extern void TextureDecoder_destroy(TextureDecoder* textureDecoder);
extern VkResult TextureDecoder_decodeCommand(TextureDecoder* decoder, VkCommandBuffer command, bool* used);
extern void TextureDecoder_registerCommand(TextureDecoder* decoder, VkCommandBuffer command, VkCommandPool pool);
extern void TextureDecoder_resetCommand(TextureDecoder* decoder, VkCommandBuffer command, bool freeCommand);
extern void TextureDecoder_resetPool(TextureDecoder* decoder, VkCommandPool pool, bool freePool);
extern VkResult TextureDecoder_linkCommands(TextureDecoder* decoder, VkCommandBuffer parent, uint32_t count, const VkCommandBuffer* secondary);
extern bool TextureDecoder_canDecompressFormat(VkFormat format);
extern VkFormat TextureDecoder_decodedFormat(VkFormat format);
extern VkResult TextureDecoder_copyBufferToImage(TextureDecoder* textureDecoder, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, VkBufferImageCopy* regions);
extern VkResult TextureDecoder_copyBufferToImage2(TextureDecoder* textureDecoder, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, VkCopyBufferToImageInfo2* copyBufferToImageInfo);
extern bool TextureDecoder_containsImage(TextureDecoder* textureDecoder, VkImage image);
extern VkResult TextureDecoder_createImage(TextureDecoder* textureDecoder, VkImageCreateInfo* imageInfo, VkImage* pImage);
extern void TextureDecoder_destroyImage(TextureDecoder* textureDecoder, VkImage image);
extern void TextureDecoder_addBoundBuffer(TextureDecoder* textureDecoder, ResourceMemory* resourceMemory, VkBuffer buffer, VkDeviceSize memoryOffset);
extern void TextureDecoder_removeBoundBuffer(TextureDecoder* textureDecoder, VkBuffer buffer);
extern void TextureDecoder_forgetMemory(TextureDecoder* textureDecoder, ResourceMemory* memory);
extern bool TextureDecoder_isFormatSupported(TextureDecoder* textureDecoder, VkFormat format);
extern bool isCompressedFormat(VkFormat format);
extern VkResult getCompressedImageFormatProperties(VkFormat format, VkImageFormatProperties* pImageFormatProperties);

#endif

#define DECOMPRESSED_FORMAT VK_FORMAT_B8G8R8A8_UNORM
