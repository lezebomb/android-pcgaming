#include "xwindow_swapchain.h"
#include "vulkan_helper.h"

void getWindowExtent(JMethods* jmethods, int windowId, VkExtent2D* extent) {
    extent->width = (*jmethods->env)->CallIntMethod(jmethods->env, jmethods->obj, jmethods->getWindowWidth, windowId);
    extent->height = (*jmethods->env)->CallIntMethod(jmethods->env, jmethods->obj, jmethods->getWindowHeight, windowId);
}

static AHardwareBuffer* getWindowHardwareBuffer(JMethods* jmethods, int windowId, jboolean useHALPixelFormatBGRA8888, uint64_t usage, uint64_t key) {
    jlong hardwareBufferPtr = (*jmethods->env)->CallLongMethod(jmethods->env, jmethods->obj, jmethods->getWindowHardwareBuffer, windowId, useHALPixelFormatBGRA8888, (jlong)usage, (jlong)key);
    if ((*jmethods->env)->ExceptionCheck(jmethods->env)) {
        (*jmethods->env)->ExceptionClear(jmethods->env);
        return NULL;
    }
    return (AHardwareBuffer*)hardwareBufferPtr;
}

// Resolve the allocation usage from the actual system driver, including any
// vendor bits. sRGB color space is independent of the imported UNORM format.
static VkResult querySurfaceImage(VkPhysicalDevice physicalDevice, VkImageUsageFlags usage,
        VkImageFormatProperties* properties, uint64_t* androidUsage) {
    VkPhysicalDeviceExternalImageFormatInfo external = {0};
    external.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
    external.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;
    VkPhysicalDeviceImageFormatInfo2 info = {0};
    info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    info.pNext = &external;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.type = VK_IMAGE_TYPE_2D;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.flags = VK_IMAGE_CREATE_ALIAS_BIT;
    VkAndroidHardwareBufferUsageANDROID ahbUsage = {0};
    ahbUsage.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID;
    VkExternalImageFormatProperties externalProperties = {0};
    externalProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
    externalProperties.pNext = &ahbUsage;
    VkImageFormatProperties2 resultProperties = {0};
    resultProperties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    resultProperties.pNext = &externalProperties;
    VkResult result = vulkanWrapper.vkGetPhysicalDeviceImageFormatProperties2(physicalDevice, &info, &resultProperties);
    if (result != VK_SUCCESS) return result;
    if (!(externalProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    *properties = resultProperties.imageFormatProperties;
    *androidUsage = ahbUsage.androidHardwareBufferUsage |
            AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    return VK_SUCCESS;
}

static VkResult createImageMemory(VkDevice device, VkImage image, AHardwareBuffer* hardwareBuffer, VkDeviceMemory* pMemory) {
    VkAndroidHardwareBufferFormatPropertiesANDROID format = {0};
    format.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID;
    VkAndroidHardwareBufferPropertiesANDROID ahbProperties = {0};
    ahbProperties.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
    ahbProperties.pNext = &format;
    VkResult propertiesResult = vulkanWrapper.vkGetAndroidHardwareBufferPropertiesANDROID(device, hardwareBuffer, &ahbProperties);
    if (propertiesResult != VK_SUCCESS) return propertiesResult;
    if (format.format != VK_FORMAT_R8G8B8A8_UNORM) {
        println("Vortek: AHB import format mismatch, requested=%d actual=%d", VK_FORMAT_R8G8B8A8_UNORM, format.format);
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

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
    if (memoryInfo.memoryTypeIndex == UINT32_MAX)
        memoryInfo.memoryTypeIndex = getMemoryTypeIndex(ahbProperties.memoryTypeBits, 0);
    if (memoryInfo.memoryTypeIndex == UINT32_MAX) return VK_ERROR_OUT_OF_DEVICE_MEMORY;

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
    VkImageFormatProperties properties = {0};
    uint64_t usage = 0;
    VkResult support = querySurfaceImage(swapchain->physicalDevice, swapchain->imageUsage, &properties, &usage);
    if (support != VK_SUCCESS) return support;
    if (swapchain->imageExtent.width > properties.maxExtent.width ||
        swapchain->imageExtent.height > properties.maxExtent.height) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    AHardwareBuffer* hardwareBuffer = getWindowHardwareBuffer(swapchain->jmethods, swapchain->windowId, JNI_FALSE, usage, (uint64_t)swapchainImage);
    if (!hardwareBuffer) return VK_ERROR_SURFACE_LOST_KHR;

    AHardwareBuffer_Desc ahbDesc = {0};
    AHardwareBuffer_describe(hardwareBuffer, &ahbDesc);
    if (ahbDesc.width != swapchain->imageExtent.width || ahbDesc.height != swapchain->imageExtent.height ||
        ahbDesc.layers != 1 || ahbDesc.format != AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM || (ahbDesc.usage & usage) != usage) {
        AHardwareBuffer_release(hardwareBuffer);
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

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
    return 2;
}

VkResult getSurfaceFormats(VkPhysicalDevice physicalDevice, uint32_t* formatCount, VkSurfaceFormatKHR* formats) {
    VkImageFormatProperties properties = {0};
    uint64_t usage = 0;
    VkResult result = querySurfaceImage(physicalDevice, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &properties, &usage);
    if (result != VK_SUCCESS) { *formatCount = 0; return result; }
    if (formats && *formatCount) {
        formats[0] = (VkSurfaceFormatKHR){VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        *formatCount = 1;
    } else if (formats) return VK_INCOMPLETE;
    else *formatCount = 1;
    return VK_SUCCESS;
}

VkResult XWindowSwapchain_create(VkPhysicalDevice physicalDevice, VkDevice device, uint32_t graphicsQueueIndex, VkSwapchainCreateInfoKHR* swapchainInfo, JMethods* jmethods, int windowId, XWindowSwapchain** output) {
    *output = NULL;
    const VkImageUsageFlags allowed = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if (!physicalDevice || !swapchainInfo->imageUsage || (swapchainInfo->imageUsage & ~allowed) ||
        swapchainInfo->presentMode != VK_PRESENT_MODE_FIFO_KHR ||
        swapchainInfo->preTransform != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ||
        swapchainInfo->compositeAlpha != VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    // Each image has independent AHB storage and ownership commands.
    if (swapchainInfo->minImageCount < 2 || swapchainInfo->minImageCount > 3 || swapchainInfo->imageArrayLayers != 1 ||
        swapchainInfo->imageFormat != VK_FORMAT_R8G8B8A8_UNORM ||
        swapchainInfo->imageColorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ||
        swapchainInfo->imageSharingMode != VK_SHARING_MODE_EXCLUSIVE ||
        swapchainInfo->flags != 0) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    XWindowSwapchain* swapchain = calloc(1, sizeof(XWindowSwapchain));
    if (!swapchain) return VK_ERROR_OUT_OF_HOST_MEMORY;
    swapchain->device = device;
    swapchain->physicalDevice = physicalDevice;
    swapchain->queueFamily = graphicsQueueIndex;
    swapchain->windowId = windowId;
    swapchain->imageCount = swapchainInfo->minImageCount;
    swapchain->images = calloc(swapchain->imageCount, sizeof(XWindowSwapchain_Image));
    if (!swapchain->images) { free(swapchain); return VK_ERROR_OUT_OF_HOST_MEMORY; }
    swapchain->imageFormat = swapchainInfo->imageFormat;
    swapchain->imageUsage = swapchainInfo->imageUsage | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    memcpy(&swapchain->imageExtent, &swapchainInfo->imageExtent, sizeof(VkExtent2D));
    swapchain->jmethods = jmethods;

    VkResult result;
    VkCommandPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    result = vulkanWrapper.vkCreateCommandPool(device, &poolInfo, NULL, &swapchain->transferPool);
    if (result != VK_SUCCESS) goto error;
    VkCommandBuffer commands[6] = {0};
    VkCommandBufferAllocateInfo allocation = {0};
    allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation.commandPool = swapchain->transferPool;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation.commandBufferCount = 2 * swapchain->imageCount;
    result = vulkanWrapper.vkAllocateCommandBuffers(device, &allocation, commands);
    if (result != VK_SUCCESS) goto error;
    for (int i = 0; i < swapchain->imageCount; i++) {
        swapchain->images[i].acquireCommand = commands[2 * i];
        swapchain->images[i].releaseCommand = commands[2 * i + 1];
        result = createImage(device, swapchain, &swapchain->images[i]);
        if (result != VK_SUCCESS) goto error;
    }

    vulkanWrapper.vkGetDeviceQueue(device, graphicsQueueIndex, 0, &swapchain->queue);
    *output = swapchain;
    return VK_SUCCESS;

error:
    for (int i = 0; i < swapchain->imageCount; i++) {
        (*jmethods->env)->CallVoidMethod(jmethods->env, jmethods->obj, jmethods->releaseOutput, (jlong)&swapchain->images[i]);
        if (swapchain->images[i].image) vulkanWrapper.vkDestroyImage(device, swapchain->images[i].image, NULL);
        if (swapchain->images[i].memory) vulkanWrapper.vkFreeMemory(device, swapchain->images[i].memory, NULL);
    }
    if (swapchain->transferPool) vulkanWrapper.vkDestroyCommandPool(device, swapchain->transferPool, NULL);
    MEMFREE(swapchain->images);
    MEMFREE(swapchain);
    return result;
}

void XWindowSwapchain_destroy(VkDevice device, XWindowSwapchain* swapchain) {
    if (!swapchain) return;
    JMethods* methods = swapchain->jmethods;
    for (int i = 0; i < swapchain->imageCount; i++) {
        (*methods->env)->CallVoidMethod(methods->env, methods->obj, methods->releaseOutput, (jlong)&swapchain->images[i]);
        vulkanWrapper.vkDestroyImage(device, swapchain->images[i].image, NULL);
        vulkanWrapper.vkFreeMemory(device, swapchain->images[i].memory, NULL);
    }

    if (swapchain->transferPool) vulkanWrapper.vkDestroyCommandPool(device, swapchain->transferPool, NULL);

    MEMFREE(swapchain->images);
    MEMFREE(swapchain);
}

static VkResult recordOwnershipTransfer(XWindowSwapchain* swapchain, uint32_t index, bool acquire) {
    XWindowSwapchain_Image* target = &swapchain->images[index];
    VkCommandBuffer command = acquire ? target->acquireCommand : target->releaseCommand;
    VkResult result = vulkanWrapper.vkResetCommandBuffer(command, 0);
    if (result != VK_SUCCESS) return result;
    VkCommandBufferBeginInfo begin = {0};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vulkanWrapper.vkBeginCommandBuffer(command, &begin);
    if (result != VK_SUCCESS) return result;
    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = target->image;
    barrier.oldLayout = acquire && !target->everAcquired ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
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
    VkExtent2D windowSize;
    getWindowExtent(swapchain->jmethods, swapchain->windowId, &windowSize);
    if (!windowSize.width || !windowSize.height) return VK_ERROR_SURFACE_LOST_KHR;
    if (swapchain->imageExtent.width != windowSize.width || swapchain->imageExtent.height != windowSize.height)
        return VK_ERROR_OUT_OF_DATE_KHR;
    uint32_t index = swapchain->imageCount;
    for (uint32_t step = 0; step < swapchain->imageCount; step++) {
        uint32_t candidate = (swapchain->nextImage + step) % swapchain->imageCount;
        if (!swapchain->images[candidate].acquired) { index = candidate; break; }
    }
    if (index == swapchain->imageCount) return timeout == 0 ? VK_NOT_READY : VK_TIMEOUT;
    XWindowSwapchain_Image* target = &swapchain->images[index];
    VkResult transferResult = recordOwnershipTransfer(swapchain, index, true);
    if (transferResult != VK_SUCCESS) return transferResult;
    {
        VkSubmitInfo submitInfo = {0};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &target->acquireCommand;
        if (signalSemaphore) {
            submitInfo.pSignalSemaphores = &signalSemaphore;
            submitInfo.signalSemaphoreCount = 1;
        }

        VkResult result = vulkanWrapper.vkQueueSubmit(swapchain->queue, 1, &submitInfo, fence);
        if (result != VK_SUCCESS) return result;
        target->everAcquired = true;
        target->acquired = true;
        swapchain->nextImage = (index + 1) % swapchain->imageCount;
    }

    *imageIndex = index;
    return VK_SUCCESS;
}

VkResult XWindowSwapchain_presentImage(XWindowSwapchain* swapchain, uint32_t imageIndex, uint32_t waitCount, const VkSemaphore* waits) {
    if (imageIndex >= swapchain->imageCount) return VK_ERROR_OUT_OF_DATE_KHR;
    XWindowSwapchain_Image* target = &swapchain->images[imageIndex];
    if (!target->acquired) return VK_ERROR_OUT_OF_DATE_KHR;
    VkResult result = recordOwnershipTransfer(swapchain, imageIndex, false);
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
    submit.pCommandBuffers = &target->releaseCommand;
    result = vulkanWrapper.vkQueueSubmit(swapchain->queue, 1, &submit, VK_NULL_HANDLE);
    free(stages);
    if (result != VK_SUCCESS) return result;
    // Start with explicit completion, before considering asynchronous fence
    // optimizations. The GL copy must see completed Vulkan writes.
    result = vulkanWrapper.vkQueueWaitIdle(swapchain->queue);
    if (result != VK_SUCCESS) return result;
    target->acquired = false;
    JNIEnv* env = swapchain->jmethods->env;
    jint copied = (*env)->CallIntMethod(env, swapchain->jmethods->obj,
            swapchain->jmethods->updateWindowContent, swapchain->windowId, (jlong)target);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        println("Vortek: Java compositor callback failed");
        return VK_ERROR_DEVICE_LOST;
    }
    return (VkResult)copied;
}
