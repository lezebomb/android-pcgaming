// Exercise the product shader inspector with a recording Vulkan module API.
// No GPU/driver emulation: this checks CPU ownership/conversion and failures.
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#define WINLATOR_H
#define VORTEK_H
#define VULKAN_HELPER_H
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"
#include "arrays.h"
typedef unsigned char u_char;
typedef struct VkContext VkContext;
#include "spirv.h"
#include "shader_inspector.h"
#define MAKE_ENGINE_VERSION(x,y,z) (((x)<<22)|((y)<<12)|(z))
typedef struct VkContext { VkDriverId driverID; char* engineName; uint32_t engineVersion; } VkContext;
static unsigned moduleCalls, intTypes;
static VkResult forcedResult = VK_SUCCESS;
static VkResult VKAPI_PTR recordModule(VkDevice device, const VkShaderModuleCreateInfo* info, const VkAllocationCallbacks* allocator, VkShaderModule* out) {
    (void)device; (void)allocator;
    moduleCalls++;
    intTypes = 0;
    for (size_t i = 5; i < info->codeSize/4; i += info->pCode[i] >> 16)
        if ((info->pCode[i] & 0xffff) == SpvOpTypeInt) intTypes++;
    *out = forcedResult == VK_SUCCESS ? (VkShaderModule)(uintptr_t)moduleCalls : VK_NULL_HANDLE;
    return forcedResult;
}
static struct { PFN_vkCreateShaderModule vkCreateShaderModule; } vulkanWrapper = {recordModule};
#include "../../app/src/main/cpp/vortekrenderer/src/shader_inspector.c"
#include "../../app/src/main/cpp/winlator/arrays.c"
#define CHECK(x) do { if (!(x)) { printf("shader_failed_line=%d\n",__LINE__); return 1; } } while(0)
#define OP(n,op) (((n)<<16)|(op))
int main(void) {
    static const uint32_t vertex[] = {
        SpvMagicNumber, 0x10000, 0, 9, 0,
        OP(2,SpvOpCapability), SpvCapabilityShader,
        OP(3,SpvOpMemoryModel), SpvAddressingModelLogical, SpvMemoryModelGLSL450,
        OP(6,SpvOpEntryPoint), SpvExecutionModelVertex, 7, 0x6e69616d, 0, 6,
        OP(4,SpvOpDecorate), 6, SpvDecorationLocation, 0,
        OP(2,SpvOpTypeVoid), 1,
        OP(3,SpvOpTypeFloat), 2, 32,
        OP(4,SpvOpTypeVector), 3, 2, 2,
        OP(4,SpvOpTypePointer), 4, SpvStorageClassInput, 3,
        OP(4,SpvOpVariable), 4, 6, SpvStorageClassInput,
        OP(3,SpvOpTypeFunction), 5, 1,
        OP(5,SpvOpFunction), 1, 7, 0, 5,
        OP(2,SpvOpLabel), 8, OP(1,SpvOpReturn), OP(1,SpvOpFunctionEnd)
    };
    ShaderInspector inspector = {.convertFormatScaled=true};
    ShaderModule* module;
    CHECK(ShaderInspector_createModule(&inspector, VK_NULL_HANDLE, vertex, sizeof(vertex), &module) == VK_SUCCESS);
    CHECK(module && module->code && !memcmp(module->code, vertex, sizeof(vertex)));
    VkVertexInputAttributeDescription attribute = {0,0,VK_FORMAT_R8G8_USCALED,0};
    VkPipelineVertexInputStateCreateInfo input = {.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,.vertexAttributeDescriptionCount=1,.pVertexAttributeDescriptions=&attribute};
    VkPipelineShaderStageCreateInfo stage = {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_VERTEX_BIT,.module=(VkShaderModule)module};
    ArrayList temporary={0};
    CHECK(ShaderInspector_inspectShaderStages(&inspector, VK_NULL_HANDLE, &stage, 1, &input, &temporary) == VK_SUCCESS);
    CHECK(intTypes == 1 && temporary.size == 1 && attribute.format == VK_FORMAT_R8G8_UINT);
    CHECK(module->module == VK_NULL_HANDLE && !memcmp(module->code, vertex, sizeof(vertex)));
    attribute.format = VK_FORMAT_R32G32_SFLOAT;
    stage.module = (VkShaderModule)module;
    CHECK(ShaderInspector_inspectShaderStages(&inspector, VK_NULL_HANDLE, &stage, 1, &input, &temporary) == VK_SUCCESS);
    CHECK(intTypes == 0 && temporary.size == 2 && moduleCalls == 2);
    forcedResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    stage.module = (VkShaderModule)module;
    CHECK(ShaderInspector_inspectShaderStages(&inspector, VK_NULL_HANDLE, &stage, 1, &input, &temporary) == VK_ERROR_OUT_OF_DEVICE_MEMORY);
    CHECK(temporary.size == 2 && !memcmp(module->code, vertex, sizeof(vertex)));
    uint32_t bad[] = {SpvMagicNumber,0x10000,0,1,0,SpvOpNop};
    ShaderModule* rejected = (ShaderModule*)(uintptr_t)1;
    CHECK(ShaderInspector_createModule(&inspector,VK_NULL_HANDLE,bad,sizeof(bad),&rejected) != VK_SUCCESS && rejected == NULL);
    uint32_t name[20] = {SpvMagicNumber,0x10000,0,2,0,OP(14,SpvOpName),1};
    memset(&name[7],'v',44); ((char*)&name[7])[43]=0;
    struct { char text[32]; uint32_t guard; } output={{0},0x12345678};
    getSpvVariableName(name,19*4,1,output.text,sizeof(output.text));
    CHECK(output.text[0]==0 && output.guard==0x12345678);
    SpvInstFilter filter={{99},{1},1,0};
    CHECK(fetchSpvInstIndex2((uint32_t*)vertex,sizeof(vertex),5,SpvOpTypeVector,&filter,false)==-1);
    ArrayList_free(&temporary,false); free(module->code); free(module);
    puts("vortek_shader_result=0 (shared module variants, source ownership, error, bounds)");
    return 0;
}
