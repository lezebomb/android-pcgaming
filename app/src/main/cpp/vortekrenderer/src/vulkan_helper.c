#include <string.h>

#include "vulkan_helper.h"
#include "dma_utils.h"
#include "dimensity_upload.h"
#include "dimensity_wsi.h"

#define VK_MAKE_VERSION_STR(s, v) sprintf(s, "%d.%d.%d", VK_VERSION_MAJOR(v), VK_VERSION_MINOR(v), VK_VERSION_PATCH(v))

DeviceMemoryInfo deviceMemoryInfo = {0};

#if ENABLE_VALIDATION_LAYER
static VkBool32 debugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT type, uint64_t object, size_t location, int32_t messageCode, const char *pLayerPrefix, const char *pMessage, void *pUserData) {
    println("Vulkan (%s): %s", pLayerPrefix, pMessage);
    return VK_FALSE;
}
#endif

static bool isExternalMemoryHandleTypeSupported(VkPhysicalDevice physicalDevice, VkExternalMemoryHandleTypeFlagBits handleType) {
    VkPhysicalDeviceExternalBufferInfo bufferInfo = {0};
    bufferInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO;
    bufferInfo.handleType = handleType;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VkExternalBufferProperties bufferProperties = {0};
    bufferProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES;
    vulkanWrapper.vkGetPhysicalDeviceExternalBufferProperties(physicalDevice, &bufferInfo, &bufferProperties);
    VkExternalMemoryProperties* memoryProperties = &bufferProperties.externalMemoryProperties;

    VkExternalMemoryFeatureFlags required = handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
            ? VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT : VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
    return (memoryProperties->externalMemoryFeatures & required) == required &&
           (memoryProperties->compatibleHandleTypes & handleType) != 0;
}

static void setupExposedDeviceExtensions(VkContext* context) {
    if (!context->engineName) return;

    ArrayList_free(context->disabledDeviceExtensions, true);
    context->disabledDeviceExtensions = NULL;

    if (strcmp(context->engineName, "DXVK") == 0) {
        bool isMaliDevice = context->driverID == VK_DRIVER_ID_ARM_PROPRIETARY;
        bool isXclipseDevice = context->driverID == VK_DRIVER_ID_SAMSUNG_PROPRIETARY;
        bool isAdrenoDevice = context->driverID == VK_DRIVER_ID_QUALCOMM_PROPRIETARY;

        if ((isMaliDevice || isXclipseDevice) && context->engineVersion < MAKE_ENGINE_VERSION(2, 0, 0)) {
            const char* disabledDeviceExtensions[] = {"VK_EXT_extended_dynamic_state", "VK_EXT_extended_dynamic_state2", "VK_EXT_extended_dynamic_state3", "VK_EXT_hdr_metadata", "VK_EXT_swapchain_maintenance1"};
            context->disabledDeviceExtensions = ArrayList_fromStrings(disabledDeviceExtensions, ARRAY_SIZE(disabledDeviceExtensions));
        }
        else if (isAdrenoDevice) {
            const char* disabledDeviceExtensions[] = {"VK_KHR_shader_float_controls", "VK_EXT_hdr_metadata", "VK_EXT_swapchain_maintenance1"};
            context->disabledDeviceExtensions = ArrayList_fromStrings(disabledDeviceExtensions, ARRAY_SIZE(disabledDeviceExtensions));
        }
        else {
            const char* disabledDeviceExtensions[] = {"VK_EXT_hdr_metadata", "VK_EXT_swapchain_maintenance1"};
            context->disabledDeviceExtensions = ArrayList_fromStrings(disabledDeviceExtensions, ARRAY_SIZE(disabledDeviceExtensions));
        }
    }
    else if (strcmp(context->engineName, "mesa zink") == 0) {
        const char* disabledDeviceExtensions[] = {"VK_EXT_extended_dynamic_state", "VK_EXT_extended_dynamic_state2", "VK_EXT_extended_dynamic_state3", "VK_EXT_color_write_enable", "VK_KHR_push_descriptor"};
        context->disabledDeviceExtensions = ArrayList_fromStrings(disabledDeviceExtensions, ARRAY_SIZE(disabledDeviceExtensions));
        ArrayList_free(context->exposedDeviceExtensions, true);
        context->exposedDeviceExtensions = ArrayList_fromStrings(globalExposedDeviceExtensions, ARRAY_SIZE(globalExposedDeviceExtensions));
    }
}

