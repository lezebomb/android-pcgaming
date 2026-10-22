// SPDX-License-Identifier: LGPL-2.1-only
#include "xwindow_swapchain.h"
#include "vulkan_helper.h"

static VkResult mergePresentResult(VkResult accumulated, VkResult next) {
    if (accumulated == VK_ERROR_DEVICE_LOST || next == VK_ERROR_DEVICE_LOST) return VK_ERROR_DEVICE_LOST;
    if (accumulated == VK_ERROR_SURFACE_LOST_KHR || next == VK_ERROR_SURFACE_LOST_KHR) return VK_ERROR_SURFACE_LOST_KHR;
    if (accumulated == VK_ERROR_OUT_OF_DATE_KHR || next == VK_ERROR_OUT_OF_DATE_KHR) return VK_ERROR_OUT_OF_DATE_KHR;
    if (accumulated < 0) return accumulated;
    if (next < 0) return next;
    return accumulated == VK_SUBOPTIMAL_KHR || next == VK_SUBOPTIMAL_KHR ? VK_SUBOPTIMAL_KHR : VK_SUCCESS;
}

static VkResult failPresent(const VkPresentInfoKHR* info, VkResult* results, VkResult result) {
    for (uint32_t i = 0; i < info->swapchainCount; i++) results[i] = result;
    return result;
}

VkResult XWindowSwapchain_present(VkQueue queue, const VkPresentInfoKHR* info, VkResult* results) {
    VkCommandBuffer* commands = info->swapchainCount ? calloc(info->swapchainCount, sizeof(*commands)) : NULL;
    VkPipelineStageFlags* stages = info->waitSemaphoreCount ? calloc(info->waitSemaphoreCount, sizeof(*stages)) : NULL;
    if ((info->swapchainCount && !commands) || (info->waitSemaphoreCount && !stages)) {
        free(commands);
        free(stages);
        return failPresent(info, results, VK_ERROR_OUT_OF_HOST_MEMORY);
    }
    uint32_t commandCount = 0;
    VkResult combined = VK_SUCCESS;
    for (uint32_t i = 0; i < info->swapchainCount; i++) {
        VkResult result = XWindowSwapchain_preparePresent((XWindowSwapchain*)info->pSwapchains[i],
                info->pImageIndices[i], &commands[commandCount]);
        results[i] = result;
        // No queue work has been submitted. Allocation errors can still leave
        // all acquired images and semaphore payloads unaffected.
        if (result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY || result == VK_ERROR_DEVICE_LOST) {
            free(commands);
            free(stages);
            return failPresent(info, results, result);
        }
        if (result == VK_SUCCESS) commandCount++;
        combined = mergePresentResult(combined, result);
    }
    for (uint32_t i = 0; i < info->waitSemaphoreCount; i++) stages[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = info->waitSemaphoreCount;
    submit.pWaitSemaphores = info->pWaitSemaphores;
    submit.pWaitDstStageMask = stages;
    submit.commandBufferCount = commandCount;
    submit.pCommandBuffers = commands;
    // Consume waits exactly once on the caller's queue, even when every
    // surface was rejected. Complete all FOREIGN releases before GL reads.
    VkResult submitted = vulkanWrapper.vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    free(commands);
    free(stages);
    if (submitted != VK_SUCCESS) return failPresent(info, results, submitted);
    VkResult completed = vulkanWrapper.vkQueueWaitIdle(queue);
    // Once submitted, an allocation error cannot promise unchanged resource
    // state. End the device session rather than permit unsafe producer reuse.
    if (completed != VK_SUCCESS) return failPresent(info, results, VK_ERROR_DEVICE_LOST);
    for (uint32_t i = 0; i < info->swapchainCount; i++) {
        if (results[i] == VK_SUCCESS) {
            results[i] = combined == VK_ERROR_DEVICE_LOST ? VK_ERROR_DEVICE_LOST :
                    XWindowSwapchain_completePresent((XWindowSwapchain*)info->pSwapchains[i], info->pImageIndices[i]);
            if (results[i] == VK_ERROR_OUT_OF_HOST_MEMORY || results[i] == VK_ERROR_OUT_OF_DEVICE_MEMORY)
                results[i] = VK_ERROR_DEVICE_LOST;
            combined = mergePresentResult(combined, results[i]);
        }
    }
    return combined;
}
