#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "vulkan/vulkan.h"
static FILE* logFile;
static HWND window;
static VkInstance instance;
static VkPhysicalDevice gpu;
static VkDevice device;
static VkSurfaceKHR surface;
static VkSwapchainKHR chain;
static VkQueue queue;
static VkCommandPool pool;
static VkCommandBuffer command;
static VkSemaphore acquired, rendered;
static VkImage image, images[3];
static uint32_t imageCount;
static VkExtent2D extent;
static PFN_vkGetInstanceProcAddr getProc;
#define FUNCTIONS(F) F(vkCreateWin32SurfaceKHR) F(vkEnumeratePhysicalDevices) F(vkGetPhysicalDeviceQueueFamilyProperties) F(vkGetPhysicalDeviceSurfaceSupportKHR) F(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) F(vkGetPhysicalDeviceSurfaceFormatsKHR) F(vkGetPhysicalDeviceProperties) F(vkCreateDevice) F(vkGetDeviceQueue) F(vkCreateSwapchainKHR) F(vkGetSwapchainImagesKHR) F(vkDestroySwapchainKHR) F(vkCreateCommandPool) F(vkAllocateCommandBuffers) F(vkBeginCommandBuffer) F(vkEndCommandBuffer) F(vkResetCommandBuffer) F(vkCmdPipelineBarrier) F(vkCmdClearColorImage) F(vkCreateSemaphore) F(vkAcquireNextImageKHR) F(vkQueueSubmit) F(vkQueuePresentKHR) F(vkDeviceWaitIdle) F(vkDestroySemaphore) F(vkDestroyCommandPool) F(vkDestroyDevice) F(vkDestroySurfaceKHR) F(vkDestroyInstance)
#define DECLARE(name) static PFN_##name name;
FUNCTIONS(DECLARE)
#define CHECK(call) do { VkResult result=(call); fprintf(logFile,"%s=%d\n",#call,result); fflush(logFile); if(result!=VK_SUCCESS) return 1; } while(0)
static LRESULT CALLBACK events(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcA(w,m,a,b);}
static void pump(void){MSG m;while(PeekMessageA(&m,NULL,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}}
static int createChain(void){
 VkSurfaceCapabilitiesKHR caps; CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu,surface,&caps));
 uint32_t n=0;CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu,surface,&n,NULL));if(!n||n>16)return 1;
 VkSurfaceFormatKHR formats[16];CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu,surface,&n,formats));
 fprintf(logFile,"format=%u colorspace=%u extent=%ux%u images=%u..%u\n",formats[0].format,formats[0].colorSpace,caps.currentExtent.width,caps.currentExtent.height,caps.minImageCount,caps.maxImageCount);
 extent=caps.currentExtent;
 VkSwapchainCreateInfoKHR info={.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,.surface=surface,.minImageCount=caps.minImageCount,.imageFormat=formats[0].format,.imageColorSpace=formats[0].colorSpace,.imageExtent=extent,.imageArrayLayers=1,.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,.preTransform=caps.currentTransform,.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,.presentMode=VK_PRESENT_MODE_FIFO_KHR,.clipped=VK_TRUE};
 CHECK(vkCreateSwapchainKHR(device,&info,NULL,&chain));n=3;CHECK(vkGetSwapchainImagesKHR(device,chain,&n,images));if(n<1||n>3)return 1;imageCount=n;return 0;
}
int WINAPI WinMain(HINSTANCE app,HINSTANCE prev,LPSTR args,int show){
 logFile=fopen("C:/DGSmoke/vortek-present.txt","w");if(!logFile)return 2;
 HMODULE dll=LoadLibraryA("vulkan-1.dll");getProc=(PFN_vkGetInstanceProcAddr)GetProcAddress(dll,"vkGetInstanceProcAddr");if(!getProc){fprintf(logFile,"missing_loader=1 complete=1 result=1\n");return 1;}
 PFN_vkCreateInstance create=(PFN_vkCreateInstance)getProc(NULL,"vkCreateInstance");
 const char* extensions[]={"VK_KHR_surface","VK_KHR_win32_surface"};
 VkApplicationInfo appInfo={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="DG Vortek output",.apiVersion=VK_API_VERSION_1_1};
 VkInstanceCreateInfo instanceInfo={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&appInfo,.enabledExtensionCount=2,.ppEnabledExtensionNames=extensions};CHECK(create(&instanceInfo,NULL,&instance));
#define LOAD(name) name=(PFN_##name)getProc(instance,#name);if(!name){fprintf(logFile,"missing=%s\n",#name);return 1;}
 FUNCTIONS(LOAD)
 WNDCLASSA wc={0};wc.lpfnWndProc=events;wc.hInstance=app;wc.lpszClassName="DG-VortekOutput";RegisterClassA(&wc);
 window=CreateWindowA(wc.lpszClassName,"Dimensity Vortek GPU output",WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,940,610,NULL,NULL,app,NULL);pump();
 VkWin32SurfaceCreateInfoKHR winInfo={.sType=VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,.hinstance=app,.hwnd=window};CHECK(vkCreateWin32SurfaceKHR(instance,&winInfo,NULL,&surface));
 uint32_t n=1;CHECK(vkEnumeratePhysicalDevices(instance,&n,&gpu));VkPhysicalDeviceProperties props;vkGetPhysicalDeviceProperties(gpu,&props);fprintf(logFile,"gpu=%s driver=%u\n",props.deviceName,props.driverVersion);
 uint32_t count=0;vkGetPhysicalDeviceQueueFamilyProperties(gpu,&count,NULL);if(count>32)return 1;VkQueueFamilyProperties families[32];vkGetPhysicalDeviceQueueFamilyProperties(gpu,&count,families);uint32_t family=UINT32_MAX;
 for(uint32_t i=0;i<count;i++){VkBool32 supported=0;CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu,i,surface,&supported));if(supported&&(families[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)){family=i;break;}}if(family==UINT32_MAX)return 1;
 float priority=1;VkDeviceQueueCreateInfo queueInfo={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=family,.queueCount=1,.pQueuePriorities=&priority};const char* swap="VK_KHR_swapchain";
 VkDeviceCreateInfo deviceInfo={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=1,.pQueueCreateInfos=&queueInfo,.enabledExtensionCount=1,.ppEnabledExtensionNames=&swap};CHECK(vkCreateDevice(gpu,&deviceInfo,NULL,&device));vkGetDeviceQueue(device,family,0,&queue);
 if(createChain())return 1;
 VkCommandPoolCreateInfo poolInfo={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,.queueFamilyIndex=family};CHECK(vkCreateCommandPool(device,&poolInfo,NULL,&pool));
 VkCommandBufferAllocateInfo allocate={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};CHECK(vkAllocateCommandBuffers(device,&allocate,&command));
 VkSemaphoreCreateInfo semaphore={.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};CHECK(vkCreateSemaphore(device,&semaphore,NULL,&acquired));CHECK(vkCreateSemaphore(device,&semaphore,NULL,&rendered));
 unsigned presents=0,recreated=0;DWORD started=GetTickCount();
 for(unsigned frame=0;frame<65&&GetTickCount()-started<18000;frame++){
  pump();if(frame==18){SetWindowPos(window,NULL,0,0,820,540,SWP_NOMOVE|SWP_NOZORDER);pump();Sleep(250);}
  uint32_t index=UINT32_MAX;VkResult result=vkAcquireNextImageKHR(device,chain,UINT64_MAX,acquired,VK_NULL_HANDLE,&index);
  if(result==VK_ERROR_OUT_OF_DATE_KHR){fprintf(logFile,"resize_out_of_date=1\n");CHECK(vkDeviceWaitIdle(device));vkDestroySwapchainKHR(device,chain,NULL);if(createChain())return 1;recreated++;continue;}
  if(result!=VK_SUCCESS||index>=imageCount){fprintf(logFile,"acquire_error=%d index=%u\n",result,index);return 1;}
  image=images[index];
  CHECK(vkResetCommandBuffer(command,0));VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};CHECK(vkBeginCommandBuffer(command,&begin));
  VkImageMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.image=image,.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
  vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
  VkClearColorValue color={{recreated?0.10f:0.04f,recreated?0.60f:0.18f,recreated?0.24f:0.70f,1.0f}};vkCmdClearColorImage(command,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color,1,&barrier.subresourceRange);
  barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=0;barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,NULL,0,NULL,1,&barrier);CHECK(vkEndCommandBuffer(command));
  VkPipelineStageFlags stage=VK_PIPELINE_STAGE_TRANSFER_BIT;VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.waitSemaphoreCount=1,.pWaitSemaphores=&acquired,.pWaitDstStageMask=&stage,.commandBufferCount=1,.pCommandBuffers=&command,.signalSemaphoreCount=1,.pSignalSemaphores=&rendered};CHECK(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE));
  VkResult perChain=VK_NOT_READY;VkPresentInfoKHR present={.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,.waitSemaphoreCount=1,.pWaitSemaphores=&rendered,.swapchainCount=1,.pSwapchains=&chain,.pImageIndices=&index,.pResults=&perChain};result=vkQueuePresentKHR(queue,&present);
  if(result!=VK_SUCCESS||perChain!=VK_SUCCESS){fprintf(logFile,"present_error=%d per_chain=%d\n",result,perChain);return 1;}presents++;Sleep(120);
 }
 CHECK(vkDeviceWaitIdle(device));vkDestroySemaphore(device,acquired,NULL);vkDestroySemaphore(device,rendered,NULL);vkDestroyCommandPool(device,pool,NULL);vkDestroySwapchainKHR(device,chain,NULL);vkDestroyDevice(device,NULL);vkDestroySurfaceKHR(instance,surface,NULL);vkDestroyInstance(instance,NULL);DestroyWindow(window);
 fprintf(logFile,"presents=%u resized=%u complete=1 result=%d\n",presents,recreated,presents>1&&recreated==1?0:1);fclose(logFile);return presents>1&&recreated==1?0:1;
}
