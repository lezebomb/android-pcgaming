#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"
#include "dimensity_wsi.h"
#define CLIENT_RING_BUFFER_SIZE 262144
static unsigned queryCalls, acquireCalls;
static VkResult acquireResult = VK_TIMEOUT;
static uint64_t receivedTimeout;
static VkResult vt_call_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice d,VkSurfaceKHR s,VkSurfaceCapabilitiesKHR* out) {
    (void)d;(void)s; *out=(VkSurfaceCapabilitiesKHR){.minImageCount=2,.maxImageCount=3,.currentExtent={640,480}}; return VK_SUCCESS;
}
static VkResult vt_call_vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice d,VkSurfaceKHR s,uint32_t* count,VkSurfaceFormatKHR* out) {
    (void)d;(void)s;queryCalls++;
    if(!out){*count=3;return VK_SUCCESS;}
    uint32_t n=*count<3?*count:3;
    for(uint32_t i=0;i<n;i++)out[i]=(VkSurfaceFormatKHR){VK_FORMAT_R8G8B8A8_UNORM,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    *count=n;return n<3?VK_INCOMPLETE:VK_SUCCESS;
}
static VkResult vt_call_vkAcquireNextImageKHR(VkDevice d,VkSwapchainKHR s,uint64_t t,VkSemaphore sem,VkFence f,uint32_t* out) {
    (void)d;(void)s;(void)sem;(void)f;acquireCalls++;receivedTimeout=t;
    if(acquireResult==VK_SUCCESS)*out=2;return acquireResult;
}
#include "../vortek-client/surface-queries.inc"
#define CHECK(x) do{if(!(x)){printf("wsi_failed_line=%d\n",__LINE__);return 1;}}while(0)
int main(void) {
    VkPhysicalDeviceSurfaceInfo2KHR info={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR};
    VkSurfaceCapabilities2KHR caps={.sType=VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,.pNext=(void*)0x1234};
    CHECK(vt_call_vkGetPhysicalDeviceSurfaceCapabilities2KHR(VK_NULL_HANDLE,&info,&caps)==VK_SUCCESS);
    CHECK(caps.surfaceCapabilities.currentExtent.width==640 && caps.pNext==(void*)0x1234);
    VkSurfaceFormat2KHR formats[3];
    for(unsigned i=0;i<3;i++)formats[i]=(VkSurfaceFormat2KHR){.sType=VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR,.pNext=(void*)0x1234};
    uint32_t n=0;
    CHECK(vt_call_vkGetPhysicalDeviceSurfaceFormats2KHR(VK_NULL_HANDLE,&info,&n,formats)==VK_INCOMPLETE && n==0 && queryCalls==0);
    n=1;CHECK(vt_call_vkGetPhysicalDeviceSurfaceFormats2KHR(VK_NULL_HANDLE,&info,&n,formats)==VK_INCOMPLETE && n==1);
    n=UINT32_MAX;CHECK(vt_call_vkGetPhysicalDeviceSurfaceFormats2KHR(VK_NULL_HANDLE,&info,&n,formats)==VK_SUCCESS && n==3);
    CHECK(formats[2].surfaceFormat.format==VK_FORMAT_R8G8B8A8_UNORM && formats[2].pNext==(void*)0x1234);
    VkAcquireNextImageInfoKHR acquire={.sType=VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,.deviceMask=1,.timeout=999};
    uint32_t image=0xfeed;
    CHECK(vt_call_vkAcquireNextImage2KHR(VK_NULL_HANDLE,&acquire,&image)==VK_TIMEOUT && image==0xfeed && receivedTimeout==999);
    acquireResult=VK_SUCCESS;CHECK(vt_call_vkAcquireNextImage2KHR(VK_NULL_HANDLE,&acquire,&image)==VK_SUCCESS && image==2);
    acquire.deviceMask=2;CHECK(vt_call_vkAcquireNextImage2KHR(VK_NULL_HANDLE,&acquire,&image)==VK_ERROR_FEATURE_NOT_PRESENT && acquireCalls==2);
    CHECK(dgUnsupportedInstanceExtension("VK_EXT_debug_utils") && !dgUnsupportedInstanceExtension("VK_KHR_get_surface_capabilities2"));
    CHECK(dgUnsupportedDeviceExtension("VK_KHR_present_wait") && !dgUnsupportedDeviceExtension("VK_KHR_synchronization2"));
    puts("vortek_wsi_result=0 (surface2 capacity, header preservation, acquire2 timeout, extension scope)");return 0;
}
