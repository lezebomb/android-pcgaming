#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

static void line(FILE *out, const char *key, const wchar_t *value) {
    char utf8[32768];
    WideCharToMultiByte(CP_UTF8, 0, value, -1, utf8, sizeof(utf8), NULL, NULL);
    fprintf(out, "%s=%s\n", key, utf8);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR cmd, int show) {
    FILE *out = fopen("C:/DGSmoke/direct-launch.txt", "a");
    if (!out) return 2;
    wchar_t value[32768];
    fprintf(out, "launch_begin=1\n");
    GetModuleFileNameW(NULL, value, 32768);
    line(out, "executable", value);
    GetCurrentDirectoryW(32768, value);
    line(out, "working_directory", value);
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    fprintf(out, "argc=%d\n", argc);
    for (int i = 1; i < argc; i++) {
        char key[32];
        snprintf(key, sizeof(key), "arg_%d", i);
        line(out, key, argv[i]);
    }
    LocalFree(argv);
    fprintf(out, "launch_complete=1\n");
    fclose(out);
    HWND window = CreateWindowW(L"STATIC", L"Dimensity direct EXE launch", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                100, 100, 480, 200, NULL, NULL, instance, NULL);
    DWORD until = GetTickCount() + 2000;
    MSG message;
    while ((LONG) (until - GetTickCount()) > 0) {
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
        Sleep(20);
    }
    if (window) DestroyWindow(window);
    return 0;
}
