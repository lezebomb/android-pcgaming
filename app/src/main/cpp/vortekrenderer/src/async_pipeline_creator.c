#include "async_pipeline_creator.h"
#include "vortek_serializer.h"
#include "vulkan_helper.h"
#include <errno.h>

typedef struct PipelineCreateRequest {
    VkDevice device;
    VkPipelineCache pipelineCache;
    PipelineType type;
    uint32_t pipelineCount;
    void* pipelineInfos;
    ShaderInspector* shaderInspector;
    MemoryPool memoryPool;
    int notifyFd;
} PipelineCreateRequest;

static bool writeReply(int fd, const void* data, size_t size) {
    const char* cursor = data;
    while (size) {
        ssize_t sent = send(fd, cursor, size, MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) return false;
        cursor += sent; size -= sent;
    }
    return true;
}

static void pipelineCreateThread(void* param) {
    PipelineCreateRequest* pipelineCreateRequest = param;
    VkPipeline* pipelines = calloc(pipelineCreateRequest->pipelineCount, sizeof(*pipelines));
    ArrayList temporaryModules = {0};
    VkResult result = pipelines ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;

    if (result == VK_SUCCESS && pipelineCreateRequest->type == PIPELINE_TYPE_GRAPHICS) {
        VkGraphicsPipelineCreateInfo* createInfos = pipelineCreateRequest->pipelineInfos;

        for (int i = 0; i < pipelineCreateRequest->pipelineCount; i++) {
            result = ShaderInspector_inspectShaderStages(pipelineCreateRequest->shaderInspector, pipelineCreateRequest->device, (VkPipelineShaderStageCreateInfo*)createInfos[i].pStages, createInfos[i].stageCount, createInfos[i].pVertexInputState, &temporaryModules);
            if (result != VK_SUCCESS) break;
        }

        if (result == VK_SUCCESS) result = vulkanWrapper.vkCreateGraphicsPipelines(pipelineCreateRequest->device, pipelineCreateRequest->pipelineCache, pipelineCreateRequest->pipelineCount, createInfos, NULL, pipelines);
    }
    else if (result == VK_SUCCESS && pipelineCreateRequest->type == PIPELINE_TYPE_COMPUTE) {
        VkComputePipelineCreateInfo* createInfos = pipelineCreateRequest->pipelineInfos;

        for (int i = 0; i < pipelineCreateRequest->pipelineCount; i++) {
            result = ShaderInspector_inspectShaderStages(pipelineCreateRequest->shaderInspector, pipelineCreateRequest->device, (VkPipelineShaderStageCreateInfo*)&createInfos[i].stage, 1, NULL, &temporaryModules);
            if (result != VK_SUCCESS) break;
        }

        if (result == VK_SUCCESS) result = vulkanWrapper.vkCreateComputePipelines(pipelineCreateRequest->device, pipelineCreateRequest->pipelineCache, pipelineCreateRequest->pipelineCount, createInfos, NULL, pipelines);
    }

    for (int i = 0; i < temporaryModules.size; i++)
        vulkanWrapper.vkDestroyShaderModule(pipelineCreateRequest->device, (VkShaderModule)temporaryModules.elements[i], NULL);
    ArrayList_free(&temporaryModules, false);
    vt_free(&pipelineCreateRequest->memoryPool);

    bool delivered = writeReply(pipelineCreateRequest->notifyFd, &result, sizeof(result));
    // Small chunks bound stack use even for large pipeline batches.
    uint64_t chunk[64];
    for (uint32_t i = 0; delivered && i < pipelineCreateRequest->pipelineCount;) {
        uint32_t n = MIN(ARRAY_SIZE(chunk), pipelineCreateRequest->pipelineCount - i);
        for (uint32_t j = 0; j < n; j++) chunk[j] = pipelines ? (uint64_t)pipelines[i+j] : 0;
        delivered = writeReply(pipelineCreateRequest->notifyFd, chunk, n * sizeof(*chunk));
        i += n;
    }
    if (!delivered && pipelines) {
        for (uint32_t i = 0; i < pipelineCreateRequest->pipelineCount; i++)
            if (pipelines[i]) vulkanWrapper.vkDestroyPipeline(pipelineCreateRequest->device, pipelines[i], NULL);
    }
    free(pipelines);
    close(pipelineCreateRequest->notifyFd);
    MEMFREE(pipelineCreateRequest->pipelineInfos);
    MEMFREE(pipelineCreateRequest);
}

