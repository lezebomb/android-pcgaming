// Observe the original benchmark launcher or engine through normal Win32 APIs.
#include <windows.h>
#include <stdio.h>
#include <string.h>

int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR arguments,int show) {
    (void)instance;(void)previous;(void)show;
    FILE *report=fopen("C:/DGSmoke/ffxiv-launch.txt","w");
    if(!report)return 2;
    STARTUPINFOA startup={.cb=sizeof(startup)};
    PROCESS_INFORMATION process={0};
    char command[16384];
    BOOL engine=strncmp(arguments,"--engine ",9)==0;
    if(snprintf(command,sizeof(command),"\"C:/Games/DGFFXIVBench/%s\" %s",
        engine?"game/ffxiv_dx11.exe":"ffxiv-dawntrail-bench.exe",engine?arguments+9:"") >= (int)sizeof(command)) {
        fclose(report);return 2;
    }
    BOOL started=CreateProcessA(NULL,command,NULL,NULL,FALSE,0,NULL,
        "C:/Games/DGFFXIVBench",&startup,&process);
    fprintf(report,"launcher_created=%d error=%lu pid=%lu\n",started,
        started?0:GetLastError(),process.dwProcessId);
    fflush(report);
    if(started) {
        DWORD wait=WaitForSingleObject(process.hProcess,60000),code=STILL_ACTIVE;
        GetExitCodeProcess(process.hProcess,&code);
        fprintf(report,"launcher_wait=%lu exit_code=%lu (0x%08lx)\n",wait,code,code);
        fflush(report);
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
    }
    fclose(report);
    // Keep the Wine session available briefly so its own logs can be collected.
    Sleep(15000);
    return started?0:1;
}