void initVulkanInstance(VkContext* context, VkInstance instance, const VkApplicationInfo* applicationInfo) {
    VkResult result;

    loadVulkanInstanceFuncs(&vulkanWrapper, instance);

    uint32_t physicalDeviceCount = 1;
    result = vulkanWrapper.vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, NULL);
    if (result != VK_SUCCESS) return;

    VkPhysicalDevice physicalDevices[physicalDeviceCount];
    result = vulkanWrapper.vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices);
    if (result != VK_SUCCESS || physicalDeviceCount == 0) return;

    if (deviceMemoryInfo.memoryTypeCount == 0) {
        VkPhysicalDeviceMemoryProperties memoryProperties;
        vulkanWrapper.vkGetPhysicalDeviceMemoryProperties(physicalDevices[0], &memoryProperties);

        deviceMemoryInfo.memoryTypes = malloc(memoryProperties.memoryTypeCount * sizeof(VkMemoryType));
        memcpy(deviceMemoryInfo.memoryTypes, memoryProperties.memoryTypes, memoryProperties.memoryTypeCount * sizeof(VkMemoryType));
        deviceMemoryInfo.memoryTypeCount = memoryProperties.memoryTypeCount;

        VkDeviceSize maxHeapSize = 0;
        for (int i = 0; i < memoryProperties.memoryHeapCount; i++) maxHeapSize = MAX(maxHeapSize, memoryProperties.memoryHeaps[i].size);
        deviceMemoryInfo.maxAllocationSize = maxHeapSize * 2 / 3;
    }

    VkPhysicalDeviceDriverProperties driverProperties = {0};
    driverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
    VkPhysicalDeviceProperties2 properties2 = {0};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &driverProperties;
    vulkanWrapper.vkGetPhysicalDeviceProperties2(physicalDevices[0], &properties2);
    context->driverID = driverProperties.driverID;

    context->hasExternalMemoryFd = isExternalMemoryHandleTypeSupported(physicalDevices[0], VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT);
    context->hasExternalMemoryDMABuf = isExternalMemoryHandleTypeSupported(physicalDevices[0], VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);

    MEMFREE(context->engineName);
    context->engineVersion = 0;

    if (applicationInfo) {
        context->engineName = applicationInfo->pEngineName ? strdup(applicationInfo->pEngineName) : NULL;
        context->engineVersion = applicationInfo->engineVersion;
    }

    setupExposedDeviceExtensions(context);

#if ENABLE_VALIDATION_LAYER
    VkDebugReportCallbackCreateInfoEXT debugCreateInfo = {0};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    debugCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debugCreateInfo.pfnCallback = debugReportCallback;

    vulkanWrapper.vkCreateDebugReportCallback(instance, &debugCreateInfo, NULL, &context->debugReportCallback);
#endif
}

void initVulkanDevice(VkContext* context, VkPhysicalDevice physicalDevice, VkDevice device) {
    context->physicalDevice = physicalDevice;
    loadVulkanDeviceFuncs(&vulkanWrapper, device);

    uint32_t queueFamilyCount = 0;
    vulkanWrapper.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);

    VkQueueFamilyProperties queueFamilies[queueFamilyCount];
    vulkanWrapper.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

    context->graphicsQueueIndex = 0;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueCount > 0 && (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            context->graphicsQueueIndex = i;
            break;
        }
    }

    VkPhysicalDeviceFeatures supportedFeatures = {0};
    vulkanWrapper.vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

    if (!context->textureDecoder) context->textureDecoder = TextureDecoder_create(context, physicalDevice, device, &supportedFeatures);
    if (!context->shaderInspector) context->shaderInspector = ShaderInspector_create(context, physicalDevice, &supportedFeatures);
    if (context->textureDecoder) context->textureDecoder->threadPool = context->threadPool;
}

uint32_t getMemoryTypeIndex(uint32_t typeBits, VkFlags properties) {
    return dgSelectMemoryType(deviceMemoryInfo.memoryTypes, deviceMemoryInfo.memoryTypeCount, typeBits, properties);
}

