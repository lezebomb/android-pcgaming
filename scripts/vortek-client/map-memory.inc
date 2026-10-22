// Included in the prepared guest client, after VT_CALL_LOCK/VT_CALL_UNLOCK.
#include "dimensity_mapping.h"
#include <errno.h>

static unsigned dgMapFailureLogs;

static VkResult dgUnmapMemory(MappedMemory* mapped, bool reserve) {
    if (!mapped || !mapped->mappingBase) return VK_SUCCESS;
    if (reserve) {
        if (mmap(mapped->mappingBase, mapped->mappedLength, PROT_NONE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED)
            return VK_ERROR_MEMORY_MAP_FAILED;
    }
    else if (munmap(mapped->mappingBase, mapped->mappedLength) != 0)
        return VK_ERROR_MEMORY_MAP_FAILED;
    mapped->mappingBase = mapped->data = NULL;
    mapped->mappedLength = 0;
    return VK_SUCCESS;
}

// Caller holds the transport lock. Preserve nanosecond/fence changes elsewhere;
// this request/reply remains the original shared-fd mapping protocol.
static VkResult dgMapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
        VkMemoryMapFlags flags, void* placedAddress, void** data) {
    if (!data) return VK_ERROR_MEMORY_MAP_FAILED;
    *data = NULL;
    VkObject* object = VkObject_fromHandle(memory);
    MappedMemory* mapped = object->tag;
    if (!mapped || mapped->mappingBase || (flags & ~VK_MEMORY_MAP_PLACED_BIT_EXT))
        return VK_ERROR_MEMORY_MAP_FAILED;
    bool placed = (flags & VK_MEMORY_MAP_PLACED_BIT_EXT) != 0;
    if (placed != (placedAddress != NULL)) return VK_ERROR_MEMORY_MAP_FAILED;
    long pageSize = sysconf(_SC_PAGESIZE);
    DGMapRange range;
    if (pageSize <= 0 || !dgMapRange(mapped->allocationSize, offset, size, pageSize, &range))
        return VK_ERROR_MEMORY_MAP_FAILED;
    // Placed mappings must cover the caller's reserved pages exactly.
    if (placed && (range.delta || (uintptr_t)placedAddress % (uintptr_t)pageSize))
        return VK_ERROR_MEMORY_MAP_FAILED;
    VT_SERIALIZE_CMD(VkDeviceMemory, (VkDeviceMemory)&object->id);
    if (vt_send(serverRing, REQUEST_CODE_VK_MAP_MEMORY, outputBuffer, bufferSize) != bufferSize)
        return VK_ERROR_DEVICE_LOST;
    int fds[MAX_FDS], count = 0, result = VK_ERROR_MEMORY_MAP_FAILED;
    int received = recv_fds(serverFd, fds, &count, &result, sizeof(result));
    if (received != sizeof(result) || count != 1 || result != VK_SUCCESS) {
        if (dgMapFailureLogs++ < 12)
            __android_log_print(ANDROID_LOG_WARN, "DimensityMemory", "Guest map reply failed: bytes=%d fds=%d result=%d", received, count, result);
        for (int i = 0; i < count; i++) close(fds[i]);
        return result < 0 ? result : VK_ERROR_MEMORY_MAP_FAILED;
    }
    void* base = mmap(placedAddress, range.length, PROT_READ | PROT_WRITE,
            MAP_SHARED | (placed ? MAP_FIXED : 0), fds[0], (off_t)range.fileOffset);
    int mapError = errno;
    close(fds[0]); // Close on mmap failure as well as success.
    if (base == MAP_FAILED) {
        if (dgMapFailureLogs++ < 12)
            __android_log_print(ANDROID_LOG_WARN, "DimensityMemory", "Guest mmap failed: errno=%d offset=%llu length=%zu placed=%d",
                    mapError, (unsigned long long)range.fileOffset, range.length, placed);
        return VK_ERROR_MEMORY_MAP_FAILED;
    }
    mapped->mappingBase = base;
    mapped->mappedLength = range.length;
    mapped->data = (char*)base + range.delta;
    *data = mapped->data; // Placed maps also return their pointer.
    return VK_SUCCESS;
}
