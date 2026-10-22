// Inspect only the child game we launch. Dismiss only its known informational Wine dialog.
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
static FILE *output;
static DWORD game_pid;
static HWND browser_window;
static HWND wine_dialog, game_window;
static BOOL CALLBACK close_wine_link(HWND window, LPARAM unused) {
    (void)unused;
    wchar_t title[128] = {0}, image[MAX_PATH] = {0};
    GetWindowTextW(window, title, 128);
    if (wcscmp(title, L"Wine Internet Explorer") != 0) return TRUE;
    DWORD pid, size = MAX_PATH; GetWindowThreadProcessId(window, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process) {
        if (QueryFullProcessImageNameW(process, 0, image, &size)) {
            const wchar_t *name = wcsrchr(image, L'\\');
            if (name && _wcsicmp(name + 1, L"iexplore.exe") == 0) {
                browser_window = window;
                fprintf(output, "close_wine_information_link=%d\n", PostMessageW(window, WM_CLOSE, 0, 0));
            }
        }
        CloseHandle(process);
    }
    return TRUE;
}
static BOOL CALLBACK cancel_browser_installer(HWND window, LPARAM unused) {
    (void)unused;
    wchar_t title[128]={0}; GetWindowTextW(window,title,128);
    DWORD pid=0,browser_pid=0;
    GetWindowThreadProcessId(window,&pid);
    if (browser_window) GetWindowThreadProcessId(browser_window,&browser_pid);
    if (browser_window && (GetWindow(window,GW_OWNER)==browser_window || pid==browser_pid) &&
        wcscmp(title,L"Wine Gecko Installer")==0)
        PostMessageW(window,WM_COMMAND,IDCANCEL,0);
    return TRUE;
}
static void text(const char *name, const wchar_t *value) {
    char utf8[4096];
    WideCharToMultiByte(CP_UTF8, 0, value, -1, utf8, sizeof(utf8), NULL, NULL);
    fprintf(output, "%s=%s\n", name, utf8);
}
static BOOL CALLBACK child(HWND window, LPARAM unused) {
    (void)unused;
    wchar_t title[1024] = {0}; GetWindowTextW(window, title, 1024);
    if (title[0]) text("child_text", title);
    return TRUE;
}
static BOOL CALLBACK enumerate(HWND window, LPARAM close_game) {
    DWORD pid; GetWindowThreadProcessId(window, &pid);
    if (pid != game_pid) return TRUE;
    wchar_t title[1024] = {0}, classname[256] = {0};
    GetWindowTextW(window, title, 1024); GetClassNameW(window, classname, 256);
    RECT rectangle = {0}; GetWindowRect(window, &rectangle);
    fprintf(output, "window=%p visible=%d enabled=%d owner=%p rect=%ld,%ld,%ld,%ld\n", window,
            IsWindowVisible(window), IsWindowEnabled(window), GetWindow(window, GW_OWNER),
            rectangle.left, rectangle.top, rectangle.right, rectangle.bottom);
    text("title", title); text("class", classname);
    EnumChildWindows(window, child, 0);
    if (wcscmp(title, L"Warzone 2100 under Wine") == 0 && wcscmp(classname, L"#32770") == 0)
        wine_dialog = window;
    if (IsWindowVisible(window) && wcsstr(title, L"Warzone") && wcscmp(classname, L"#32770") != 0) {
        game_window = window;
        if (close_game) PostMessageW(window, WM_CLOSE, 0, 0);
    }
    return TRUE;
}
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show) {
    (void)instance; (void)previous; (void)show;
    output = fopen("C:/DGSmoke/warzone-dialog.txt", "w");
    if (!output) return 2;
    char arguments[1024];
    snprintf(arguments,sizeof(arguments),"\"C:\\Games\\DGCompat\\Warzone\\bin\\warzone2100.exe\" --gfxbackend=%s --window --resolution=960x600 --configdir=C:/Games/DGCompat/Warzone/profile --debug=main --debugfile=C:/Games/DGCompat/Warzone/profile/logs/dimensity-dialog-run.log",
        strstr(command,"--directx") ? "directx" : "vulkan");
#ifdef DG_WARZONE_CAMPAIGN
    strcat(arguments, " --game=CAM_1A");
#endif
    STARTUPINFOA startup = {0}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION game = {0};
    if (!CreateProcessA(NULL, arguments, NULL, NULL, FALSE, 0, NULL,
                        "C:/Games/DGCompat/Warzone/bin", &startup, &game)) {
        fprintf(output, "launch_error=%lu\n", (unsigned long)GetLastError()); fclose(output); return 1;
    }
    game_pid = game.dwProcessId; CloseHandle(game.hThread);
    fprintf(output, "game_pid=%lu\n", (unsigned long)game_pid); fflush(output);
    for (UINT i = 0; i < 20 && !wine_dialog; i++) {
        Sleep(1000);
        fprintf(output, "snapshot_second=%u\n", i + 1);
        EnumWindows(enumerate, 0); fflush(output);
        if (WaitForSingleObject(game.hProcess, 0) == WAIT_OBJECT_0) {
            DWORD code; GetExitCodeProcess(game.hProcess, &code);
            fprintf(output, "early_exit_code=%lu complete=1\n", (unsigned long)code);
            fclose(output); CloseHandle(game.hProcess); return 1;
        }
    }
    fprintf(output, "known_wine_dialog_found=%d\n", wine_dialog != NULL); fflush(output);
    if (wine_dialog) {
        // Allow an application window snapshot while the native dialog is still open.
#ifdef DG_WARZONE_PLAY
        Sleep(300);
#else
        Sleep(10000);
#endif
        fprintf(output, "dismiss_posted=%d\n", PostMessageW(wine_dialog, WM_COMMAND, IDOK, 0)); fflush(output);
        Sleep(200);
        Sleep(1000);
        EnumWindows(close_wine_link, 0);
        if (game_window) {
            ShowWindow(game_window, SW_RESTORE);
            fprintf(output, "restore_foreground=%d\n", SetForegroundWindow(game_window)); fflush(output);
        }
    }
#ifdef DG_WARZONE_PLAY
    fprintf(output, "interactive_ready=1\n"); fflush(output);
    DWORD deadline = GetTickCount() + (strstr(command,"--long-play") ? 480000 : 180000);
    while ((LONG)(deadline - GetTickCount()) > 0 &&
            GetFileAttributesA("C:/DGSmoke/warzone-play.stop") == INVALID_FILE_ATTRIBUTES &&
            WaitForSingleObject(game.hProcess, 0) != WAIT_OBJECT_0) {
        Sleep(250);
        EnumWindows(close_wine_link,0);
        EnumWindows(cancel_browser_installer,0);
    }
#elif defined(DG_WARZONE_CAMPAIGN)
    Sleep(60000);
#else
#ifndef DG_WARZONE_PLAY
    Sleep(12000);
#endif
#endif
    fprintf(output, "after_dismiss=1\n"); EnumWindows(enumerate, 0); fflush(output);
    // Leave a brief interactive interval for a screenshot, then close our child normally.
    Sleep(12000);
    EnumWindows(enumerate, 1);
    DWORD wait = WaitForSingleObject(game.hProcess, 8000), code = STILL_ACTIVE;
    GetExitCodeProcess(game.hProcess, &code);
    fprintf(output, "wait=%lu exit_code=%lu complete=1\n", (unsigned long)wait, (unsigned long)code);
    fclose(output); CloseHandle(game.hProcess);
    return wait == WAIT_OBJECT_0 ? 0 : 1;
}