uint32_t getMemoryPropertyFlags(uint32_t memoryTypeIndex) {
    if (deviceMemoryInfo.memoryTypeCount == 0 || memoryTypeIndex >= deviceMemoryInfo.memoryTypeCount) return 0;
    return deviceMemoryInfo.memoryTypes[memoryTypeIndex].propertyFlags;
}

bool isHostVisibleMemory(uint32_t memoryTypeIndex) {
    if (deviceMemoryInfo.memoryTypeCount == 0 || memoryTypeIndex >= deviceMemoryInfo.memoryTypeCount) return false;
    VkMemoryType* memoryType = &deviceMemoryInfo.memoryTypes[memoryTypeIndex];
    return (memoryType->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
}

void injectExtensions(VkContext* context, char*** extensions, uint32_t* extensionCount, const char* const* extraExtensions, uint32_t extraExtensionCount, const char* const* skipExtensions, uint32_t skipExtensionCount) {
    if (skipExtensions) {
        uint32_t oldExtensionCount = *extensionCount;
        char** oldExtensions = *extensions;

        bool skipped[oldExtensionCount];
        uint32_t newExtensionCount = 0;
        for (int i = 0; i < oldExtensionCount; i++) {
            skipped[i] = false;
            for (int j = 0; j < skipExtensionCount; j++) {
                if (strcmp(skipExtensions[j], oldExtensions[i]) == 0) {
                    skipped[i] = true;
                    break;
                }
            }

            if (!skipped[i]) newExtensionCount++;
        }

        char** newExtensions = vt_alloc(&context->memoryPool, newExtensionCount * sizeof(char*));
        for (int i = 0, j = 0; i < oldExtensionCount; i++) if (!skipped[i]) newExtensions[j++] = oldExtensions[i];

        *extensions = newExtensions;
        *extensionCount = newExtensionCount;
    }

    uint32_t newExtensionCount = *extensionCount;
    bool founds[extraExtensionCount];
    for (int i = 0; i < extraExtensionCount; i++) {
        founds[i] = false;
        for (int j = 0; j < *extensionCount; j++) {
            if (strcmp(extraExtensions[i], (*extensions)[j]) == 0) {
                founds[i] = true;
                break;
            }
        }

        if (!founds[i]) newExtensionCount++;
    }

    if (newExtensionCount == *extensionCount) return;

    char** newExtensions = vt_alloc(&context->memoryPool, newExtensionCount * sizeof(char*));
    for (int i = 0, j = 0; i < extraExtensionCount; i++) {
        if (!founds[i]) {
            char* value = vt_alloc(&context->memoryPool, strlen(extraExtensions[i]) + 1);
            strcpy(value, extraExtensions[i]);
            newExtensions[(*extensionCount) + j++] = value;
        }
    }

    for (int i = 0; i < *extensionCount; i++) newExtensions[i] = (*extensions)[i];

    *extensions = newExtensions;
    *extensionCount = newExtensionCount;
}

void injectExtensions2(VkContext* context, VkExtensionProperties** extensions, uint32_t* extensionCount, const char* const* extraExtensions, uint32_t extraExtensionCount, const char* const* skipExtensions, uint32_t skipExtensionCount) {
    if (skipExtensions) {
        uint32_t oldExtensionCount = *extensionCount;
        VkExtensionProperties* oldExtensions = *extensions;

        bool skipped[oldExtensionCount];
        uint32_t newExtensionCount = 0;
        for (int i = 0; i < oldExtensionCount; i++) {
            skipped[i] = false;
            for (int j = 0; j < skipExtensionCount; j++) {
                if (strcmp(skipExtensions[j], oldExtensions[i].extensionName) == 0) {
                    skipped[i] = true;
                    break;
                }
            }

            if (!skipped[i]) newExtensionCount++;
        }

        VkExtensionProperties* newExtensions = vt_alloc(&context->memoryPool, newExtensionCount * sizeof(VkExtensionProperties));
        for (int i = 0, j = 0; i < oldExtensionCount; i++) {
            if (!skipped[i]) memcpy(&newExtensions[j++], &oldExtensions[i], sizeof(VkExtensionProperties));
        }

        *extensions = newExtensions;
        *extensionCount = newExtensionCount;
    }

    uint32_t newExtensionCount = *extensionCount;
    bool founds[extraExtensionCount];
    for (int i = 0; i < extraExtensionCount; i++) {
        founds[i] = false;
        for (int j = 0; j < *extensionCount; j++) {
            if (strcmp(extraExtensions[i], (*extensions)[j].extensionName) == 0) {
                founds[i] = true;
                break;
            }
        }

        if (!founds[i]) newExtensionCount++;
    }

    if (newExtensionCount == *extensionCount) return;

    VkExtensionProperties* newExtensions = vt_alloc(&context->memoryPool, newExtensionCount * sizeof(VkExtensionProperties));
    for (int i = 0, j = 0; i < extraExtensionCount; i++) {
        if (!founds[i]) {
            VkExtensionProperties* properties = &newExtensions[(*extensionCount) + j++];
            strcpy(properties->extensionName, extraExtensions[i]);
            properties->specVersion = 1;
        }
    }

    for (int i = 0; i < *extensionCount; i++) memcpy(&newExtensions[i], &(*extensions)[i], sizeof(VkExtensionProperties));

    *extensions = newExtensions;
    *extensionCount = newExtensionCount;
}

void checkDeviceMemoryProperties(VkContext* context, VkPhysicalDeviceMemoryProperties* memoryProperties, void* pNext) {
    VkDeviceSize maxHeapSize = (VkDeviceSize)context->maxDeviceMemory << 20;
    if (maxHeapSize == 0) maxHeapSize = deviceMemoryInfo.maxAllocationSize;

    for (int i = 0; i < memoryProperties->memoryHeapCount; i++) {
        memoryProperties->memoryHeaps[i].size = MIN(memoryProperties->memoryHeaps[i].size, maxHeapSize);
    }

    VkPhysicalDeviceMemoryBudgetPropertiesEXT* memoryBudgetProperties = findNextVkStructure(pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT);
    if (memoryBudgetProperties) {
        memoryBudgetProperties->heapUsage[0] = context->totalAllocationSize;
        memoryBudgetProperties->heapBudget[0] = maxHeapSize > 0 ? maxHeapSize : deviceMemoryInfo.maxAllocationSize;
    }
}

void disableUnsupportedDeviceFeatures(VkPhysicalDevice physicalDevice, VkDeviceCreateInfo* createInfo) {
    VkPhysicalDeviceTransformFeedbackFeaturesEXT* transformFeedbackFeatures = findNextVkStructure(createInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT);
    if (transformFeedbackFeatures) {
        VkPhysicalDeviceFeatures2 features = {0};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = transformFeedbackFeatures;
        vulkanWrapper.vkGetPhysicalDeviceFeatures2(physicalDevice, &features);
    }

    VkPhysicalDeviceFeatures* enabledFeatures = (VkPhysicalDeviceFeatures*)createInfo->pEnabledFeatures;
    VkPhysicalDeviceFeatures2* features2 = NULL;
    if (!enabledFeatures) {
        features2 = findNextVkStructure((void*)createInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
        if (features2) enabledFeatures = &features2->features;
    }

    if (enabledFeatures) {
        VkPhysicalDeviceFeatures supportedFeatures = {0};
        vulkanWrapper.vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

        for (int offset = 0; offset < sizeof(VkPhysicalDeviceFeatures); offset += sizeof(VkBool32)) {
            VkBool32 srcValue = *(VkBool32*)(((char*)&supportedFeatures) + offset);
            VkBool32 dstValue = *(VkBool32*)(((char*)enabledFeatures) + offset);
            if (dstValue && !srcValue) *(VkBool32*)(((char*)enabledFeatures) + offset) = VK_FALSE;
        }
    }
}

VkExtensionProperties* getExposedDeviceExtensionProperties(VkContext* context, VkPhysicalDevice physicalDevice, uint32_t* propertyCount) {
    uint32_t srcPropertyCount;
    VkResult result = vulkanWrapper.vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &srcPropertyCount, NULL);
    if (result != VK_SUCCESS) return NULL;

    VkExtensionProperties srcProperties[srcPropertyCount];
    result = vulkanWrapper.vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &srcPropertyCount, srcProperties);
    if (result != VK_SUCCESS) return NULL;

    int dstPropertyCount = context->exposedDeviceExtensions ? context->exposedDeviceExtensions->size : srcPropertyCount;
    VkExtensionProperties* dstProperties = vt_alloc(&context->memoryPool, dstPropertyCount * sizeof(VkExtensionProperties));
    *propertyCount = 0;

    for (int i = 0, j; i < dstPropertyCount; i++) {
        bool expose = true;
        VkExtensionProperties* srcProperty = NULL;
        if (context->exposedDeviceExtensions) {
            expose = false;
            char* extensionName = context->exposedDeviceExtensions->elements[i];

            for (j = 0; j < srcPropertyCount; j++) {
                if (strcmp(extensionName, srcProperties[j].extensionName) == 0) {
                    expose = true;
                    srcProperty = &srcProperties[j];
                    break;
                }
            }
        }
        else srcProperty = &srcProperties[i];
        if (!expose || dgUnsupportedDeviceExtension(srcProperty->extensionName)) continue;

        if (context->disabledDeviceExtensions) {
            for (j = 0; j < context->disabledDeviceExtensions->size; j++) {
                char* extensionName = context->disabledDeviceExtensions->elements[j];
                if (strcmp(srcProperty->extensionName, extensionName) == 0) {
                    expose = false;
                    break;
                }
            }
        }

        if (expose) {
            int index = (*propertyCount)++;
            strcpy(dstProperties[index].extensionName, srcProperty->extensionName);
            dstProperties[index].specVersion = srcProperty->specVersion;
        }
    }

    injectExtensions2(context, &dstProperties, propertyCount,
                      globalImplementedDeviceExtensions, ARRAY_SIZE(globalImplementedDeviceExtensions), NULL, 0);
    return dstProperties;
}

void checkFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* formatProperties) {
    if (TextureDecoder_canDecompressFormat(format) && formatProperties->linearTilingFeatures == 0 && formatProperties->optimalTilingFeatures == 0) {
        vulkanWrapper.vkGetPhysicalDeviceFormatProperties(physicalDevice, TextureDecoder_decodedFormat(format), formatProperties);
        formatProperties->linearTilingFeatures = 0;
        formatProperties->bufferFeatures = 0;
        formatProperties->optimalTilingFeatures &= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT;
    }
    else if (isFormatScaled(format)) {
        formatProperties->bufferFeatures |= VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT;
    }
}

void checkImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* imageFormatProperties, VkResult* result) {
    VkImageUsageFlags supported = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (*result == VK_ERROR_FORMAT_NOT_SUPPORTED && TextureDecoder_canDecompressFormat(format) &&
        type == VK_IMAGE_TYPE_2D && tiling == VK_IMAGE_TILING_OPTIMAL && !(usage & ~supported)) {
        *result = vulkanWrapper.vkGetPhysicalDeviceImageFormatProperties(physicalDevice,
                TextureDecoder_decodedFormat(format), type, tiling, usage,
                (flags & ~VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT) | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
                imageFormatProperties);
        if (*result == VK_SUCCESS) imageFormatProperties->sampleCounts = VK_SAMPLE_COUNT_1_BIT;
    }
}

