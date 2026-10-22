// CPU ordering check of the product batch, with recording Vulkan/GL endpoints.
// This does not execute a GPU or establish rendered gameplay.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"
#define VORTEK_XWINDOW_SWAPCHAIN_H
#define VULKAN_HELPER_H
typedef struct { VkResult prepared, copied; VkCommandBuffer command; } XWindowSwapchain;
static VkQueue seenQueue;
static uint32_t submitCalls, idleCalls, copyCalls, seenWaits, seenCommands;
static VkCommandBuffer seenCommand[2];
static VkResult submitResult = VK_SUCCESS, idleResult = VK_SUCCESS;
static VkResult XWindowSwapchain_preparePresent(XWindowSwapchain* chain, uint32_t index, VkCommandBuffer* command) {
    (void)index; if (chain->prepared == VK_SUCCESS) *command = chain->command;
    return chain->prepared;
}
static VkResult XWindowSwapchain_completePresent(XWindowSwapchain* chain, uint32_t index) {
    (void)index; copyCalls++; return idleCalls ? chain->copied : VK_ERROR_DEVICE_LOST;
}
static VkResult recordSubmit(VkQueue queue, uint32_t count, const VkSubmitInfo* info, VkFence fence) {
    (void)count; (void)fence; submitCalls++; seenQueue=queue;
    seenWaits=info->waitSemaphoreCount; seenCommands=info->commandBufferCount;
    for(uint32_t i=0;i<seenCommands && i<2;i++)seenCommand[i]=info->pCommandBuffers[i];
    return submitResult;
}
static VkResult recordIdle(VkQueue queue) { idleCalls++; return queue==seenQueue ? idleResult : VK_ERROR_DEVICE_LOST; }
static struct { PFN_vkQueueSubmit vkQueueSubmit; PFN_vkQueueWaitIdle vkQueueWaitIdle; }
    vulkanWrapper = {recordSubmit, recordIdle};
#include "../../app/src/main/cpp/vortekrenderer/src/present_queue.c"
#define CHECK(x) do { if(!(x)) { printf("present_queue_failed_line=%d\n",__LINE__); return 1; } } while(0)
static void reset(void) { submitCalls=idleCalls=copyCalls=0; submitResult=idleResult=VK_SUCCESS; }
int main(void) {
    XWindowSwapchain a={VK_SUCCESS,VK_SUCCESS,(VkCommandBuffer)(uintptr_t)11};
    XWindowSwapchain b={VK_SUCCESS,VK_SUCCESS,(VkCommandBuffer)(uintptr_t)12};
    VkSwapchainKHR chains[]={(VkSwapchainKHR)&a,(VkSwapchainKHR)&b};
    uint32_t indices[]={0,1}; VkSemaphore wait=(VkSemaphore)(uintptr_t)21;
    VkPresentInfoKHR info={.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,.waitSemaphoreCount=1,.pWaitSemaphores=&wait,
        .swapchainCount=2,.pSwapchains=chains,.pImageIndices=indices};
    VkResult results[2]; VkQueue queue=(VkQueue)(uintptr_t)1234;
    CHECK(XWindowSwapchain_present(queue,&info,results)==VK_SUCCESS);
    CHECK(submitCalls==1 && idleCalls==1 && copyCalls==2 && seenQueue==queue && seenWaits==1 && seenCommands==2);
    CHECK(seenCommand[0]==a.command && seenCommand[1]==b.command);
    reset(); a.prepared=VK_ERROR_OUT_OF_DATE_KHR;
    CHECK(XWindowSwapchain_present(queue,&info,results)==VK_ERROR_OUT_OF_DATE_KHR);
    CHECK(submitCalls==1 && seenWaits==1 && seenCommands==1 && seenCommand[0]==b.command && copyCalls==1);
    reset(); b.prepared=VK_ERROR_SURFACE_LOST_KHR;
    CHECK(XWindowSwapchain_present(queue,&info,results)==VK_ERROR_SURFACE_LOST_KHR);
    CHECK(submitCalls==1 && seenWaits==1 && seenCommands==0 && copyCalls==0 && idleCalls==1);
    reset(); a.prepared=VK_SUCCESS; b.prepared=VK_ERROR_OUT_OF_HOST_MEMORY;
    CHECK(XWindowSwapchain_present(queue,&info,results)==VK_ERROR_OUT_OF_HOST_MEMORY && submitCalls==0 && copyCalls==0);
    reset(); b.prepared=VK_SUCCESS; submitResult=VK_ERROR_OUT_OF_DEVICE_MEMORY;
    CHECK(XWindowSwapchain_present(queue,&info,results)==VK_ERROR_OUT_OF_DEVICE_MEMORY && idleCalls==0 && copyCalls==0);
    reset(); idleResult=VK_ERROR_OUT_OF_HOST_MEMORY;
    CHECK(XWindowSwapchain_present(queue,&info,results)==VK_ERROR_DEVICE_LOST && copyCalls==0);
    puts("vortek_present_queue_result=0 (caller queue, one wait batch, per-window errors, completion order)"); return 0;
}
