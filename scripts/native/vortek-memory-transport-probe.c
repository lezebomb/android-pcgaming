// Query real app-UID transport compatibility, not a game/rendering benchmark.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <android/hardware_buffer.h>
#include "native_handle.h"
extern const native_handle_t* AHardwareBuffer_getNativeHandle(const AHardwareBuffer* buffer);
#define VK_USE_PLATFORM_ANDROID_KHR
#include "vulkan/vulkan.h"
int main(int argc, char** argv) {
    (void)argv;
    VkApplicationInfo app={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.apiVersion=VK_API_VERSION_1_1};
    VkInstanceCreateInfo instanceInfo={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&app};
    VkInstance instance=VK_NULL_HANDLE; VkResult result=vkCreateInstance(&instanceInfo,NULL,&instance);
    if(result!=VK_SUCCESS){printf("create_instance=%d\n",result);return 1;}
    uint32_t count=1;VkPhysicalDevice physical;result=vkEnumeratePhysicalDevices(instance,&count,&physical);
    if(result!=VK_SUCCESS||!count){vkDestroyInstance(instance,NULL);return 2;}
    VkPhysicalDeviceMemoryProperties memory;vkGetPhysicalDeviceMemoryProperties(physical,&memory);
    for(uint32_t i=0;i<memory.memoryTypeCount;i++)printf("type=%u flags=0x%x heap=%u\n",i,memory.memoryTypes[i].propertyFlags,memory.memoryTypes[i].heapIndex);
    for(unsigned i=0;i<2;i++) { VkPhysicalDeviceExternalBufferInfo bi={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO,
        .usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,.handleType=i?VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT:VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT};
        VkExternalBufferProperties bp={.sType=VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES};vkGetPhysicalDeviceExternalBufferProperties(physical,&bi,&bp);
        printf("external_buffer_handle=0x%x features=0x%x compatible=0x%x\n",bi.handleType,bp.externalMemoryProperties.externalMemoryFeatures,bp.externalMemoryProperties.compatibleHandleTypes); }
    uint32_t extCount=0;vkEnumerateDeviceExtensionProperties(physical,NULL,&extCount,NULL);
    VkExtensionProperties* exts=calloc(extCount,sizeof(*exts));vkEnumerateDeviceExtensionProperties(physical,NULL,&extCount,exts);
    const char* enabled[3];uint32_t enabledCount=0;
    for(uint32_t i=0;i<extCount;i++)if(!strcmp(exts[i].extensionName,"VK_ANDROID_external_memory_android_hardware_buffer")||!strcmp(exts[i].extensionName,"VK_KHR_external_memory_fd")||!strcmp(exts[i].extensionName,"VK_EXT_external_memory_dma_buf"))enabled[enabledCount++]=exts[i].extensionName;
    float priority=1;VkDeviceQueueCreateInfo q={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=0,.queueCount=1,.pQueuePriorities=&priority};
    VkDeviceCreateInfo create={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=1,.pQueueCreateInfos=&q,.enabledExtensionCount=enabledCount,.ppEnabledExtensionNames=enabled};
    VkDevice device=VK_NULL_HANDLE;result=vkCreateDevice(physical,&create,NULL,&device);free(exts);
    if(result!=VK_SUCCESS){printf("create_device=%d\n",result);vkDestroyInstance(instance,NULL);return 3;}
    PFN_vkGetAndroidHardwareBufferPropertiesANDROID getAhb=(PFN_vkGetAndroidHardwareBufferPropertiesANDROID)vkGetDeviceProcAddr(device,"vkGetAndroidHardwareBufferPropertiesANDROID");
    PFN_vkGetMemoryFdPropertiesKHR getFd=(PFN_vkGetMemoryFdPropertiesKHR)vkGetDeviceProcAddr(device,"vkGetMemoryFdPropertiesKHR");
    AHardwareBuffer_Desc desc={.width=4096,.height=1,.layers=1,.format=AHARDWAREBUFFER_FORMAT_BLOB,
        .usage=AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER|AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN|AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN};
    if(argc>1) desc.usage=AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER|AHARDWAREBUFFER_USAGE_CPU_READ_RARELY|AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;
    printf("ahb_usage=0x%llx\n",(unsigned long long)desc.usage);
    AHardwareBuffer* ahb=NULL;int allocated=AHardwareBuffer_allocate(&desc,&ahb);printf("ahb_allocate=%d\n",allocated);
    if(ahb&&getAhb){
        VkAndroidHardwareBufferPropertiesANDROID props={.sType=VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID};
        result=getAhb(device,ahb,&props);printf("ahb_properties=%d bytes=%llu typeBits=0x%x\n",result,(unsigned long long)props.allocationSize,props.memoryTypeBits);
        if(result==VK_SUCCESS)for(uint32_t i=0;i<memory.memoryTypeCount;i++)if(props.memoryTypeBits&(1u<<i)){
            VkImportAndroidHardwareBufferInfoANDROID import={.sType=VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,.buffer=ahb};
            VkMemoryAllocateInfo ai={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.pNext=&import,.allocationSize=props.allocationSize,.memoryTypeIndex=i};
            VkDeviceMemory allocation=VK_NULL_HANDLE;result=vkAllocateMemory(device,&ai,NULL,&allocation);printf("ahb_import_type=%u result=%d\n",i,result);
            if(result==VK_SUCCESS){void* mapped=NULL;result=vkMapMemory(device,allocation,0,VK_WHOLE_SIZE,0,&mapped);printf("native_map_type=%u result=%d\n",i,result);if(result==VK_SUCCESS)vkUnmapMemory(device,allocation);vkFreeMemory(device,allocation,NULL);}
        }
    }
    printf("fd_properties_entry=%d\n",getFd!=NULL);
    if(ahb&&getFd){const native_handle_t* h=AHardwareBuffer_getNativeHandle(ahb);
        if(h&&h->numFds>0){VkMemoryFdPropertiesKHR fp={.sType=VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR};
        result=getFd(device,VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,h->data[0],&fp);printf("ahb_as_dma_properties=%d typeBits=0x%x\n",result,fp.memoryTypeBits);}}

    if(ahb)AHardwareBuffer_release(ahb);vkDestroyDevice(device,NULL);vkDestroyInstance(instance,NULL);return 0;
}
