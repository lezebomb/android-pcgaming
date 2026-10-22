#include "resource_memory.h"
#include "vulkan_helper.h"
#include "dma_utils.h"
#include "native_handle.h"

extern const native_handle_t* AHardwareBuffer_getNativeHandle(const AHardwareBuffer* buffer);
extern DeviceMemoryInfo deviceMemoryInfo;

// The AHB owns this fd; keep our own reference for the guest's mmap.
static int duplicateHardwareBufferFd(AHardwareBuffer* buffer) {
    const native_handle_t* handle = AHardwareBuffer_getNativeHandle(buffer);
    return handle && handle->numFds > 0 ? fcntl(handle->data[0], F_DUPFD_CLOEXEC, 0) : -1;
}

static bool fitsAllocationBudget(const VkContext* context, VkDeviceSize size) {
    uint64_t limit = (uint64_t)context->maxDeviceMemory << 20;
    if (!limit) limit = deviceMemoryInfo.maxAllocationSize;
    return size && context->totalAllocationSize <= limit && size <= limit - context->totalAllocationSize;
}

static VkResult allocateNative(VkContext* context, VkDevice device, const VkMemoryAllocateInfo* info, ResourceMemory* resource) {
    // Check the final native size, which may include AHB allocator padding.
    if (!fitsAllocationBudget(context, info->allocationSize)) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    VkResult result = vulkanWrapper.vkAllocateMemory(device, info, NULL, &resource->memory);
    if (result == VK_SUCCESS) {
        resource->allocationSize = info->allocationSize;
        context->totalAllocationSize += resource->allocationSize;
    }
    return result;
}

static VkResult allocateOpaqueFd(VkContext* context, VkDevice device, const VkMemoryAllocateInfo* original, ResourceMemory* resource) {
    if (!vulkanWrapper.vkGetMemoryFd) return VK_ERROR_FEATURE_NOT_PRESENT;
    VkExportMemoryAllocateInfo exportInfo = {0};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportInfo.pNext = original->pNext;
    exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkMemoryAllocateInfo info = *original;
    info.pNext = &exportInfo;
    // Keep the real resource's dedicated allocation and device-address flags.
    // Dedicating this memory to a dummy buffer forbids later game bindings.
    VkResult result = allocateNative(context, device, &info, resource);
    if (result != VK_SUCCESS) return result;
    VkMemoryGetFdInfoKHR fdInfo = {0};
    fdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    fdInfo.memory = resource->memory;
    fdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    result = vulkanWrapper.vkGetMemoryFd(device, &fdInfo, &resource->fd);
    return result == VK_SUCCESS && resource->fd < 0 ? VK_ERROR_INVALID_EXTERNAL_HANDLE : result;
}

static VkResult allocateDmaBuf(VkContext* context, VkDevice device, const VkMemoryAllocateInfo* original, ResourceMemory* resource) {
    if (!vulkanWrapper.vkGetMemoryFdProperties) return VK_ERROR_FEATURE_NOT_PRESENT;
    int fd = dmabuf_alloc(original->allocationSize);
    if (fd < 0) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    VkMemoryFdPropertiesKHR properties = {0};
    properties.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
    VkResult result = vulkanWrapper.vkGetMemoryFdProperties(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd, &properties);
    if (result == VK_SUCCESS && !(properties.memoryTypeBits & (1u << original->memoryTypeIndex)))
        result = VK_ERROR_INVALID_EXTERNAL_HANDLE;
    int importFd = result == VK_SUCCESS ? fcntl(fd, F_DUPFD_CLOEXEC, 0) : -1;
    if (result == VK_SUCCESS && importFd < 0) result = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (result == VK_SUCCESS) {
        VkImportMemoryFdInfoKHR importInfo = {0};
        importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
        importInfo.pNext = original->pNext;
        importInfo.fd = importFd;
        importInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
        VkMemoryAllocateInfo info = *original;
        info.pNext = &importInfo;
        result = allocateNative(context, device, &info, resource);
    }
    if (result == VK_SUCCESS) resource->fd = fd; // Vulkan consumed importFd.
    else {
        if (importFd >= 0) close(importFd); // Failed imports do not consume it.
        close(fd);
    }
    return result;
}

