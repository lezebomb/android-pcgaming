// Optional actual Shader Model 6 draw, reusing the bounded Present/readback harness.
#include <stdlib.h>
static ID3D12RootSignature *shader_root;
static ID3D12PipelineState *shader_pipeline;
#ifdef DG_BC_TEXTURE_CHECK
#include "d3d12-bc-textures.h"
#endif
typedef HRESULT (WINAPI *SerializeRootFn)(const D3D12_ROOT_SIGNATURE_DESC *, D3D_ROOT_SIGNATURE_VERSION,
                                        ID3DBlob **, ID3DBlob **);
static D3D12_SHADER_BYTECODE shader_read(const char *path) {
    D3D12_SHADER_BYTECODE code = {0};
    FILE *file = fopen(path, "rb");
    if (!file) return code;
    fseek(file, 0, SEEK_END); long size = ftell(file); rewind(file);
    if (size > 0 && size <= 1024 * 1024) {
        void *data = malloc((size_t)size);
        if (data && fread(data, 1, (size_t)size, file) == (size_t)size) {
            code.pShaderBytecode = data; code.BytecodeLength = (SIZE_T)size;
        } else free(data);
    }
    fclose(file); return code;
}
static int shader_prepare(ID3D12Device *device, HMODULE module) {
    SerializeRootFn serialize = (SerializeRootFn)(void *)GetProcAddress(module, "D3D12SerializeRootSignature");
    D3D12_ROOT_SIGNATURE_DESC root_desc = {0};
    root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
#ifdef DG_BC_TEXTURE_CHECK
    D3D12_DESCRIPTOR_RANGE range = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
    D3D12_ROOT_PARAMETER parameter = {0}; parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameter.DescriptorTable.NumDescriptorRanges = 1; parameter.DescriptorTable.pDescriptorRanges = &range;
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler = {0}; sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = 3.402823466e+38F; sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    root_desc.NumParameters = root_desc.NumStaticSamplers = 1;
    root_desc.pParameters = &parameter; root_desc.pStaticSamplers = &sampler;
#endif
    ID3DBlob *blob = NULL, *errors = NULL;
    if (!serialize) return 0;
    HRESULT hr = serialize(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors);
    if (errors) errors->lpVtbl->Release(errors);
    if (!check(hr, "serialize_root")) return 0;
    hr = ID3D12Device_CreateRootSignature(device, 0, blob->lpVtbl->GetBufferPointer(blob),
            blob->lpVtbl->GetBufferSize(blob), &IID_ID3D12RootSignature, (void **)&shader_root);
    blob->lpVtbl->Release(blob);
    if (!check(hr, "shader_root")) return 0;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {0};
    desc.pRootSignature = shader_root;
    desc.VS = shader_read("C:/DGSmoke/gradient-vs.dxil");
#ifdef DG_BC_TEXTURE_CHECK
    desc.PS = shader_read("C:/DGSmoke/texture-ps.dxil");
#else
    desc.PS = shader_read("C:/DGSmoke/gradient-ps.dxil");
#endif
    if (!desc.VS.pShaderBytecode || !desc.PS.pShaderBytecode) {
        free((void *)desc.VS.pShaderBytecode); free((void *)desc.PS.pShaderBytecode);
        fprintf(output, "shader_files_missing=1\n"); return 0;
    }
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    D3D12_RENDER_TARGET_BLEND_DESC *blend = &desc.BlendState.RenderTarget[0];
    blend->SrcBlend = blend->SrcBlendAlpha = D3D12_BLEND_ONE;
    blend->DestBlend = blend->DestBlendAlpha = D3D12_BLEND_ZERO;
    blend->BlendOp = blend->BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend->LogicOp = D3D12_LOGIC_OP_NOOP;
    blend->RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.DepthStencilState.BackFace = desc.DepthStencilState.FrontFace;
    desc.SampleMask = 0xffffffff;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1; desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    hr = ID3D12Device_CreateGraphicsPipelineState(device, &desc, &IID_ID3D12PipelineState, (void **)&shader_pipeline);
    free((void *)desc.VS.pShaderBytecode); free((void *)desc.PS.pShaderBytecode);
    if (!check(hr, "shader_pipeline")) return 0;
#ifdef DG_BC_TEXTURE_CHECK
    return bc_prepare(device);
#else
    return 1;
#endif
}
static void shader_draw(ID3D12GraphicsCommandList *list, D3D12_CPU_DESCRIPTOR_HANDLE rtv) {
#ifdef DG_BC_TEXTURE_CHECK
    bc_draw(list, rtv);
#else
    D3D12_VIEWPORT viewport = {0, 0, 320, 200, 0, 1};
    D3D12_RECT scissor = {0, 0, 320, 200};
    ID3D12GraphicsCommandList_SetPipelineState(list, shader_pipeline);
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, shader_root);
    ID3D12GraphicsCommandList_RSSetViewports(list, 1, &viewport);
    ID3D12GraphicsCommandList_RSSetScissorRects(list, 1, &scissor);
    ID3D12GraphicsCommandList_OMSetRenderTargets(list, 1, &rtv, FALSE, NULL);
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D12GraphicsCommandList_DrawInstanced(list, 3, 1, 0, 0);
#endif
}
static int shader_matches(const BYTE *pixel, UINT x, UINT y) {
#ifdef DG_BC_TEXTURE_CHECK
    return bc_matches(pixel, x, y);
#else
    int expected[4] = {(int)(((x + 0.5f) / 320) * 255 + 0.5f),
                       (int)(((y + 0.5f) / 200) * 255 + 0.5f), 64, 255};
    for (int i = 0; i < 4; i++) if (abs((int)pixel[i] - expected[i]) > 2) return 0;
    return 1;
#endif
}
