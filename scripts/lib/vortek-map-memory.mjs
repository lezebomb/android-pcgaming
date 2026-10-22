import fs from 'node:fs';
// Only replaces pinned upstream bodies, so source drift cannot silently match.
export function patchMapMemory(calls) {
  function body(name,next,replacement) {
    const start=calls.indexOf(name),end=calls.indexOf('\n'+next,start);
    if(start<0||end<start)throw Error('Unexpected mapping function '+name);
    calls=calls.slice(0,start)+replacement+'\n'+calls.slice(end);
  }
  body('VkResult vt_call_vkMapMemory(','void vt_call_vkUnmapMemory(',`VkResult vt_call_vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
    VT_CALL_LOCK();
    VkResult result = dgMapMemory(memory, offset, size, flags, NULL, ppData);
    VT_CALL_UNLOCK();
    return result;
}`);
  body('void vt_call_vkUnmapMemory(','VkResult vt_call_vkFlushMappedMemoryRanges(',`void vt_call_vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {
    VT_CALL_LOCK();
    dgUnmapMemory(VkObject_fromHandle(memory)->tag, false);
    VT_CALL_UNLOCK();
}`);
  body('VkResult vt_call_vkMapMemory2KHR(','VkResult vt_call_vkUnmapMemory2KHR(',`VkResult vt_call_vkMapMemory2KHR(VkDevice device, const VkMemoryMapInfoKHR* info, void** ppData) {
    if (!info) return VK_ERROR_MEMORY_MAP_FAILED;
    const VkMemoryMapPlacedInfoEXT* placed = findNextVkStructure(info->pNext, VK_STRUCTURE_TYPE_MEMORY_MAP_PLACED_INFO_EXT);
    VT_CALL_LOCK();
    VkResult result = dgMapMemory(info->memory, info->offset, info->size, info->flags, placed ? placed->pPlacedAddress : NULL, ppData);
    VT_CALL_UNLOCK();
    return result;
}`);
  body('VkResult vt_call_vkUnmapMemory2KHR(','static const struct VulkanFunc vkDispatchTable[]',`VkResult vt_call_vkUnmapMemory2KHR(VkDevice device, const VkMemoryUnmapInfoKHR* info) {
    if (!info || (info->flags & ~VK_MEMORY_UNMAP_RESERVE_BIT_EXT)) return VK_ERROR_MEMORY_MAP_FAILED;
    VT_CALL_LOCK();
    VkResult result = dgUnmapMemory(VkObject_fromHandle(info->memory)->tag, (info->flags & VK_MEMORY_UNMAP_RESERVE_BIT_EXT) != 0);
    VT_CALL_UNLOCK();
    return result;
}`);
  const old='    if (memoryObject->tag) MEMFREE(memoryObject->tag);';
  if(calls.split(old).length!==2)throw Error('Unexpected mapped memory destruction');
  calls=calls.replace(old,'    if (memoryObject->tag) {\n        dgUnmapMemory(memoryObject->tag, false);\n        MEMFREE(memoryObject->tag);\n    }');
  const marker='VkResult vt_call_vkAllocateMemory(';
  if(!calls.includes(marker))throw Error('Missing memory allocation entry');
  return calls.replace(marker,fs.readFileSync('scripts/vortek-client/map-memory.inc','utf8').replaceAll('\r\n','\n')+'\n'+marker);
}
