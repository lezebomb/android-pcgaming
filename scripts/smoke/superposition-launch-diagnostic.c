// Observe the official free GUI launcher as our own child. No engine CLI or binary patch.
#include <windows.h>
#include <stdio.h>
#include <string.h>
static DWORD child_pid;
static FILE *record;
static BOOL CALLBACK windows(HWND window, LPARAM closing) {
    DWORD pid; GetWindowThreadProcessId(window, &pid);
    if (pid != child_pid || !IsWindowVisible(window)) return TRUE;
    char title[512] = {0}, name[128] = {0};
    GetWindowTextA(window, title, sizeof(title)); GetClassNameA(window, name, sizeof(name));
    fprintf(record, "window=%p enabled=%d title=%s class=%s\n", window, IsWindowEnabled(window), title, name);
    if (closing) PostMessageW(window, WM_CLOSE, 0, 0);
    return TRUE;
}
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show) {
    (void)instance; (void)previous; (void)command; (void)show;
    record = fopen("C:/DGSmoke/superposition-launch.txt", "w");
    if (!record) return 2;
    SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
    HANDLE transcript = CreateFileA("C:/DGSmoke/superposition-launch-stderr.txt", GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE null_input = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    STARTUPINFOA startup = {0}; startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = startup.hStdError = transcript; startup.hStdInput = null_input;
    PROCESS_INFORMATION child = {0};
    char arguments[] = "\"C:\\Games\\DGSuperposition\\bin\\launcher.exe\"";
    if (!CreateProcessA(NULL, arguments, NULL, NULL, TRUE, 0, NULL, "C:/Games/DGSuperposition", &startup, &child)) {
        fprintf(record, "launch_error=%lu complete=1\n", (unsigned long)GetLastError()); fclose(record); return 1;
    }
    CloseHandle(child.hThread); CloseHandle(transcript); CloseHandle(null_input);
    child_pid = child.dwProcessId;
    fprintf(record, "child_pid=%lu\n", (unsigned long)child_pid); fflush(record);
    DWORD wait = WAIT_TIMEOUT;
    UINT observeSeconds = strstr(command, "--observe-gui") ? 300 : 30;
    for (UINT second=0; second<observeSeconds; second++) {
        wait = WaitForSingleObject(child.hProcess, 1000);
        if (wait != WAIT_TIMEOUT) break;
        fprintf(record, "second=%u\n", second+1); EnumWindows(windows, 0); fflush(record);
    }
    if (wait == WAIT_TIMEOUT) {
        fprintf(record, "gui_observed_interval_complete=1\n"); EnumWindows(windows, 1);
        wait = WaitForSingleObject(child.hProcess, 8000);
    }
    DWORD code = STILL_ACTIVE; GetExitCodeProcess(child.hProcess, &code);
    fprintf(record, "wait=%lu exit_code=0x%08lx complete=1\n", (unsigned long)wait, (unsigned long)code);
    fclose(record); CloseHandle(child.hProcess);
    return wait == WAIT_OBJECT_0 && code == 0 ? 0 : 1;
}
