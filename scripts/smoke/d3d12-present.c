// One bounded FL12_0 clear/readback/Present check; not a shader or game benchmark.
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdio.h>

typedef HRESULT (WINAPI *CreateDeviceFn)(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **);
static FILE *output;
static int check(HRESULT hr, const char *stage) {
    fprintf(output, "%s=0x%08lx\n", stage, (unsigned long)hr);
    fflush(output);
    return SUCCEEDED(hr);
}
static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w, LPARAM l) {
    return DefWindowProcA(window, message, w, l);
}
static int wait_gpu(ID3D12CommandQueue *queue, ID3D12Fence *fence, HANDLE event, UINT64 value) {
    if (!check(ID3D12CommandQueue_Signal(queue, fence, value), "queue_signal")) return 0;
    if (!check(ID3D12Fence_SetEventOnCompletion(fence, value, event), "fence_event")) return 0;
    if (WaitForSingleObject(event, 5000) != WAIT_OBJECT_0) {
        fprintf(output, "gpu_wait_timeout=1\n"); fflush(output); return 0;
    }
    return 1;
}
#ifdef DG_SHADER_CHECK
#include "d3d12-shader-check.h"
#endif
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show) {
    (void)previous; (void)command; (void)show;
    CreateDirectoryA("C:\\DGSmoke", NULL);
#ifdef DG_BC_TEXTURE_CHECK
    output = fopen("C:/DGSmoke/d3d12-bc-textures.txt", "w");
#elif defined(DG_SHADER_CHECK)
    output = fopen("C:/DGSmoke/d3d12-shader.txt", "w");
#else
    output = fopen("C:\\DGSmoke\\d3d12-present.txt", "w");
#endif
    if (!output) return 2;
    int result = 1;
    HMODULE module = LoadLibraryA("d3d12.dll");
    CreateDeviceFn create = module ? (CreateDeviceFn)(void *)GetProcAddress(module, "D3D12CreateDevice") : NULL;
    ID3D12Device *device = NULL;
    ID3D12CommandQueue *queue = NULL;
    IDXGIFactory2 *factory = NULL;
    IDXGISwapChain1 *chain1 = NULL;
    IDXGISwapChain3 *chain = NULL;
    ID3D12DescriptorHeap *heap = NULL;
    ID3D12Resource *buffers[2] = {NULL, NULL}, *readback = NULL;
    ID3D12CommandAllocator *allocator = NULL;
    ID3D12GraphicsCommandList *list = NULL;
    ID3D12Fence *fence = NULL;
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    HWND window = NULL;
    if (!create || !event || !check(create(NULL, D3D_FEATURE_LEVEL_12_0, &IID_ID3D12Device, (void **)&device), "create_fl12_0")) goto done;
#ifdef DG_SHADER_CHECK
    if (!shader_prepare(device, module)) goto done;
#endif
    WNDCLASSA wc = {0}; wc.hInstance = instance; wc.lpfnWndProc = window_proc; wc.lpszClassName = "DimensityD3D12Present";
    RegisterClassA(&wc);
    window = CreateWindowA(wc.lpszClassName, "Dimensity DX12 clear / readback", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        40, 40, 640, 480, NULL, NULL, instance, NULL);
    if (!window) goto done;
    D3D12_COMMAND_QUEUE_DESC queue_desc = {0};
    if (!check(ID3D12Device_CreateCommandQueue(device, &queue_desc, &IID_ID3D12CommandQueue, (void **)&queue), "queue")) goto done;
    if (!check(CreateDXGIFactory2(0, &IID_IDXGIFactory2, (void **)&factory), "factory")) goto done;
    DXGI_SWAP_CHAIN_DESC1 swap_desc = {0};
    swap_desc.Width = 320; swap_desc.Height = 200; swap_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_desc.SampleDesc.Count = 1; swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.BufferCount = 2; swap_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    if (!check(IDXGIFactory2_CreateSwapChainForHwnd(factory, (IUnknown *)queue, window, &swap_desc, NULL, NULL, &chain1), "swapchain")) goto done;
    if (!check(IDXGISwapChain1_QueryInterface(chain1, &IID_IDXGISwapChain3, (void **)&chain), "swapchain3")) goto done;
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {0}; heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heap_desc.NumDescriptors = 2;
    if (!check(ID3D12Device_CreateDescriptorHeap(device, &heap_desc, &IID_ID3D12DescriptorHeap, (void **)&heap), "rtv_heap")) goto done;
    D3D12_CPU_DESCRIPTOR_HANDLE handles[2];
    // MinGW's COM ABI exposes structure-return methods through an output pointer in C.
    heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(heap, &handles[0]);
    handles[1].ptr = handles[0].ptr + ID3D12Device_GetDescriptorHandleIncrementSize(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (UINT i = 0; i < 2; i++) {
        if (!check(IDXGISwapChain3_GetBuffer(chain, i, &IID_ID3D12Resource, (void **)&buffers[i]), "backbuffer")) goto done;
        ID3D12Device_CreateRenderTargetView(device, buffers[i], NULL, handles[i]);
    }
    D3D12_RESOURCE_DESC buffer_desc;
    buffers[0]->lpVtbl->GetDesc(buffers[0], &buffer_desc);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT64 total_size;
    ID3D12Device_GetCopyableFootprints(device, &buffer_desc, 0, 1, 0, &footprint, NULL, NULL, &total_size);
    D3D12_HEAP_PROPERTIES props = {0}; props.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC read_desc = {0}; read_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    read_desc.Width = total_size; read_desc.Height = 1; read_desc.DepthOrArraySize = 1; read_desc.MipLevels = 1;
    read_desc.SampleDesc.Count = 1; read_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (!check(ID3D12Device_CreateCommittedResource(device, &props, D3D12_HEAP_FLAG_NONE, &read_desc,
        D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void **)&readback), "readback")) goto done;
    if (!check(ID3D12Device_CreateCommandAllocator(device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&allocator), "allocator")) goto done;
    if (!check(ID3D12Device_CreateCommandList(device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, NULL, &IID_ID3D12GraphicsCommandList, (void **)&list), "command_list")) goto done;
    ID3D12GraphicsCommandList_Close(list);
    if (!check(ID3D12Device_CreateFence(device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&fence), "fence")) goto done;
    const FLOAT colors[4][4] = {{1,0,0,1}, {0,1,0,1}, {0,0,1,1}, {1,1,0,1}};
    const D3D12_RECT rectangles[4] = {{0,0,160,100}, {160,0,320,100}, {0,100,160,200}, {160,100,320,200}};
    UINT successful_frames = 0;
#ifdef DG_SHADER_CHECK
    const UINT frame_count = 12;
#else
    const UINT frame_count = 60;
#endif
    for (UINT frame = 0; frame < frame_count; frame++) {
        MSG message; while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageA(&message); }
        UINT index = IDXGISwapChain3_GetCurrentBackBufferIndex(chain);
        if (!check(ID3D12CommandAllocator_Reset(allocator), "allocator_reset") ||
            !check(ID3D12GraphicsCommandList_Reset(list, allocator, NULL), "list_reset")) goto done;
        D3D12_RESOURCE_BARRIER barrier = {0}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = buffers[index]; barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &barrier);
