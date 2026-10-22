// Real allocation lifetimes under a tiny explicit budget; no surface or performance claim.
#define VK_NO_PROTOTYPES
#include <windows.h>
#include <vulkan/vulkan_core.h>
#include <stdio.h>
static FILE *logfile;
static int fail(const char *step, VkResult result) {
    fprintf(logfile, "failed=%s result=%d\n", step, result); fflush(logfile); return 1;
}
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR arguments, int show) {
    (void)instance; (void)previous; (void)arguments; (void)show;
    logfile = fopen("C:/DGSmoke/vulkan-sparse-budget.txt", "w");
    if (!logfile) return 2;
    int result = 1;
    HMODULE library = LoadLibraryA("vulkan-1.dll");
    if (!library) library = LoadLibraryA("winevulkan.dll");
    if (!library) { fail("loader", VK_ERROR_INITIALIZATION_FAILED); goto finish; }
    PFN_vkGetInstanceProcAddr get = (PFN_vkGetInstanceProcAddr)GetProcAddress(library, "vkGetInstanceProcAddr");
    if (!get) { fail("get_proc", VK_ERROR_INITIALIZATION_FAILED); goto finish; }
    PFN_vkCreateInstance create_instance = (PFN_vkCreateInstance)get(NULL,"vkCreateInstance");
    VkApplicationInfo app = {.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="Dimensity sparse allocation",.apiVersion=VK_API_VERSION_1_1};
    VkInstanceCreateInfo ci = {.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&app};
    VkInstance vk = VK_NULL_HANDLE;
    VkResult status = create_instance(&ci,NULL,&vk);
    if (status) { fail("instance",status); goto finish; }
#define INSTANCE_PROC(type, name) PFN_##type name = (PFN_##type)get(vk,#type)
    INSTANCE_PROC(vkDestroyInstance, destroy_instance);
    INSTANCE_PROC(vkEnumeratePhysicalDevices, enumerate);
    INSTANCE_PROC(vkGetPhysicalDeviceProperties, properties);
    INSTANCE_PROC(vkGetPhysicalDeviceFeatures, features);
    INSTANCE_PROC(vkGetPhysicalDeviceQueueFamilyProperties, families);
    INSTANCE_PROC(vkCreateDevice, create_device);
    INSTANCE_PROC(vkGetDeviceProcAddr, device_get);
    uint32_t count=4;
    VkPhysicalDevice physical[4];
    status=enumerate(vk,&count,physical);
    if (status || !count) { fail("physical",status); goto instance_done; }
    VkPhysicalDeviceProperties props; properties(physical[0],&props);
    VkPhysicalDeviceFeatures supported; features(physical[0],&supported);
    fprintf(logfile,"device=%s sparse_binding=%u\n",props.deviceName,supported.sparseBinding);
    if (!supported.sparseBinding) { fail("layer_not_enabled",VK_ERROR_FEATURE_NOT_PRESENT); goto instance_done; }
    uint32_t n=0; families(physical[0],&n,NULL);
    if (n>32) { fail("queue_count",VK_ERROR_INITIALIZATION_FAILED); goto instance_done; }
    VkQueueFamilyProperties q[32]; families(physical[0],&n,q);
    uint32_t family=0; while (family<n && !(q[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) family++;
    if (family==n) { fail("queue",VK_ERROR_FEATURE_NOT_PRESENT); goto instance_done; }
    float priority=1;
    VkDeviceQueueCreateInfo qi={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=family,.queueCount=1,.pQueuePriorities=&priority};
    VkPhysicalDeviceFeatures enabled={.sparseBinding=VK_TRUE};
    VkDeviceCreateInfo dc={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=1,.pQueueCreateInfos=&qi,.pEnabledFeatures=&enabled};
    VkDevice device;
    status=create_device(physical[0],&dc,NULL,&device);
    if (status) { fail("device",status); goto instance_done; }
#define DEVICE_PROC(type, name) PFN_##type name=(PFN_##type)device_get(device,#type)
    DEVICE_PROC(vkCreateBuffer, create_buffer);
    DEVICE_PROC(vkGetBufferMemoryRequirements, requirements);
    DEVICE_PROC(vkDestroyBuffer, destroy_buffer);
    DEVICE_PROC(vkDeviceWaitIdle, idle);
    DEVICE_PROC(vkDestroyDevice, destroy_device);
    const VkDeviceSize budget=4096, requested=4;
    VkBufferCreateInfo info={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=requested,
        .usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,.sharingMode=VK_SHARING_MODE_EXCLUSIVE};
    VkBuffer buffer;
    status=create_buffer(device,&info,NULL,&buffer);
    if (status) { fail("normal_buffer",status); goto device_done; }
    VkMemoryRequirements req; requirements(device,buffer,&req); destroy_buffer(device,buffer,NULL);
    fprintf(logfile,"requested=%llu allocation=%llu alignment=%llu budget=%llu\n",
        (unsigned long long)requested,(unsigned long long)req.size,(unsigned long long)req.alignment,(unsigned long long)budget);
    if (req.size>budget || req.size<requested) { fail("budget_test_geometry",VK_ERROR_INITIALIZATION_FAILED); goto device_done; }
    int padding = req.size>requested;
    uint32_t cycles=padding ? (uint32_t)(budget/(req.size-requested))+3 : 64;
    if (cycles>2048) { fail("cycle_bound",VK_ERROR_INITIALIZATION_FAILED); goto device_done; }
    info.flags=VK_BUFFER_CREATE_SPARSE_BINDING_BIT;
    for (uint32_t i=0;i<cycles;i++) {
        status=create_buffer(device,&info,NULL,&buffer);
        if (status) { fprintf(logfile,"cycle=%u\n",i); fail("sparse_recreate",status); goto device_done; }
        idle(device); destroy_buffer(device,buffer,NULL);
    }
    fprintf(logfile,"completed_cycles=%u padding_regression_case_available=%d\n",cycles,padding);
    // Fill the budget with live allocations, then release all and verify recovery.
    static VkBuffer held[1025]; uint32_t live=0;
    for (;live<1025;live++) {
        status=create_buffer(device,&info,NULL,&held[live]);
        if (status!=VK_SUCCESS) break;
    }
    idle(device);
    for (uint32_t i=0;i<live;i++) destroy_buffer(device,held[i],NULL);
    fprintf(logfile,"live_before_budget_rejection=%u budget_error=%d\n",live,status);
    if (!live || status!=VK_ERROR_OUT_OF_DEVICE_MEMORY) { fail("live_budget_enforcement",status); goto device_done; }
    info.size=budget*2;
    status=create_buffer(device,&info,NULL,&buffer);
    if (status==VK_SUCCESS) { destroy_buffer(device,buffer,NULL); fail("budget_not_enforced",status); goto device_done; }
    if (status!=VK_ERROR_OUT_OF_DEVICE_MEMORY) { fail("unexpected_budget_error",status); goto device_done; }
    fprintf(logfile,"oversized_rejected=%d\n",status);
    info.size=requested;
    status=create_buffer(device,&info,NULL,&buffer);
    if (status) { fail("after_rejected_allocation",status); goto device_done; }
    idle(device); destroy_buffer(device,buffer,NULL);
    result=0;
device_done:
    idle(device); destroy_device(device,NULL);
instance_done:
    destroy_instance(vk,NULL);
finish:
    fprintf(logfile,"complete=1 result=%d\n",result); fclose(logfile);
    return result;
}
