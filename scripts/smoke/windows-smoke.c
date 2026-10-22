// Bounded Direct3D/XInput fixture. No game assets or external runtime DLLs.
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <xinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *report;
static HWND window;
static int running = 1;
static unsigned buttons_seen, connected_samples, changes;
static SHORT axis_min[4], axis_max[4];
static BYTE trigger_max[2];
static XINPUT_STATE previous;
typedef DWORD (WINAPI *GetStateFn)(DWORD, XINPUT_STATE *);
static GetStateFn get_state;
static DWORD graphics_duration = 3500;
static int focus_check, focus_held, focus_neutral, focus_resumed, focus_released;

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CLOSE) { running = 0; DestroyWindow(hwnd); return 0; }
    if (msg == WM_DESTROY) { running = 0; return 0; }
    return DefWindowProcA(hwnd, msg, wp, lp);
}
static void tick(void) {
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    XINPUT_STATE state = {0};
    if (get_state && get_state(0, &state) == ERROR_SUCCESS) {
        connected_samples++;
        XINPUT_GAMEPAD *p = &state.Gamepad;
        if (focus_check) {
            if (!focus_held && (p->wButtons & XINPUT_GAMEPAD_A)) {
                focus_held=1; fprintf(report,"held_seen=1\n");
            } else if (focus_held && !focus_neutral && !p->wButtons && !p->bLeftTrigger && !p->bRightTrigger
                    && abs(p->sThumbLX)<=1 && abs(p->sThumbLY)<=1 && abs(p->sThumbRX)<=1 && abs(p->sThumbRY)<=1) {
                focus_neutral=1; fprintf(report,"neutral_after_hold=1\n");
            } else if (focus_neutral && !focus_resumed && (p->wButtons & XINPUT_GAMEPAD_A)) {
                focus_resumed=1; fprintf(report,"fresh_press_after_focus=1\n");
            } else if (focus_resumed && !(p->wButtons & XINPUT_GAMEPAD_A)) {
                focus_released=1; fprintf(report,"fresh_release=1\n"); running=0;
            }
            fflush(report);
        }
        buttons_seen |= p->wButtons;
        SHORT axes[] = {p->sThumbLX,p->sThumbLY,p->sThumbRX,p->sThumbRY};
        for (int i = 0; i < 4; i++) {
            if (axes[i] < axis_min[i]) axis_min[i] = axes[i];
            if (axes[i] > axis_max[i]) axis_max[i] = axes[i];
        }
        if (p->bLeftTrigger > trigger_max[0]) trigger_max[0] = p->bLeftTrigger;
        if (p->bRightTrigger > trigger_max[1]) trigger_max[1] = p->bRightTrigger;
        if (memcmp(p, &previous.Gamepad, sizeof(*p))) {
            changes++;
            fprintf(report,"input buttons=%04x lx=%d ly=%d rx=%d ry=%d lt=%u rt=%u\n",
                p->wButtons,axes[0],axes[1],axes[2],axes[3],p->bLeftTrigger,p->bRightTrigger);
            fflush(report);
        }
        previous = state;
    }
    Sleep(16);
}
static void d3d11_test(void) {
    DXGI_SWAP_CHAIN_DESC d = {0};
    d.BufferDesc.Width=640; d.BufferDesc.Height=400; d.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    d.SampleDesc.Count=1; d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d.BufferCount=1; d.OutputWindow=window; d.Windowed=TRUE; d.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    ID3D11Device *dev=NULL; ID3D11DeviceContext *ctx=NULL; IDXGISwapChain *swap=NULL;
    ID3D11Texture2D *buffer=NULL; ID3D11RenderTargetView *target=NULL;
    D3D_FEATURE_LEVEL level=0;
    HRESULT hr=D3D11CreateDeviceAndSwapChain(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,0,NULL,0,
        D3D11_SDK_VERSION,&d,&swap,&dev,&level,&ctx);
    fprintf(report,"d3d11_create=0x%08lx feature_level=0x%x\n",(unsigned long)hr,level); fflush(report);
    if (SUCCEEDED(hr)) {
        hr=IDXGISwapChain_GetBuffer(swap,0,&IID_ID3D11Texture2D,(void**)&buffer);
        if (SUCCEEDED(hr)) hr=ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)buffer,NULL,&target);
        unsigned frames=0; DWORD begin=GetTickCount();
        SetWindowTextA(window,"Dimensity: Direct3D 11 (blue)");
        while (running && SUCCEEDED(hr) && GetTickCount()-begin<graphics_duration) {
            const float color[]={0.06f,0.3f,0.8f,1};
            ID3D11DeviceContext_ClearRenderTargetView(ctx,target,color);
            hr=IDXGISwapChain_Present(swap,1,0); if (SUCCEEDED(hr)) frames++;
            tick();
        }
        fprintf(report,"d3d11_present=0x%08lx frames=%u\n",(unsigned long)hr,frames);
    }
    if(target) ID3D11RenderTargetView_Release(target);
    if(buffer) ID3D11Texture2D_Release(buffer);
    if(swap) IDXGISwapChain_Release(swap);
    if(ctx) ID3D11DeviceContext_Release(ctx);
    if(dev) ID3D11Device_Release(dev);
    fflush(report);
}
static void d3d9_test(void) {
    IDirect3D9 *d3d=Direct3DCreate9(D3D_SDK_VERSION); IDirect3DDevice9 *dev=NULL;
    D3DPRESENT_PARAMETERS d={0};
    d.BackBufferWidth=640; d.BackBufferHeight=400; d.BackBufferFormat=D3DFMT_UNKNOWN;
    d.BackBufferCount=1; d.SwapEffect=D3DSWAPEFFECT_DISCARD; d.hDeviceWindow=window;
    d.Windowed=TRUE; d.PresentationInterval=D3DPRESENT_INTERVAL_ONE;
    HRESULT hr=d3d?IDirect3D9_CreateDevice(d3d,0,D3DDEVTYPE_HAL,window,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,&d,&dev):E_FAIL;
    fprintf(report,"d3d9_create=0x%08lx\n",(unsigned long)hr); fflush(report);
    if(SUCCEEDED(hr)) {
        unsigned frames=0; DWORD begin=GetTickCount();
        SetWindowTextA(window,"Dimensity: Direct3D 9 (green)");
        while(running && SUCCEEDED(hr) && GetTickCount()-begin<graphics_duration) {
            hr=IDirect3DDevice9_Clear(dev,0,NULL,D3DCLEAR_TARGET,D3DCOLOR_XRGB(20,160,70),1,0);
            if(SUCCEEDED(hr)) hr=IDirect3DDevice9_Present(dev,NULL,NULL,NULL,NULL);
            if(SUCCEEDED(hr)) frames++;
            tick();
        }
        fprintf(report,"d3d9_present=0x%08lx frames=%u\n",(unsigned long)hr,frames);
    }
    if(dev) IDirect3DDevice9_Release(dev);
    if(d3d) IDirect3D9_Release(d3d);
    fflush(report);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE prev,LPSTR command,int show) {
    (void)prev; (void)show;
    const int graphics_only = strstr(command, "--graphics") != NULL;
    focus_check = strstr(command, "--focus") != NULL;
    const int d3d9_only = strstr(command, "--d3d9") != NULL;
    const int d3d11_only = strstr(command, "--d3d11") != NULL;
    if (d3d9_only || d3d11_only) graphics_duration = 8000;
    CreateDirectoryA("C:\\DGSmoke",NULL);
    report=fopen(focus_check ? "C:/DGSmoke/focus-input.txt" : "C:/DGSmoke/result.txt","w"); if(!report) return 2;
    fprintf(report,"Dimensity bounded smoke fixture v1\n"); fflush(report);
    const char *names[]={"xinput1_3.dll","xinput1_4.dll","xinput9_1_0.dll"};
    HMODULE input=NULL;
    for(int i=0;i<3 && !get_state;i++) {
        input=LoadLibraryA(names[i]);
        if(input) get_state=(GetStateFn)(void*)GetProcAddress(input,"XInputGetState");
        if(get_state) fprintf(report,"xinput_dll=%s\n",names[i]);
        else if(input) FreeLibrary(input);
    }
    WNDCLASSA wc={0}; wc.lpfnWndProc=window_proc; wc.hInstance=instance;
    wc.lpszClassName="DGSmoke"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    RegisterClassA(&wc);
    window=CreateWindowA("DGSmoke","Dimensity smoke",WS_OVERLAPPEDWINDOW|WS_VISIBLE,50,50,680,460,NULL,NULL,instance,NULL);
    if(!window) { fprintf(report,"window_create_failed=%lu\n",GetLastError()); fclose(report); return 3; }
    ShowWindow(window,SW_SHOW); UpdateWindow(window);
    if (!focus_check && !d3d9_only) d3d11_test();
    if (!focus_check && running && !d3d11_only) d3d9_test();
    SetWindowTextA(window,"Dimensity: XInput - 35 second bounded check");
    DWORD begin=GetTickCount();
    while(running && !graphics_only && !d3d9_only && !d3d11_only && GetTickCount()-begin<35000) tick();
    fprintf(report,"xinput_connected_samples=%u changes=%u buttons_seen=0x%04x\n",connected_samples,changes,buttons_seen);
    fprintf(report,"axis_ranges lx=%d..%d ly=%d..%d rx=%d..%d ry=%d..%d triggers=%u,%u\n",
        axis_min[0],axis_max[0],axis_min[1],axis_max[1],axis_min[2],axis_max[2],axis_min[3],axis_max[3],trigger_max[0],trigger_max[1]);
    if (focus_check) fprintf(report,"focus_result=%d\n",focus_released ? 0 : 1);
    fprintf(report,"complete=1\n"); fclose(report);
    if(input) FreeLibrary(input);
    if(running) DestroyWindow(window);
    return 0;
}
