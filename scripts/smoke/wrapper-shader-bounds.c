#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* One captured shader, original versus bounds-fixed wrapper. No Vulkan device.
 * Symbol VA is valid ONLY for the SHA-256-pinned wrapper in the caller script. */
static int replay(const char *library, const char *input) {
    void *module = dlopen(library, RTLD_NOW | RTLD_LOCAL);
    if (!module) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 2; }
    Dl_info location;
    if (!dladdr(dlsym(module, "vk_icdGetInstanceProcAddr"), &location)) return 3;
    void (*patch)(uint32_t *, uint32_t) =
        (void (*)(uint32_t *, uint32_t))((char *)location.dli_fbase + 0x111adc);
    int fd = open(input, O_RDONLY);
    struct stat info;
    if (fd < 0 || fstat(fd, &info) || info.st_size != 104736) return 4;
    const size_t size = (size_t)info.st_size, page = (size_t)sysconf(_SC_PAGESIZE);
    const size_t rounded = (size + page - 1) & ~(page - 1);
    char *region = mmap(NULL, rounded + page, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED || mprotect(region + rounded, page, PROT_NONE)) return 5;
    uint32_t *code = (uint32_t *)(region + rounded - size);
    size_t offset = 0;
    while (offset < size) {
        ssize_t count = read(fd, (char *)code + offset, size - offset);
        if (count <= 0) return 6;
        offset += (size_t)count;
    }
    close(fd);
    printf("replay_bytes=%zu words=%zu guard_at_end=1\n", size, size / 4);
    fflush(stdout);
    patch(code, (uint32_t)size);
    /* The captured input needs no transformation; preserve every byte. */
    fd = open(input, O_RDONLY);
    char buffer[4096];
    offset = 0;
    while (offset < size) {
        ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count <= 0 || memcmp((char *)code + offset, buffer, (size_t)count)) return 7;
        offset += (size_t)count;
    }
    close(fd);
    puts("returned=1 input_unchanged=1");
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4) return 64;
    setbuf(stdout, NULL);
    int result = 0;
    for (int i = 1; i <= 2; i++) {
        pid_t child = fork();
        if (child == 0) _exit(replay(argv[i], argv[3]));
        if (child < 0) return 65;
        int status;
        if (waitpid(child, &status, 0) != child) return 66;
        if (i == 1) {
            int expected = WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV;
            printf("original_guard_sigsegv=%d\n", expected);
            if (!expected) result = 1;
        } else {
            int passed = WIFEXITED(status) && WEXITSTATUS(status) == 0;
            printf("fixed_guard_returned=%d\n", passed);
            if (!passed) result = 1;
        }
    }
    printf("bounds_result=%d\n", result);
    return result;
}