void checkDeviceProperties(VkContext* context, VkPhysicalDeviceProperties* properties, void* pNext) {
    char deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE] = {0};
    sprintf(deviceName, DEVICE_NAME, properties->deviceName);
    strcpy(properties->deviceName, deviceName);
    properties->apiVersion = MIN(properties->apiVersion, context->vkMaxVersion);

    VkPhysicalDeviceVulkan12Properties* vulkan12Properties = findNextVkStructure(pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES);
    if (vulkan12Properties) {
        VK_MAKE_VERSION_STR(vulkan12Properties->driverInfo, properties->driverVersion);

        if (context->disabledDeviceExtensions) {
            bool floatControlsEnabled = true;
            for (int i = 0; i < context->disabledDeviceExtensions->size && floatControlsEnabled; i++) {
                if (strcmp((const char*)context->disabledDeviceExtensions->elements[i], "VK_KHR_shader_float_controls") == 0) floatControlsEnabled = false;
            }

            if (!floatControlsEnabled) {
                vulkan12Properties->shaderSignedZeroInfNanPreserveFloat32 = VK_FALSE;
                vulkan12Properties->shaderSignedZeroInfNanPreserveFloat64 = VK_FALSE;
                vulkan12Properties->shaderDenormFlushToZeroFloat32 = VK_FALSE;
                vulkan12Properties->shaderDenormFlushToZeroFloat64 = VK_FALSE;
                vulkan12Properties->shaderDenormPreserveFloat32 = VK_FALSE;
                vulkan12Properties->shaderDenormPreserveFloat64 = VK_FALSE;
            }
        }
    }

    VkPhysicalDeviceDriverProperties* driverProperties = findNextVkStructure(pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES);
    if (driverProperties) VK_MAKE_VERSION_STR(driverProperties->driverInfo, properties->driverVersion);

    VkPhysicalDeviceMapMemoryPlacedPropertiesEXT* mapPlacedProperties = findNextVkStructure(pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_PROPERTIES_EXT);
    if (mapPlacedProperties) mapPlacedProperties->minPlacedMemoryMapAlignment = getpagesize();
}

