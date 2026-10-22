// Observe original game startup without patching its code, DLLs or API results.
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>

typedef bool (WINAPI *SteamInitFn)(void);
typedef void (WINAPI *SteamShutdownFn)(void);
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show) {
    (void)instance; (void)previous; (void)command; (void)show;
    CreateDirectoryA("C:\\DGSmoke", NULL);
    FILE *output = fopen("C:\\DGSmoke\\elden-launch.txt", "w");
    if (!output) return 2;
    const char *directory = "C:\\Steam\\steamapps\\common\\ELDEN RING\\Game";
    const char *executable = "C:\\Steam\\steamapps\\common\\ELDEN RING\\Game\\eldenring.exe";
    fprintf(output, "set_working_directory=%d\n", SetCurrentDirectoryA(directory));
    char app_id[64] = {0}; GetEnvironmentVariableA("SteamAppId", app_id, sizeof(app_id));
    fprintf(output, "steam_app_id=%s\n", app_id);
    HMODULE steam = LoadLibraryA("steam_api64.dll");
    fprintf(output, "load_original_steam_api=%d error=%lu\n", steam != NULL, steam ? 0 : (unsigned long)GetLastError());
    if (steam) {
        SteamInitFn init = (SteamInitFn)(void *)GetProcAddress(steam, "SteamAPI_Init");
        bool initialized = init ? init() : false;
        fprintf(output, "original_steam_api_init=%d\n", initialized);
        if (initialized) {
            SteamShutdownFn shutdown = (SteamShutdownFn)(void *)GetProcAddress(steam, "SteamAPI_Shutdown");
            if (shutdown) shutdown();
        }
        FreeLibrary(steam);
    }
    fflush(output);
    SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
    HANDLE stream = CreateFileA("C:\\DGSmoke\\elden-game-output.txt", GENERIC_WRITE, FILE_SHARE_READ,
        &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    STARTUPINFOA startup = {0}; startup.cb = sizeof(startup);
    if (stream != INVALID_HANDLE_VALUE) {
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = stream; startup.hStdError = stream;
    }
    PROCESS_INFORMATION game = {0};
    char arguments[] = "\"eldenring.exe\"";
    DWORD started = GetTickCount();
    if (!CreateProcessA(executable, arguments, NULL, NULL, TRUE, 0, NULL, directory, &startup, &game)) {
        fprintf(output, "create_process_error=%lu\n", (unsigned long)GetLastError());
        if (stream != INVALID_HANDLE_VALUE) CloseHandle(stream);
        fclose(output); return 1;
    }
    if (stream != INVALID_HANDLE_VALUE) CloseHandle(stream);
    CloseHandle(game.hThread);
    fprintf(output, "game_pid=%lu\n", (unsigned long)game.dwProcessId); fflush(output);
    WaitForSingleObject(game.hProcess, INFINITE);
    DWORD code = 0; GetExitCodeProcess(game.hProcess, &code);
    fprintf(output, "game_exit_code=0x%08lx elapsed_ms=%lu\n", (unsigned long)code, (unsigned long)(GetTickCount() - started));
    fprintf(output, "complete=1\n"); fflush(output); fclose(output);
    CloseHandle(game.hProcess);
    return code;
}