void AsyncPipelineCreator_create(VkContext* context, PipelineType type) {
    PipelineCreateRequest* pipelineCreateRequest = calloc(1, sizeof(PipelineCreateRequest));
    if (!pipelineCreateRequest) {
        char failure = 0;
        send(context->clientFd, &failure, 1, MSG_NOSIGNAL);
        return;
    }
    pipelineCreateRequest->type = type;
    pipelineCreateRequest->shaderInspector = context->shaderInspector;

    uint64_t deviceId = 0;
    uint64_t pipelineCacheId = 0;

    if (type == PIPELINE_TYPE_GRAPHICS) {
        vt_unserialize_vkCreateGraphicsPipelines((VkDevice)&deviceId, (VkPipelineCache)&pipelineCacheId, &pipelineCreateRequest->pipelineCount, NULL, NULL, NULL, context->inputBuffer, NULL);

        VkGraphicsPipelineCreateInfo* createInfos = calloc(pipelineCreateRequest->pipelineCount, sizeof(VkGraphicsPipelineCreateInfo));
        if (!createInfos) goto failed;
        vt_unserialize_vkCreateGraphicsPipelines(VK_NULL_HANDLE, VK_NULL_HANDLE, NULL, createInfos, NULL, NULL, context->inputBuffer, &pipelineCreateRequest->memoryPool);
        pipelineCreateRequest->pipelineInfos = createInfos;
    }
    else if (type == PIPELINE_TYPE_COMPUTE) {
        vt_unserialize_vkCreateComputePipelines((VkDevice)&deviceId, (VkPipelineCache)&pipelineCacheId, &pipelineCreateRequest->pipelineCount, NULL, NULL, NULL, context->inputBuffer, NULL);

        VkComputePipelineCreateInfo* createInfos = calloc(pipelineCreateRequest->pipelineCount, sizeof(VkComputePipelineCreateInfo));
        if (!createInfos) goto failed;
        vt_unserialize_vkCreateComputePipelines(VK_NULL_HANDLE, VK_NULL_HANDLE, NULL, createInfos, NULL, NULL, context->inputBuffer, &pipelineCreateRequest->memoryPool);
        pipelineCreateRequest->pipelineInfos = createInfos;
    }

    pipelineCreateRequest->device = VkObject_fromId(deviceId);
    pipelineCreateRequest->pipelineCache = VkObject_fromId(pipelineCacheId);

    int fds[2] = {-1, -1};
    char success = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds) == 0 ? 1 : 0;
    int sent = success ? send_fds(context->clientFd, fds, 1, &success, 1) : send(context->clientFd, &success, 1, MSG_NOSIGNAL);
    if (sent != 1 && success) {
        close(fds[0]); close(fds[1]); success = 0;
    }

    if (success) {
        pipelineCreateRequest->notifyFd = fds[1];
        close(fds[0]);
        ThreadPool_run(context->threadPool, pipelineCreateThread, pipelineCreateRequest);
    }
    else {
        vt_free(&pipelineCreateRequest->memoryPool);
        MEMFREE(pipelineCreateRequest->pipelineInfos);
        MEMFREE(pipelineCreateRequest);
    }
    return;
failed: {
        char failure = 0;
        send(context->clientFd, &failure, 1, MSG_NOSIGNAL);
        vt_free(&pipelineCreateRequest->memoryPool);
        free(pipelineCreateRequest->pipelineInfos);
        free(pipelineCreateRequest);
    }
}