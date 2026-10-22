// Paired wire correction; VkMemoryHeap has native tail padding on ARM64.
export function patchMemoryWire(text) {
  const encode='    if (val->memoryHeapCount <= VK_MAX_MEMORY_HEAPS) memcpy(outputBuffer + bufferOffset, val->memoryHeaps, val->memoryHeapCount * vt_sizeof_VkMemoryHeap(&val->memoryHeaps[0]));';
  const decode='    if (val->memoryHeapCount <= VK_MAX_MEMORY_HEAPS) memcpy(val->memoryHeaps, inputBuffer + bufferOffset, val->memoryHeapCount * vt_sizeof_VkMemoryHeap(&val->memoryHeaps[0]));';
  const newEncode='    for (uint32_t i = 0; i < val->memoryHeapCount && i < VK_MAX_MEMORY_HEAPS; i++)\n        vt_serialize_VkMemoryHeap(&val->memoryHeaps[i], outputBuffer + bufferOffset + i * vt_sizeof_VkMemoryHeap(&val->memoryHeaps[0]));';
  const newDecode='    for (uint32_t i = 0; i < val->memoryHeapCount && i < VK_MAX_MEMORY_HEAPS; i++)\n        vt_unserialize_VkMemoryHeap(&val->memoryHeaps[i], inputBuffer + bufferOffset + i * vt_sizeof_VkMemoryHeap(&val->memoryHeaps[0]), memoryPool);';
  for (const [old, next] of [[encode,newEncode],[decode,newDecode]]) {
    if (text.includes(old)) text=text.replace(old,next);
    else if (!text.includes(next)) throw Error('Unexpected memory heap serializer');
  }
  const begin=text.indexOf('static inline int vt_sizeof_VkPhysicalDeviceMemoryBudgetPropertiesEXT(');
  const end=text.indexOf('static inline int vt_sizeof_VkQueueFamilyProperties(',begin);
  if(begin<0||end<begin)throw Error('Missing memory budget serializer');
  let budget=text.slice(begin,end);
  if(!budget.includes('sizeof(VkDeviceSize)')) {
    if((budget.match(/VK_MAX_MEMORY_HEAPS/g)||[]).length!==10)throw Error('Unexpected memory budget array sizing');
    budget=budget.replaceAll('VK_MAX_MEMORY_HEAPS','(VK_MAX_MEMORY_HEAPS * sizeof(VkDeviceSize))');
  }
  return text.slice(0,begin)+budget+text.slice(end);
}