static VkResult allocateHardwareBuffer(VkContext* context, VkDevice device, const VkMemoryAllocateInfo* original, ResourceMemory* resource) {
    if (original->allocationSize > UINT32_MAX) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    if (!vulkanWrapper.vkGetAndroidHardwareBufferPropertiesANDROID) return VK_ERROR_FEATURE_NOT_PRESENT;
    const VkMemoryDedicatedAllocateInfo* dedicated = findNextVkStructure(original->pNext, VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO);
    // A BLOB cannot satisfy the dimensions/format of a dedicated image import.
    if (dedicated && dedicated->image) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    AHardwareBuffer_Desc desc = {0};
    desc.width = original->allocationSize;
    desc.height = desc.layers = 1;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER |
            AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN | AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN;
    desc.format = AHARDWAREBUFFER_FORMAT_BLOB;
    if (AHardwareBuffer_allocate(&desc, &resource->hardwareBuffer) != 0 || !resource->hardwareBuffer)
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    VkAndroidHardwareBufferPropertiesANDROID properties = {0};
    properties.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
    VkResult result = vulkanWrapper.vkGetAndroidHardwareBufferPropertiesANDROID(device, resource->hardwareBuffer, &properties);
    if (result != VK_SUCCESS) return result;
    // Never silently choose a different native memory type: the game selected
    // this index from its resource requirements and may rely on all its flags.
    if (!(properties.memoryTypeBits & (1u << original->memoryTypeIndex)) ||
            properties.allocationSize < original->allocationSize) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    resource->fd = duplicateHardwareBufferFd(resource->hardwareBuffer);
    if (resource->fd < 0) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    VkImportAndroidHardwareBufferInfoANDROID importInfo = {0};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    importInfo.pNext = original->pNext;
    importInfo.buffer = resource->hardwareBuffer;
    VkMemoryAllocateInfo info = *original;
    info.pNext = &importInfo;
    info.allocationSize = properties.allocationSize;
    return allocateNative(context, device, &info, resource);
}

ResourceMemory* ResourceMemory_allocate(VkContext* context, VkDevice device, const VkMemoryAllocateInfo* info, VkResult* outResult) {
    VkResult result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    if (!info || info->memoryTypeIndex >= deviceMemoryInfo.memoryTypeCount || info->memoryTypeIndex >= 32 ||
            !fitsAllocationBudget(context, info->allocationSize)) goto failed;
    ResourceMemory* resource = calloc(1, sizeof(*resource));
    if (!resource) { result = VK_ERROR_OUT_OF_HOST_MEMORY; goto failed; }
    resource->fd = -1;
    resource->device = device;
    if (!isHostVisibleMemory(info->memoryTypeIndex)) result = allocateNative(context, device, info, resource);
    else {
        bool opaque = context->hasExternalMemoryFd;
        bool dma = context->hasExternalMemoryDMABuf;
        switch (context->resourceMemoryType) {
            case RESOURCE_MEMORY_TYPE_OPAQUE_FD: opaque = true; dma = false; break;
            case RESOURCE_MEMORY_TYPE_DMA_BUF: opaque = false; dma = true; break;
            case RESOURCE_MEMORY_TYPE_AHARDWAREBUFFER: opaque = dma = false; break;
            default: break;
        }
        if (opaque) result = allocateOpaqueFd(context, device, info, resource);
        else {
            result = dma ? allocateDmaBuf(context, device, info, resource) : VK_ERROR_FEATURE_NOT_PRESENT;
            if (result != VK_SUCCESS) result = allocateHardwareBuffer(context, device, info, resource);
        }
    }
    if (result == VK_SUCCESS) { *outResult = result; return resource; }
    ResourceMemory_free(context, device, resource);
failed:
    *outResult = result;
    return NULL;
}

// A guest fd mmap alone does not make this VkDeviceMemory host-mapped in
// the system driver. Keep one full native mapping so forwarded flush/invalidate
// ranges refer to a valid mapped allocation; guest unmaps stay local to Wine.
VkResult ResourceMemory_map(ResourceMemory* resource) {
    if (!resource || !resource->memory || resource->fd < 0) return VK_ERROR_MEMORY_MAP_FAILED;
    if (resource->hostMapped) return VK_SUCCESS;
    void* data = NULL;
    VkResult result = vulkanWrapper.vkMapMemory(resource->device, resource->memory, 0, VK_WHOLE_SIZE, 0, &data);
    if (result == VK_SUCCESS) resource->hostMapped = true;
    return result;
}

void ResourceMemory_free(VkContext* context, VkDevice device, ResourceMemory* resource) {
    if (!resource) return;
    if (resource->fd >= 0) close(resource->fd);
    if (resource->hostMapped) vulkanWrapper.vkUnmapMemory(device, resource->memory);
    if (resource->memory) vulkanWrapper.vkFreeMemory(device, resource->memory, NULL);
    // Only successful allocations were charged, including failed fd exports
    // that happen after native allocation. Error cleanup balances that charge.
    uint64_t total = context->totalAllocationSize;
    context->totalAllocationSize = total >= resource->allocationSize ? total - resource->allocationSize : 0;
    if (resource->hardwareBuffer) AHardwareBuffer_release(resource->hardwareBuffer);
    free(resource);
}
