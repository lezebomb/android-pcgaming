#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
static int running=1, leftHeld=0;
static LRESULT CALLBACK proc(HWND w, UINT m, WPARAM a, LPARAM b) {
    if(m==WM_CLOSE) {running=0;DestroyWindow(w);return 0;}
    if(m==WM_LBUTTONDOWN) {leftHeld=1;SetCapture(w);return 0;}
    if(m==WM_LBUTTONUP) {leftHeld=0;ReleaseCapture();return 0;}
    return DefWindowProcA(w,m,a,b);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show) {
    (void)previous;(void)command;(void)show;
    FILE *out=fopen("C:/DGSmoke/mapped-disconnect.txt","w");
    if(!out)return 2;
    WNDCLASSA wc={0};wc.hInstance=instance;wc.lpfnWndProc=proc;wc.lpszClassName="DimensityMappedInput";
    wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.hCursor=LoadCursor(NULL,IDC_ARROW);RegisterClassA(&wc);
    HWND w=CreateWindowA(wc.lpszClassName,"Mapped controller disconnect",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        0,0,640,480,NULL,NULL,instance,NULL);
    ShowWindow(w,SW_SHOW);SetForegroundWindow(w);UpdateWindow(w);
    // Put the pointer inside the client, rather than the initial desktop corner.
    SetCursorPos(240,180);
    fprintf(out,"ready=1\n");fflush(out);
    DWORD start=GetTickCount();int stage=0;
    while(running&&GetTickCount()-start<45000&&stage<4) {
        MSG msg;while(PeekMessageA(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageA(&msg);}
        int held=leftHeld;
        if(stage==0&&held){stage=1;fprintf(out,"mapped_press=1\n");fflush(out);}
        else if(stage==1&&!held){stage=2;fprintf(out,"released_after_disconnect=1\n");fflush(out);}
        else if(stage==2&&held){stage=3;fprintf(out,"reconnected_press=1\n");fflush(out);}
        else if(stage==3&&!held){stage=4;fprintf(out,"reconnected_release=1\n");fflush(out);}
        Sleep(10);
    }
    fprintf(out,"mapped_disconnect_result=%d complete=1\n",stage==4?0:1);
    fclose(out);DestroyWindow(w);return stage==4?0:1;
}
