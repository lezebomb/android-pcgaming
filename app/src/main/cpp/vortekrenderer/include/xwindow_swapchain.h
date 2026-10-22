#ifndef VORTEK_XWINDOW_SWAPCHAIN_H
#define VORTEK_XWINDOW_SWAPCHAIN_H

#include <android/hardware_buffer.h>

#include "vortek.h"

typedef struct XWindowSwapchain_Image {
    VkImage image;
    VkDeviceMemory memory;
} XWindowSwapchain_Image;

typedef struct XWindowSwapchain {
    int windowId;
    XWindowSwapchain_Image* images;
    int imageCount;
    VkFormat imageFormat;
    VkExtent2D imageExtent;
    VkImageUsageFlags imageUsage;
    VkQueue queue;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    uint32_t queueFamily;
    VkCommandPool transferPool;
    VkCommandBuffer acquireCommand;
    VkCommandBuffer releaseCommand;
    bool everAcquired;
    JMethods* jmethods;
} XWindowSwapchain;

extern void getWindowExtent(JMethods* jmethods, int windowId, VkExtent2D* extent);
extern int getSurfaceMinImageCount();
extern VkResult getSurfaceFormats(VkPhysicalDevice physicalDevice, uint32_t* formatCount, VkSurfaceFormatKHR* formats);

extern VkResult XWindowSwapchain_create(VkPhysicalDevice physicalDevice, VkDevice device, uint32_t graphicsQueueIndex, VkSwapchainCreateInfoKHR* swapchainInfo, JMethods* jmethods, int windowId, XWindowSwapchain** output);
extern void XWindowSwapchain_destroy(VkDevice device, XWindowSwapchain* swapchain);
extern VkResult XWindowSwapchain_acquireNextImage(XWindowSwapchain* swapchain, uint64_t timeout, VkSemaphore signalSemaphore, VkFence fence, uint32_t* imageIndex);
extern VkResult XWindowSwapchain_presentImage(XWindowSwapchain* swapchain, uint32_t waitCount, const VkSemaphore* waits);

#endif
