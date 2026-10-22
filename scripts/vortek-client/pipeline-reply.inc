// Caller holds the transport lock only until receiving the dedicated reply fd.
static VkResult waitForPipelineCreation(int pipelineCount, VkPipeline* pPipelines) {
    if (pipelineCount > 0 && pPipelines)
        memset(pPipelines, 0, (size_t)pipelineCount * sizeof(*pPipelines));
    int fds[MAX_FDS], count = 0;
    char success = 0;
    int received = recv_fds(serverFd, fds, &count, &success, 1);
    VT_CALL_UNLOCK();
    if (received != 1 || !success || count != 1 || pipelineCount <= 0 || !pPipelines) {
        for (int i = 0; i < count; i++) close(fds[i]);
        return received == 1 && !success ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_ERROR_DEVICE_LOST;
    }
    int fd = fds[0];
    VkResult result = VK_ERROR_DEVICE_LOST;
    // Stream reads may be short, even when the writer supplied one buffer.
    unsigned char* cursor = (unsigned char*)&result;
    size_t remaining = sizeof(result);
    while (remaining) {
        ssize_t bytes = read(fd, cursor, remaining);
        if (bytes < 0 && errno == EINTR) continue;
        if (bytes <= 0) { close(fd); return VK_ERROR_DEVICE_LOST; }
        cursor += bytes; remaining -= bytes;
    }
    for (int i = 0; i < pipelineCount; i++) {
        uint64_t id = 0;
        cursor = (unsigned char*)&id; remaining = sizeof(id);
        while (remaining) {
            ssize_t bytes = read(fd, cursor, remaining);
            if (bytes < 0 && errno == EINTR) continue;
            if (bytes <= 0) { close(fd); return VK_ERROR_DEVICE_LOST; }
            cursor += bytes; remaining -= bytes;
        }
        if (id) {
            VkObject* object = VkObject_create(VK_OBJECT_TYPE_PIPELINE, id);
            pPipelines[i] = VkObject_toHandle(object);
        } // A failed/compile-required pipeline remains VK_NULL_HANDLE.
    }
    close(fd);
    return result;
}
