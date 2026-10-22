// Four one-block textures exercise upload, BC decompression and descriptor sampling.
#include <string.h>
static ID3D12DescriptorHeap *bc_heap;
static ID3D12Resource *bc_textures[4], *bc_upload;
static D3D12_PLACED_SUBRESOURCE_FOOTPRINT bc_footprints[4];
static D3D12_GPU_DESCRIPTOR_HANDLE bc_gpu;
static UINT bc_increment;
static int bc_uploaded;
static const DXGI_FORMAT bc_formats[4] = {
    DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_BC5_UNORM, DXGI_FORMAT_BC7_UNORM
};
static const BYTE bc_expected[4][4] = {{255,0,0,255}, {0,255,0,255}, {64,192,0,255}, {0,0,254,254}};
static void bc_bits(BYTE *block, UINT *position, UINT value, UINT count) {
    for (UINT i = 0; i < count; i++, (*position)++)
        if ((value >> i) & 1) block[*position / 8] |= (BYTE)(1u << (*position % 8));
}
static int bc_prepare(ID3D12Device *device) {
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {0};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.NumDescriptors = 4; heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (!check(ID3D12Device_CreateDescriptorHeap(device, &heap_desc, &IID_ID3D12DescriptorHeap, (void **)&bc_heap), "bc_srv_heap")) return 0;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu;
    bc_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(bc_heap, &cpu);
    bc_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(bc_heap, &bc_gpu);
    bc_increment = ID3D12Device_GetDescriptorHandleIncrementSize(device, heap_desc.Type);
    D3D12_HEAP_PROPERTIES props = {0}; props.Type = D3D12_HEAP_TYPE_DEFAULT;
    UINT64 total_size = 0;
    for (UINT i = 0; i < 4; i++) {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support = {0}; support.Format = bc_formats[i];
        if (!check(ID3D12Device_CheckFeatureSupport(device, D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)), "bc_format_support")) return 0;
        fprintf(output, "bc_format_%u=%u support1=0x%x\n", i, (UINT)bc_formats[i], (UINT)support.Support1);
        D3D12_RESOURCE_DESC desc = {0}; desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = desc.Height = 4; desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.Format = bc_formats[i]; desc.SampleDesc.Count = 1;
        if (!check(ID3D12Device_CreateCommittedResource(device, &props, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void **)&bc_textures[i]), "bc_texture")) return 0;
        UINT64 size;
        ID3D12Device_GetCopyableFootprints(device, &desc, 0, 1, i * 512, &bc_footprints[i], NULL, NULL, &size);
        if (i * 512 + size > total_size) total_size = i * 512 + size;
        D3D12_SHADER_RESOURCE_VIEW_DESC srv = {0}; srv.Format = desc.Format;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2D.MipLevels = 1;
        ID3D12Device_CreateShaderResourceView(device, bc_textures[i], &srv, cpu);
        cpu.ptr += bc_increment;
    }
    props.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC upload_desc = {0}; upload_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    upload_desc.Width = total_size + 512; upload_desc.Height = upload_desc.DepthOrArraySize = upload_desc.MipLevels = 1;
    upload_desc.SampleDesc.Count = 1; upload_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (!check(ID3D12Device_CreateCommittedResource(device, &props, D3D12_HEAP_FLAG_NONE, &upload_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void **)&bc_upload), "bc_upload")) return 0;
    BYTE *mapped = NULL; D3D12_RANGE no_read = {0,0};
    if (!check(ID3D12Resource_Map(bc_upload, 0, &no_read, (void **)&mapped), "bc_upload_map")) return 0;
    memset(mapped, 0, (size_t)upload_desc.Width);
    BYTE *bc1 = mapped + bc_footprints[0].Offset;
    bc1[0] = 0; bc1[1] = 0xf8; // RGB565 red, all selectors zero.
    BYTE *bc3 = mapped + bc_footprints[1].Offset;
    bc3[0] = 255; bc3[8] = 0xe0; bc3[9] = 7; // opaque green.
    BYTE *bc5 = mapped + bc_footprints[2].Offset;
    bc5[0] = 64; bc5[8] = 192;
    BYTE *bc7 = mapped + bc_footprints[3].Offset;
    UINT bit = 0;
    // Microsoft BC7 mode 6: 7-bit mode prefix, eight 7-bit endpoint fields,
    // two endpoint p-bits, 63 index bits (anchor omits its high bit).
    bc_bits(bc7, &bit, 1u << 6, 7);
    for (UINT component = 0; component < 4; component++) {
        bc_bits(bc7, &bit, component >= 2 ? 127 : 0, 7);
        bc_bits(bc7, &bit, component >= 2 ? 127 : 0, 7);
    }
    bc_bits(bc7, &bit, 0, 2);
    bc_bits(bc7, &bit, 0, 3);
    for (UINT i = 1; i < 16; i++) bc_bits(bc7, &bit, 0, 4);
    if (bit != 128) { ID3D12Resource_Unmap(bc_upload, 0, NULL); return 0; }
    D3D12_RANGE written = {0, (SIZE_T)upload_desc.Width};
    ID3D12Resource_Unmap(bc_upload, 0, &written);
    return 1;
}
static void bc_draw(ID3D12GraphicsCommandList *list, D3D12_CPU_DESCRIPTOR_HANDLE rtv) {
    if (!bc_uploaded) {
        for (UINT i = 0; i < 4; i++) {
            D3D12_TEXTURE_COPY_LOCATION source = {0}, target = {0};
            source.pResource = bc_upload; source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source.PlacedFootprint = bc_footprints[i];
            target.pResource = bc_textures[i]; target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            ID3D12GraphicsCommandList_CopyTextureRegion(list, &target, 0, 0, 0, &source, NULL);
            D3D12_RESOURCE_BARRIER barrier = {0}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = bc_textures[i]; barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &barrier);
        }
        bc_uploaded = 1;
    }
    ID3D12GraphicsCommandList_SetPipelineState(list, shader_pipeline);
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, shader_root);
    ID3D12GraphicsCommandList_SetDescriptorHeaps(list, 1, &bc_heap);
    ID3D12GraphicsCommandList_OMSetRenderTargets(list, 1, &rtv, FALSE, NULL);
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (UINT i = 0; i < 4; i++) {
        UINT x = (i % 2) * 160, y = (i / 2) * 100;
        D3D12_VIEWPORT viewport = {(FLOAT)x, (FLOAT)y, 160, 100, 0, 1};
        D3D12_RECT scissor = {(LONG)x, (LONG)y, (LONG)x + 160, (LONG)y + 100};
        D3D12_GPU_DESCRIPTOR_HANDLE descriptor = {bc_gpu.ptr + i * bc_increment};
        ID3D12GraphicsCommandList_RSSetViewports(list, 1, &viewport);
        ID3D12GraphicsCommandList_RSSetScissorRects(list, 1, &scissor);
        ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(list, 0, descriptor);
        ID3D12GraphicsCommandList_DrawInstanced(list, 3, 1, 0, 0);
    }
}
static int bc_matches(const BYTE *pixel, UINT x, UINT y) {
    UINT i = (y >= 100 ? 2 : 0) + (x >= 160 ? 1 : 0);
    for (UINT channel = 0; channel < 4; channel++)
        if (abs((int)pixel[channel] - (int)bc_expected[i][channel]) > 2) return 0;
    return 1;
}
static void bc_cleanup(void) {
    for (UINT i = 0; i < 4; i++) if (bc_textures[i]) ID3D12Resource_Release(bc_textures[i]);
    if (bc_upload) ID3D12Resource_Release(bc_upload);
    if (bc_heap) ID3D12DescriptorHeap_Release(bc_heap);
}
