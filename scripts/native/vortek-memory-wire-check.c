#include <limits.h>
#include "vortek.h"
#include "vortek_serializer.h"
bool vortekSerializerCastVkObject = false;
#define CHECK(x) do { if (!(x)) { printf("wire_failed_line=%d\n",__LINE__); return 1; } } while(0)
int main(void) {
 VkPhysicalDeviceMemoryProperties input={0},output;
 input.memoryTypeCount=2;input.memoryTypes[0]=(VkMemoryType){VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,0};input.memoryTypes[1]=(VkMemoryType){VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,1};
 input.memoryHeapCount=2;input.memoryHeaps[0]=(VkMemoryHeap){0x1122334455667788ull,VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};input.memoryHeaps[1]=(VkMemoryHeap){0x2233445566778899ull,VK_MEMORY_HEAP_MULTI_INSTANCE_BIT};
 char wire[4096]={0};memset(&output,0xa5,sizeof(output));vt_serialize_VkPhysicalDeviceMemoryProperties(&input,wire);vt_unserialize_VkPhysicalDeviceMemoryProperties(&output,wire,NULL);
 CHECK(output.memoryHeapCount==2 && output.memoryTypeCount==2);
 CHECK(output.memoryHeaps[0].size==0x1122334455667788ull && output.memoryHeaps[0].flags==VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);
 CHECK(output.memoryHeaps[1].size==0x2233445566778899ull && output.memoryHeaps[1].flags==VK_MEMORY_HEAP_MULTI_INSTANCE_BIT);
 CHECK((unsigned char)wire[44]==VK_MEMORY_HEAP_MULTI_INSTANCE_BIT);
 VkPhysicalDeviceMemoryBudgetPropertiesEXT a={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT},b={0};
 for(unsigned i=0;i<VK_MAX_MEMORY_HEAPS;i++){a.heapBudget[i]=0x100000000ull+i;a.heapUsage[i]=0x200000000ull+i;}
 CHECK(vt_sizeof_VkPhysicalDeviceMemoryBudgetPropertiesEXT(&a)==260);
 vt_serialize_VkPhysicalDeviceMemoryBudgetPropertiesEXT(&a,wire);vt_unserialize_VkPhysicalDeviceMemoryBudgetPropertiesEXT(&b,wire,NULL);
 for(unsigned i=0;i<VK_MAX_MEMORY_HEAPS;i++)CHECK(b.heapBudget[i]==0x100000000ull+i && b.heapUsage[i]==0x200000000ull+i);
 printf("memory_wire_result=0 native_heap_size=%zu\n",sizeof(VkMemoryHeap));return 0;
}