void checkDeviceFeatures(VkPhysicalDeviceFeatures* features, void* pNext) {
    // Preserve native core features. BC1-5 decoding is advertised per format;
    // it does not establish full BC6/7, tessellation or other missing features.

    VkPhysicalDeviceMapMemoryPlacedFeaturesEXT* mapMemoryPlacedFeatures = findNextVkStructure(pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_FEATURES_EXT);
    if (mapMemoryPlacedFeatures) {
        mapMemoryPlacedFeatures->memoryMapPlaced = VK_TRUE;
        mapMemoryPlacedFeatures->memoryMapRangePlaced = VK_FALSE;
        mapMemoryPlacedFeatures->memoryUnmapReserve = VK_TRUE;
    }
}

void destroyVkObject(VkObjectType type, VkDevice device, void* handle) {
    if (!handle) return;
    switch (type) {
        case VK_OBJECT_TYPE_INSTANCE:
            vulkanWrapper.vkDestroyInstance(handle, NULL);
            break;
        case VK_OBJECT_TYPE_DEVICE:
            vulkanWrapper.vkDestroyDevice(handle, NULL);
            break;
        case VK_OBJECT_TYPE_SEMAPHORE:
            vulkanWrapper.vkDestroySemaphore(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_FENCE:
            vulkanWrapper.vkDestroyFence(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_BUFFER:
            vulkanWrapper.vkDestroyBuffer(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_IMAGE:
            vulkanWrapper.vkDestroyImage(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_EVENT:
            vulkanWrapper.vkDestroyEvent(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_QUERY_POOL:
            vulkanWrapper.vkDestroyQueryPool(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_BUFFER_VIEW:
            vulkanWrapper.vkDestroyBufferView(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_IMAGE_VIEW:
            vulkanWrapper.vkDestroyImageView(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_SHADER_MODULE: {
            ShaderModule* shaderModule = handle;
            if (shaderModule->module) vulkanWrapper.vkDestroyShaderModule(device, shaderModule->module, NULL);
            MEMFREE(shaderModule->code);
            MEMFREE(shaderModule);
            break;
        }
        case VK_OBJECT_TYPE_PIPELINE_CACHE:
            vulkanWrapper.vkDestroyPipelineCache(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
            vulkanWrapper.vkDestroyPipelineLayout(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_RENDER_PASS:
            vulkanWrapper.vkDestroyRenderPass(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_PIPELINE: {
            vulkanWrapper.vkDestroyPipeline(device, handle, NULL);
            break;
        }
        case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
            vulkanWrapper.vkDestroyDescriptorSetLayout(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_SAMPLER:
            vulkanWrapper.vkDestroySampler(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
            vulkanWrapper.vkDestroyDescriptorPool(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_FRAMEBUFFER:
            vulkanWrapper.vkDestroyFramebuffer(device, handle, NULL);
            break;
        case VK_OBJECT_TYPE_COMMAND_POOL:
            vulkanWrapper.vkDestroyCommandPool(device, handle, NULL);
            break;
    }
}
