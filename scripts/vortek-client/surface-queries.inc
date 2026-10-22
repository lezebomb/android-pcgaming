// The proxy's surface handle is a guest window, not a system Vulkan surface.
// Extend the existing checked RPC queries without passing it to the driver.
VkResult vt_call_vkGetPhysicalDeviceSurfaceCapabilities2KHR(VkPhysicalDevice device,
        const VkPhysicalDeviceSurfaceInfo2KHR* info, VkSurfaceCapabilities2KHR* output) {
    if (!info || !output) return VK_ERROR_SURFACE_LOST_KHR;
    return vt_call_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, info->surface, &output->surfaceCapabilities);
}

VkResult vt_call_vkGetPhysicalDeviceSurfaceFormats2KHR(VkPhysicalDevice device,
        const VkPhysicalDeviceSurfaceInfo2KHR* info, uint32_t* count, VkSurfaceFormat2KHR* output) {
    if (!info || !count) return VK_ERROR_SURFACE_LOST_KHR;
    if (!output) return vt_call_vkGetPhysicalDeviceSurfaceFormatsKHR(device, info->surface, count, NULL);
    if (!*count) return VK_INCOMPLETE;
    // Bound allocation by the actual format count, not caller-provided capacity.
    uint32_t available = 0;
    VkResult result = vt_call_vkGetPhysicalDeviceSurfaceFormatsKHR(device, info->surface, &available, NULL);
    if (result != VK_SUCCESS) return result;
    uint32_t capacity = *count < available ? *count : available;
    if (!capacity) { *count = 0; return VK_SUCCESS; }
    if (capacity > CLIENT_RING_BUFFER_SIZE / sizeof(VkSurfaceFormatKHR)) return VK_ERROR_OUT_OF_HOST_MEMORY;
    VkSurfaceFormatKHR* raw = calloc(capacity, sizeof(*raw));
    if (!raw) return VK_ERROR_OUT_OF_HOST_MEMORY;
    uint32_t written = capacity;
    result = vt_call_vkGetPhysicalDeviceSurfaceFormatsKHR(device, info->surface, &written, raw);
    if (result == VK_SUCCESS || result == VK_INCOMPLETE) {
        if (written > capacity) result = VK_ERROR_DEVICE_LOST;
        else {
            for (uint32_t i = 0; i < written; i++) output[i].surfaceFormat = raw[i];
            *count = written; // Preserve caller-owned sType and pNext fields.
        }
    }
    free(raw);
    return result;
}

VkResult vt_call_vkAcquireNextImage2KHR(VkDevice device,
        const VkAcquireNextImageInfoKHR* info, uint32_t* imageIndex) {
    if (!info || !imageIndex) return VK_ERROR_INITIALIZATION_FAILED;
    if (info->deviceMask != 1) return VK_ERROR_FEATURE_NOT_PRESENT;
    // One reply decoder for both entry points; TIMEOUT/NOT_READY are not indices.
    return vt_call_vkAcquireNextImageKHR(device, info->swapchain, info->timeout,
            info->semaphore, info->fence, imageIndex);
}
