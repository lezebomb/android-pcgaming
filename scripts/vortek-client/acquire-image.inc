VkResult vt_call_vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain,
        uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* imageIndex) {
    if (!imageIndex) return VK_ERROR_INITIALIZATION_FAILED;
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* swapchainObject = VkObject_fromHandle(swapchain);
    VkObject* semaphoreObject = VkObject_fromHandle(semaphore);
    VkObject* fenceObject = VkObject_fromHandle(fence);
    VT_SERIALIZE_CMD(vkAcquireNextImageKHR, (VkDevice)&deviceObject->id,
            (VkSwapchainKHR)&swapchainObject->id, timeout, (VkSemaphore)&semaphoreObject->id,
            (VkFence)&fenceObject->id, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_ACQUIRE_NEXT_IMAGE_KHR, VT_RETURN);
    char* input = NULL;
    int size = 0;
    int result = vt_recv(clientRing, &input, &size, &globalMemoryPool);
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
        if (size != sizeof(*imageIndex)) result = VK_ERROR_DEVICE_LOST;
        else memcpy(imageIndex, input, sizeof(*imageIndex));
    }
    VT_CALL_UNLOCK();
    return (VkResult)result;
}
