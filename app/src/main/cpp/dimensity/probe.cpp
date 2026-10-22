#include <jni.h>
#include <vulkan/vulkan.h>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

static std::string quote(const char* text) {
    std::string out = "\"";
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
        if (*p == '\\' || *p == '"') { out += '\\'; out += *p; }
        else if (*p >= 32) out += *p;
    }
    return out + "\"";
}
static std::string version(uint32_t v) {
    return std::to_string(VK_VERSION_MAJOR(v)) + "." + std::to_string(VK_VERSION_MINOR(v)) + "." + std::to_string(VK_VERSION_PATCH(v));
}
static std::string inspect() {
    uint32_t loader = VK_API_VERSION_1_0;
    auto enumerateVersion = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion");
    if (enumerateVersion) enumerateVersion(&loader);
    const uint32_t api = loader >= VK_API_VERSION_1_1 ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0;
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Dimensity Gaming capability probe";
    app.apiVersion = api;
    VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create.pApplicationInfo = &app;
    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&create, nullptr, &instance);
    if (result != VK_SUCCESS) return "{\"status\":\"unavailable\",\"vk_result\":" + std::to_string(result) + "}";
    uint32_t count = 0;
    result = vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (result != VK_SUCCESS || !count) { vkDestroyInstance(instance, nullptr); return "{\"status\":\"unavailable: no physical device\"}"; }
    std::vector<VkPhysicalDevice> devices(count);
    result = vkEnumeratePhysicalDevices(instance, &count, devices.data());
    if (result != VK_SUCCESS) { vkDestroyInstance(instance, nullptr); return "{\"status\":\"unavailable: device enumeration failed\"}"; }
    std::ostringstream out;
    out << "{\"status\":\"ok\",\"loader_api\":" << quote(version(loader).c_str()) << ",\"devices\":[";
    for (uint32_t d = 0; d < count; ++d) {
        if (d) out << ',';
        auto device = devices[d];
        VkPhysicalDeviceProperties props{};
        VkPhysicalDeviceFeatures features{};
        vkGetPhysicalDeviceProperties(device, &props);
        vkGetPhysicalDeviceFeatures(device, &features);
        out << "{\"name\":" << quote(props.deviceName) << ",\"api_version\":" << quote(version(props.apiVersion).c_str())
            << ",\"vendor_id\":" << props.vendorID << ",\"driver_version_raw\":" << props.driverVersion
            << ",\"texture_compression_bc\":" << (features.textureCompressionBC ? "true" : "false")
            << ",\"texture_compression_astc_ldr\":" << (features.textureCompressionASTC_LDR ? "true" : "false")
            << ",\"texture_compression_etc2\":" << (features.textureCompressionETC2 ? "true" : "false");
        uint32_t n = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &n, nullptr);
        std::vector<VkExtensionProperties> extensions(n);
        VkResult extResult = vkEnumerateDeviceExtensionProperties(device, nullptr, &n, extensions.data());
        bool budget = false, ahb = false;
        out << ",\"extensions\":[";
        for (uint32_t i = 0; extResult == VK_SUCCESS && i < n; ++i) {
            if (i) out << ',';
            out << quote(extensions[i].extensionName);
            budget |= !strcmp(extensions[i].extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
            ahb |= !strcmp(extensions[i].extensionName, "VK_ANDROID_external_memory_android_hardware_buffer");
        }
        out << "],\"ahardwarebuffer_extension\":" << (ahb ? "true" : "false")
            << ",\"ahardwarebuffer_usage_compatibility\":\"unavailable: no image allocation performed\"";
        struct Format { VkFormat format; const char* name; } formats[] = {
            {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, "BC1_RGBA"}, {VK_FORMAT_BC3_UNORM_BLOCK, "BC3"},
            {VK_FORMAT_BC5_UNORM_BLOCK, "BC5"}, {VK_FORMAT_BC7_UNORM_BLOCK, "BC7"},
            {VK_FORMAT_ASTC_4x4_UNORM_BLOCK, "ASTC_4x4"}, {VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, "ETC2_RGBA8"},
            {VK_FORMAT_R8G8B8A8_UNORM, "RGBA8"}, {VK_FORMAT_B8G8R8A8_UNORM, "BGRA8"}
        };
        out << ",\"formats\":[";
        for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
            if (i) out << ',';
            VkFormatProperties f{};
            vkGetPhysicalDeviceFormatProperties(device, formats[i].format, &f);
            VkImageFormatProperties image{};
            auto r = vkGetPhysicalDeviceImageFormatProperties(device, formats[i].format, VK_IMAGE_TYPE_2D,
                VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, 0, &image);
            out << "{\"name\":" << quote(formats[i].name) << ",\"optimal_features\":" << f.optimalTilingFeatures
                << ",\"sampled_2d_supported\":" << (r == VK_SUCCESS ? "true" : "false") << '}';
        }
        out << "],\"memory_heaps\":[";
        VkPhysicalDeviceMemoryProperties2 mem{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
        VkPhysicalDeviceMemoryBudgetPropertiesEXT b{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
        auto getMemory2 = (PFN_vkGetPhysicalDeviceMemoryProperties2)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties2");
        bool canBudget = budget && getMemory2 && api >= VK_API_VERSION_1_1 && props.apiVersion >= VK_API_VERSION_1_1;
        if (canBudget) { mem.pNext = &b; getMemory2(device, &mem); }
        else vkGetPhysicalDeviceMemoryProperties(device, &mem.memoryProperties);
        for (uint32_t i = 0; i < mem.memoryProperties.memoryHeapCount; ++i) {
            if (i) out << ',';
            out << "{\"size_bytes\":" << mem.memoryProperties.memoryHeaps[i].size;
            if (canBudget) out << ",\"budget_bytes\":" << b.heapBudget[i] << ",\"usage_bytes\":" << b.heapUsage[i];
            else out << ",\"budget_bytes\":\"unavailable\"";
            out << '}';
        }
        out << "],\"swapchain_formats\":\"unavailable: requires an active game surface\",\"present_modes\":\"unavailable: requires an active game surface\"}";
    }
    out << "]}";
    vkDestroyInstance(instance, nullptr);
    return out.str();
}
extern "C" JNIEXPORT jstring JNICALL Java_com_winlator_cmod_dimensity_VulkanProbe_inspect(JNIEnv* env, jclass) {
    return env->NewStringUTF(inspect().c_str());
}
