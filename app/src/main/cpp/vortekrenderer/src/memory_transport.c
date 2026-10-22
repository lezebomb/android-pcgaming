// SPDX-License-Identifier: LGPL-2.1-only
#include "resource_memory.h"
#include "vulkan_helper.h"
#include "dma_utils.h"

// Native HOST_VISIBLE does not imply that an allocation can be mapped by the
// guest process through this proxy's external-memory transport. Query before
// the guest chooses a memory type; never substitute another index at allocation.
uint32_t ResourceMemory_queryHostVisibleTypes(VkContext* context, VkPhysicalDevice physical) {
    VkPhysicalDeviceMemoryProperties memory = {0};
    vulkanWrapper.vkGetPhysicalDeviceMemoryProperties(physical, &memory);
    uint32_t hostTypes = 0;
    for (uint32_t i = 0; i < memory.memoryTypeCount && i < 32; i++)
        if (memory.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) hostTypes |= 1u << i;
    bool opaque = context->hasExternalMemoryFd, dma = context->hasExternalMemoryDMABuf;
    switch (context->resourceMemoryType) {
        case RESOURCE_MEMORY_TYPE_OPAQUE_FD: opaque = true; dma = false; break;
        case RESOURCE_MEMORY_TYPE_DMA_BUF: opaque = false; dma = true; break;
        case RESOURCE_MEMORY_TYPE_AHARDWAREBUFFER: opaque = dma = false; break;
        default: break;
    }
    // Opaque export allocates the selected native type directly. DMA/AHB
    // imports constrain the index to their reported memoryTypeBits.
    if (opaque || !hostTypes) return hostTypes;
    uint32_t extensionCount = 0;
    if (vulkanWrapper.vkEnumerateDeviceExtensionProperties(physical, NULL, &extensionCount, NULL) != VK_SUCCESS) return 0;
    VkExtensionProperties* extensions = calloc(extensionCount, sizeof(*extensions));
    if (!extensions) return 0;
    if (vulkanWrapper.vkEnumerateDeviceExtensionProperties(physical, NULL, &extensionCount, extensions) != VK_SUCCESS) {
        free(extensions);
        return 0;
    }
    const char* enabled[3];
    uint32_t enabledCount = 0;
    for (uint32_t i = 0; i < extensionCount; i++) {
        const char* name = extensions[i].extensionName;
        if (!strcmp(name, "VK_ANDROID_external_memory_android_hardware_buffer") ||
                (dma && (!strcmp(name, "VK_KHR_external_memory_fd") || !strcmp(name, "VK_EXT_external_memory_dma_buf"))))
            enabled[enabledCount++] = name;
    }
    uint32_t familyCount = 0;
    vulkanWrapper.vkGetPhysicalDeviceQueueFamilyProperties(physical, &familyCount, NULL);
    VkQueueFamilyProperties* families = calloc(familyCount, sizeof(*families));
    if (!families) { free(extensions); return 0; }
    vulkanWrapper.vkGetPhysicalDeviceQueueFamilyProperties(physical, &familyCount, families);
    uint32_t family = UINT32_MAX;
    for (uint32_t i = 0; i < familyCount; i++)
        if (families[i].queueCount && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) { family = i; break; }
    free(families);
    if (family == UINT32_MAX) { free(extensions); return 0; }
    float priority = 1;
    VkDeviceQueueCreateInfo queue = {0};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueFamilyIndex = family;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo create = {0};
    create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create.queueCreateInfoCount = 1;
    create.pQueueCreateInfos = &queue;
    create.enabledExtensionCount = enabledCount;
    create.ppEnabledExtensionNames = enabled;
    VkDevice probe = VK_NULL_HANDLE;
    VkResult created = vulkanWrapper.vkCreateDevice(physical, &create, NULL, &probe);
    free(extensions);
    if (created != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_WARN, "DimensityMemory", "Transport query device failed: %d", created);
        return 0;
    }
    // Keep these function pointers local. Loading a temporary device into the
    // shared dispatch table would overwrite the actual game's device state.
    PFN_vkGetAndroidHardwareBufferPropertiesANDROID getAhb = (PFN_vkGetAndroidHardwareBufferPropertiesANDROID)
            vulkanWrapper.vkGetDeviceProcAddr(probe, "vkGetAndroidHardwareBufferPropertiesANDROID");
    PFN_vkGetMemoryFdPropertiesKHR getFd = (PFN_vkGetMemoryFdPropertiesKHR)
            vulkanWrapper.vkGetDeviceProcAddr(probe, "vkGetMemoryFdPropertiesKHR");
    PFN_vkDestroyDevice destroy = (PFN_vkDestroyDevice)vulkanWrapper.vkGetDeviceProcAddr(probe, "vkDestroyDevice");
    uint32_t sharedTypes = 0;
    if (dma && getFd) {
        int fd = dmabuf_alloc(4096);
        if (fd >= 0) {
            VkMemoryFdPropertiesKHR properties = {0};
            properties.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
            if (getFd(probe, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd, &properties) == VK_SUCCESS)
                sharedTypes |= properties.memoryTypeBits;
            close(fd);
        }
    }
    if (getAhb) {
        AHardwareBuffer_Desc desc = {0};
        desc.width = 4096;
        desc.height = desc.layers = 1;
        desc.format = AHARDWAREBUFFER_FORMAT_BLOB;
        desc.usage = AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER | AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN |
                AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
        AHardwareBuffer* buffer = NULL;
        if (AHardwareBuffer_allocate(&desc, &buffer) == 0 && buffer) {
            VkAndroidHardwareBufferPropertiesANDROID properties = {0};
            properties.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
            if (getAhb(probe, buffer, &properties) == VK_SUCCESS) sharedTypes |= properties.memoryTypeBits;
            AHardwareBuffer_release(buffer);
        }
    }
    destroy(probe, NULL);
    sharedTypes &= hostTypes;
    __android_log_print(ANDROID_LOG_INFO, "DimensityMemory", "Transport HOST_VISIBLE mask: native=0x%x shared=0x%x", hostTypes, sharedTypes);
    return sharedTypes;
}
