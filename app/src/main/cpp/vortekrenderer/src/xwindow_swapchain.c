#include "xwindow_swapchain.h"
#include "vulkan_helper.h"

void getWindowExtent(JMethods* jmethods, int windowId, VkExtent2D* extent) {
    extent->width = (*jmethods->env)->CallIntMethod(jmethods->env, jmethods->obj, jmethods->getWindowWidth, windowId);
    extent->height = (*jmethods->env)->CallIntMethod(jmethods->env, jmethods->obj, jmethods->getWindowHeight, windowId);
}

static AHardwareBuffer* getWindowHardwareBuffer(JMethods* jmethods, int windowId, jboolean useHALPixelFormatBGRA8888) {
    jlong hardwareBufferPtr = (*jmethods->env)->CallLongMethod(jmethods->env, jmethods->obj, jmethods->getWindowHardwareBuffer, windowId, useHALPixelFormatBGRA8888);
    return (AHardwareBuffer*)hardwareBufferPtr;
}

static VkResult createImageMemory(VkDevice device, VkImage image, AHardwareBuffer* hardwareBuffer, VkDeviceMemory* pMemory) {
    VkAndroidHardwareBufferPropertiesANDROID ahbProperties = {0};
    ahbProperties.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
    VkResult propertiesResult = vulkanWrapper.vkGetAndroidHardwareBufferPropertiesANDROID(device, hardwareBuffer, &ahbProperties);
    if (propertiesResult != VK_SUCCESS) return propertiesResult;

    VkImportAndroidHardwareBufferInfoANDROID memoryImportInfo = {0};
    memoryImportInfo.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    memoryImportInfo.buffer = hardwareBuffer;

    VkMemoryDedicatedAllocateInfo memoryDedicatedInfo = {0};
    memoryDedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    memoryDedicatedInfo.pNext = &memoryImportInfo;
    memoryDedicatedInfo.image = image;
    memoryDedicatedInfo.buffer = VK_NULL_HANDLE;

    VkMemoryAllocateInfo memoryInfo = {0};
    memoryInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryInfo.pNext = &memoryDedicatedInfo;
    memoryInfo.allocationSize = ahbProperties.allocationSize;
    memoryInfo.memoryTypeIndex = getMemoryTypeIndex(ahbProperties.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory memory;
    VkResult result = vulkanWrapper.vkAllocateMemory(device, &memoryInfo, NULL, &memory);
    if (result != VK_SUCCESS) return result;

    result = vulkanWrapper.vkBindImageMemory(device, image, memory, 0);
    if (result != VK_SUCCESS) {
        vulkanWrapper.vkFreeMemory(device, memory, NULL);
        return result;
    }

    *pMemory = memory;
    return VK_SUCCESS;
}

static VkResult createImage(VkDevice device, XWindowSwapchain* swapchain, XWindowSwapchain_Image* swapchainImage) {
    jboolean useHALPixelFormatBGRA8888 = swapchain->imageFormat == VK_FORMAT_B8G8R8A8_UNORM || swapchain->imageFormat == VK_FORMAT_B8G8R8A8_SRGB;
    AHardwareBuffer* hardwareBuffer = getWindowHardwareBuffer(swapchain->jmethods, swapchain->windowId, useHALPixelFormatBGRA8888);
    if (!hardwareBuffer) return VK_ERROR_SURFACE_LOST_KHR;

    AHardwareBuffer_Desc ahbDesc = {0};
    AHardwareBuffer_describe(hardwareBuffer, &ahbDesc);

    VkExternalFormatANDROID externalFormatAndroid = {0};
    externalFormatAndroid.sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
    externalFormatAndroid.externalFormat = 0;

    VkExternalMemoryImageCreateInfo externalMemoryImageInfo = {0};
    externalMemoryImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryImageInfo.pNext = &externalFormatAndroid;
    externalMemoryImageInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

    VkImageCreateInfo imageInfo = {0};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = &externalMemoryImageInfo;
    imageInfo.flags = VK_IMAGE_CREATE_ALIAS_BIT;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = swapchain->imageFormat;
    imageInfo.extent.width = ahbDesc.width;
    imageInfo.extent.height = ahbDesc.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = swapchain->imageUsage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image;
    VkDeviceMemory memory;
    VkResult result;

    result = vulkanWrapper.vkCreateImage(device, &imageInfo, NULL, &image);
    if (result != VK_SUCCESS) {
        AHardwareBuffer_release(hardwareBuffer);
        return result;
    }

    result = createImageMemory(device, image, hardwareBuffer, &memory);
    // Java lends an acquired reference across the JNI callback. Vulkan keeps
    // its own import reference on success; every failure releases ours too.
    AHardwareBuffer_release(hardwareBuffer);
    if (result != VK_SUCCESS) {
        vulkanWrapper.vkDestroyImage(device, image, NULL);
        return result;
    }

    swapchainImage->image = image;
    swapchainImage->memory = memory;
    return VK_SUCCESS;
}

int getSurfaceMinImageCount() {
    return 1;
}

VkSurfaceFormatKHR* getSurfaceFormats(uint32_t* formatCount) {
    static const VkFormat supportedFormats[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB};
    int supportedFormatCount = ARRAY_SIZE(supportedFormats);
    VkSurfaceFormatKHR* surfaceFormats = calloc(supportedFormatCount, sizeof(VkSurfaceFormatKHR));

    if (formatCount) *formatCount = supportedFormatCount;

    for (int i = 0; i < supportedFormatCount; i++) {
        surfaceFormats[i].format = supportedFormats[i];
        surfaceFormats[i].colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }

    return surfaceFormats;
}

XWindowSwapchain* XWindowSwapchain_create(VkDevice device, uint32_t graphicsQueueIndex, VkSwapchainCreateInfoKHR* swapchainInfo, JMethods* jmethods, int windowId) {
    // This conservative backend exposes one independently synchronized output.
    if (swapchainInfo->minImageCount != 1) return NULL;
    XWindowSwapchain* swapchain = calloc(1, sizeof(XWindowSwapchain));
    if (!swapchain) return NULL;
    swapchain->device = device;
    swapchain->queueFamily = graphicsQueueIndex;
    swapchain->windowId = windowId;
    swapchain->imageCount = swapchainInfo->minImageCount;
    swapchain->images = calloc(swapchain->imageCount, sizeof(XWindowSwapchain_Image));
    if (!swapchain->images) { free(swapchain); return NULL; }
    swapchain->imageFormat = swapchainInfo->imageFormat;
    swapchain->imageUsage = swapchainInfo->imageUsage;
    memcpy(&swapchain->imageExtent, &swapchainInfo->imageExtent, sizeof(VkExtent2D));
    swapchain->jmethods = jmethods;

    VkResult result;
    VkCommandPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    result = vulkanWrapper.vkCreateCommandPool(device, &poolInfo, NULL, &swapchain->transferPool);
    if (result != VK_SUCCESS) goto error;
    VkCommandBuffer commands[2] = {0};
    VkCommandBufferAllocateInfo allocation = {0};
    allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation.commandPool = swapchain->transferPool;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation.commandBufferCount = 2;
    result = vulkanWrapper.vkAllocateCommandBuffers(device, &allocation, commands);
    if (result != VK_SUCCESS) goto error;
    swapchain->acquireCommand = commands[0];
    swapchain->releaseCommand = commands[1];
    for (int i = 0; i < swapchain->imageCount; i++) {
        result = createImage(device, swapchain, &swapchain->images[i]);
        if (result != VK_SUCCESS) goto error;
    }

    vulkanWrapper.vkGetDeviceQueue(device, graphicsQueueIndex, 0, &swapchain->queue);
    return swapchain;

error:
    for (int i = 0; i < swapchain->imageCount; i++) {
        if (swapchain->images[i].image) vulkanWrapper.vkDestroyImage(device, swapchain->images[i].image, NULL);
        if (swapchain->images[i].memory) vulkanWrapper.vkFreeMemory(device, swapchain->images[i].memory, NULL);
    }
    if (swapchain->transferPool) vulkanWrapper.vkDestroyCommandPool(device, swapchain->transferPool, NULL);
    MEMFREE(swapchain->images);
    MEMFREE(swapchain);
    return NULL;
}

void XWindowSwapchain_destroy(VkDevice device, XWindowSwapchain* swapchain) {
    if (!swapchain) return;
    for (int i = 0; i < swapchain->imageCount; i++) {
        vulkanWrapper.vkDestroyImage(device, swapchain->images[i].image, NULL);
        vulkanWrapper.vkFreeMemory(device, swapchain->images[i].memory, NULL);
    }

    if (swapchain->transferPool) vulkanWrapper.vkDestroyCommandPool(device, swapchain->transferPool, NULL);

    MEMFREE(swapchain->images);
    MEMFREE(swapchain);
}

static VkResult recordOwnershipTransfer(XWindowSwapchain* swapchain, bool acquire) {
    VkCommandBuffer command = acquire ? swapchain->acquireCommand : swapchain->releaseCommand;
    VkResult result = vulkanWrapper.vkResetCommandBuffer(command, 0);
    if (result != VK_SUCCESS) return result;
    VkCommandBufferBeginInfo begin = {0};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vulkanWrapper.vkBeginCommandBuffer(command, &begin);
    if (result != VK_SUCCESS) return result;
    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = swapchain->images[0].image;
    barrier.oldLayout = acquire && !swapchain->everAcquired ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = acquire ? VK_QUEUE_FAMILY_FOREIGN_EXT : swapchain->queueFamily;
    barrier.dstQueueFamilyIndex = acquire ? swapchain->queueFamily : VK_QUEUE_FAMILY_FOREIGN_EXT;
    barrier.srcAccessMask = acquire ? 0 : VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = acquire ? VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT : 0;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vulkanWrapper.vkCmdPipelineBarrier(command,
            acquire ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            acquire ? VK_PIPELINE_STAGE_ALL_COMMANDS_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, NULL, 0, NULL, 1, &barrier);
    return vulkanWrapper.vkEndCommandBuffer(command);
}

VkResult XWindowSwapchain_acquireNextImage(XWindowSwapchain* swapchain, uint64_t timeout, VkSemaphore signalSemaphore, VkFence fence, uint32_t* imageIndex) {
    VkResult transferResult = recordOwnershipTransfer(swapchain, true);
    if (transferResult != VK_SUCCESS) return transferResult;
    {
        VkSubmitInfo submitInfo = {0};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &swapchain->acquireCommand;
        if (signalSemaphore) {
            submitInfo.pSignalSemaphores = &signalSemaphore;
            submitInfo.signalSemaphoreCount = 1;
        }

        VkResult result = vulkanWrapper.vkQueueSubmit(swapchain->queue, 1, &submitInfo, fence);
        if (result != VK_SUCCESS) return result;
        swapchain->everAcquired = true;
    }

    VkExtent2D windowSize;
    getWindowExtent(swapchain->jmethods, swapchain->windowId, &windowSize);

    VkResult result = VK_SUCCESS;
    if (swapchain->imageExtent.width != windowSize.width || swapchain->imageExtent.height != windowSize.height) {
        result = VK_ERROR_SURFACE_LOST_KHR;
    }

    *imageIndex = 0;
    return result;
}

VkResult XWindowSwapchain_presentImage(XWindowSwapchain* swapchain, uint32_t waitCount, const VkSemaphore* waits) {
    VkResult result = recordOwnershipTransfer(swapchain, false);
    if (result != VK_SUCCESS) return result;
    VkPipelineStageFlags* stages = waitCount ? calloc(waitCount, sizeof(VkPipelineStageFlags)) : NULL;
    if (waitCount && !stages) return VK_ERROR_OUT_OF_HOST_MEMORY;
    for (uint32_t i = 0; i < waitCount; i++) stages[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = waitCount;
    submit.pWaitSemaphores = waits;
    submit.pWaitDstStageMask = stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &swapchain->releaseCommand;
    result = vulkanWrapper.vkQueueSubmit(swapchain->queue, 1, &submit, VK_NULL_HANDLE);
    free(stages);
    if (result != VK_SUCCESS) return result;
    // Start with explicit completion, before considering asynchronous fence
    // optimizations. The GL copy must see completed Vulkan writes.
    result = vulkanWrapper.vkQueueWaitIdle(swapchain->queue);
    if (result != VK_SUCCESS) return result;
    JNIEnv* env = swapchain->jmethods->env;
    jboolean copied = (*env)->CallBooleanMethod(env, swapchain->jmethods->obj,
                                                swapchain->jmethods->updateWindowContent, swapchain->windowId);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        println("Vortek: Java compositor callback failed");
        return VK_ERROR_SURFACE_LOST_KHR;
    }
    return copied ? VK_SUCCESS : VK_ERROR_SURFACE_LOST_KHR;
}