#ifdef DG_SHADER_CHECK
        shader_draw(list, handles[index]);
#else
        for (UINT color = 0; color < 4; color++) ID3D12GraphicsCommandList_ClearRenderTargetView(list, handles[index], colors[color], 1, &rectangles[color]);
#endif
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &barrier);
        D3D12_TEXTURE_COPY_LOCATION source = {0}, target = {0};
        source.pResource = buffers[index]; source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        target.pResource = readback; target.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; target.PlacedFootprint = footprint;
        ID3D12GraphicsCommandList_CopyTextureRegion(list, &target, 0, 0, 0, &source, NULL);
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE; barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &barrier);
        if (!check(ID3D12GraphicsCommandList_Close(list), "list_close")) goto done;
        ID3D12CommandList *submitted = (ID3D12CommandList *)list;
        ID3D12CommandQueue_ExecuteCommandLists(queue, 1, &submitted);
        if (!wait_gpu(queue, fence, event, frame + 1)) goto done;
        const BYTE *pixels = NULL; D3D12_RANGE range = {0, (SIZE_T)total_size};
        if (!check(ID3D12Resource_Map(readback, 0, &range, (void **)&pixels), "map")) goto done;
        int matches = 1;
        for (UINT color = 0; color < 4; color++) {
            UINT x = (color % 2) * 160 + 80, y = (color / 2) * 100 + 50;
            const BYTE *pixel = pixels + footprint.Offset + y * footprint.Footprint.RowPitch + x * 4;
#ifdef DG_SHADER_CHECK
            if (!shader_matches(pixel, x, y)) matches = 0;
#else
            for (UINT channel = 0; channel < 4; channel++) if (pixel[channel] != (BYTE)(colors[color][channel] * 255)) matches = 0;
#endif
            if (frame == 0) fprintf(output, "pixel_%u=%u,%u,%u,%u\n", color, pixel[0], pixel[1], pixel[2], pixel[3]);
        }
        D3D12_RANGE no_write = {0,0}; ID3D12Resource_Unmap(readback, 0, &no_write);
        if (!matches) { fprintf(output, "readback_mismatch=1\n"); fflush(output); goto done; }
        if (!check(IDXGISwapChain3_Present(chain, 1, 0), "present")) goto done;
        successful_frames++;
        Sleep(100);
    }
    fprintf(output, "frames_presented=%u readback_verified=1\n", successful_frames);
    result = 0;
done:
#ifdef DG_SHADER_CHECK
    fprintf(output, "complete=1 result=%d dxil_draw=1\n", result);
#ifdef DG_BC_TEXTURE_CHECK
    fprintf(output, "bc_sampling=1\n");
#endif
#else
    fprintf(output, "complete=1 result=%d clear_copy_present_only=1\n", result);
#endif
    fflush(output);
    // On a timed-out GPU let process teardown own resources; no indefinite release wait.
    if (result == 0) {
#ifdef DG_SHADER_CHECK
#ifdef DG_BC_TEXTURE_CHECK
        bc_cleanup();
#endif
        if (shader_pipeline) ID3D12PipelineState_Release(shader_pipeline);
        if (shader_root) ID3D12RootSignature_Release(shader_root);
#endif
        if (fence) ID3D12Fence_Release(fence);
        if (list) ID3D12GraphicsCommandList_Release(list);
        if (allocator) ID3D12CommandAllocator_Release(allocator);
        if (readback) ID3D12Resource_Release(readback);
        for (UINT i=0;i<2;i++) if (buffers[i]) ID3D12Resource_Release(buffers[i]);
        if (heap) ID3D12DescriptorHeap_Release(heap);
        if (chain) IDXGISwapChain3_Release(chain);
        if (chain1) IDXGISwapChain1_Release(chain1);
        if (factory) IDXGIFactory2_Release(factory);
        if (queue) ID3D12CommandQueue_Release(queue);
        if (device) ID3D12Device_Release(device);
    }
    if (event) CloseHandle(event);
    if (window) DestroyWindow(window);
    fclose(output);
    return result;
}
