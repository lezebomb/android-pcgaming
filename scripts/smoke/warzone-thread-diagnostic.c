// Capture only a child game created by this tool; no root or Android debugger.
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <tlhelp32.h>
#include <dbghelp.h>
#include <stdio.h>

static unsigned long long ticks(FILETIME time) {
    return ((unsigned long long)time.dwHighDateTime << 32) | time.dwLowDateTime;
}
static void capture(FILE *output, HANDLE process, DWORD process_id, int pass) {
    fprintf(output, "\npass=%d game_pid=%lu\n", pass, (unsigned long)process_id);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 entry = {0}; entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry)) do {
        if (entry.th32OwnerProcessID != process_id) continue;
        HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
        if (!thread) continue;
        FILETIME created, exited, kernel, user;
        if (GetThreadTimes(thread, &created, &exited, &kernel, &user))
            fprintf(output, "thread=%lu cpu_ms=%llu\n", (unsigned long)entry.th32ThreadID, (ticks(kernel) + ticks(user)) / 10000);
        if (SuspendThread(thread) != (DWORD)-1) {
            CONTEXT context = {0}; context.ContextFlags = CONTEXT_FULL;
            if (GetThreadContext(thread, &context)) {
                STACKFRAME64 frame = {0};
                frame.AddrPC.Offset = context.Rip; frame.AddrPC.Mode = AddrModeFlat;
                frame.AddrFrame.Offset = context.Rbp; frame.AddrFrame.Mode = AddrModeFlat;
                frame.AddrStack.Offset = context.Rsp; frame.AddrStack.Mode = AddrModeFlat;
                for (unsigned index=0; index<16 && frame.AddrPC.Offset; index++) {
                    unsigned char storage[sizeof(SYMBOL_INFO) + 256] = {0};
                    SYMBOL_INFO *symbol = (SYMBOL_INFO *)storage;
                    symbol->SizeOfStruct = sizeof(*symbol); symbol->MaxNameLen = 255;
                    DWORD64 displacement = 0;
                    IMAGEHLP_MODULE64 module = {0}; module.SizeOfStruct = sizeof(module);
                    const char *module_name = SymGetModuleInfo64(process, frame.AddrPC.Offset, &module) ? module.ModuleName : "?";
                    if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
                        fprintf(output, "  %s!%s+0x%llx\n", module_name, symbol->Name, (unsigned long long)displacement);
                    else
                        fprintf(output, "  %s+0x%llx\n", module_name, (unsigned long long)(frame.AddrPC.Offset - module.BaseOfImage));
                    if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, NULL,
                        SymFunctionTableAccess64, SymGetModuleBase64, NULL)) break;
                }
            } else fprintf(output, "context_error=%lu\n", (unsigned long)GetLastError());
            ResumeThread(thread);
        }
        CloseHandle(thread);
    } while (Thread32Next(snapshot, &entry));
    CloseHandle(snapshot);
    fflush(output);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show) {
    (void)instance; (void)previous; (void)command; (void)show;
    CreateDirectoryA("C:\\DGSmoke", NULL);
    FILE *output = fopen("C:\\DGSmoke\\warzone-threads.txt", "w");
    if (!output) return 2;
    char arguments[] = "\"C:\\Games\\DGCompat\\Warzone\\bin\\warzone2100.exe\" --gfxbackend=vulkan --window --resolution=960x600 --configdir=C:/Games/DGCompat/Warzone/profile --debug=main --debugfile=C:/Games/DGCompat/Warzone/profile/logs/dimensity-thread-run.log";
    STARTUPINFOA startup = {0}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION game = {0};
    if (!CreateProcessA(NULL, arguments, NULL, NULL, FALSE, 0, NULL, "C:\\Games\\DGCompat\\Warzone\\bin", &startup, &game)) {
        fprintf(output, "launch_error=%lu\n", (unsigned long)GetLastError()); fclose(output); return 1;
    }
    CloseHandle(game.hThread);
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS);
    Sleep(7000);
    if (SymInitialize(game.hProcess, NULL, TRUE)) {
        capture(output, game.hProcess, game.dwProcessId, 1);
        Sleep(2000);
        capture(output, game.hProcess, game.dwProcessId, 2);
        SymCleanup(game.hProcess);
    } else fprintf(output, "symbols_error=%lu\n", (unsigned long)GetLastError());
    fprintf(output, "capture_complete=1\n"); fclose(output);
    // Keep child interactive; app's normal Exit controls its lifetime.
    WaitForSingleObject(game.hProcess, INFINITE);
    CloseHandle(game.hProcess);
    return 0;
}
