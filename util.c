/*
 * Copyright (C) 2025 Mior, Lucas;
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(UTIL_C)
#define UTIL_C

#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <libgen.h>
#include <pthread.h>
#include <limits.h>
#include <sys/stat.h>

#if defined(__linux__)
#define OS_LINUX 1
#define OS_MAC 0
#define OS_BSD 0
#define OS_WINDOWS 0
#elif defined(__APPLE__) && defined(__MACH__)
#define OS_LINUX 0
#define OS_MAC 1
#define OS_BSD 0
#define OS_WINDOWS 0
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define OS_LINUX 0
#define OS_MAC 0
#define OS_BSD 1
#define OS_WINDOWS 0
#elif defined(_WIN32) || defined(_WIN64)
#define OS_LINUX 0
#define OS_MAC 0
#define OS_BSD 0
#define OS_WINDOWS 1
#else
#error "Unsupported OS.\n"
#endif

#define OS_UNIX (OS_LINUX || OS_MAC || OS_BSD)

#if OS_WINDOWS
#include <windows.h>
#endif

#if OS_UNIX
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#endif

#ifndef DEBUGGING
#define DEBUGGING 0
#endif

#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ == 0
#define TESTING_util 1
#elif !defined(TESTING_util)
#define TESTING_util 0
#endif

#if TESTING_util
static char *program = __FILE__;
#else
static char *program;
#endif

#define SIZEOF(X) (int64)sizeof(X)

#if !defined(SIZEKB)
#define SIZEKB(X) ((int64)(X)*1024l)
#define SIZEMB(X) ((int64)(X)*1024l*1024l)
#define SIZEGB(X) ((int64)(X)*1024l*1024l*1024l)
#endif

#if !defined(LENGTH)
#define LENGTH(x) (int64)((sizeof(x) / sizeof(*x)))
#endif
#if !defined(SNPRINTF)
#define SNPRINTF(BUFFER, FORMAT, ...)                                          \
    snprintf2(BUFFER, sizeof(BUFFER), FORMAT, __VA_ARGS__)
#endif
#if !defined(STRING_FROM_STRINGS)
#define STRING_FROM_STRINGS(BUFFER, SEP, ARRAY, LENGTH)                        \
    string_from_strings(BUFFER, sizeof(BUFFER), SEP, ARRAY, LENGTH)
#endif

#if DEBUGGING || TESTING_util
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wc11-extensions"
#pragma clang diagnostic ignored "-Wformat"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#endif

#define PRINT_VAR_EVAL(FORMAT, variable)                                       \
    printf("%s = " FORMAT "\n", #variable, variable)

#define PRINT_VAR(variable)                                                    \
    _Generic((variable),                                                       \
        bool: PRINT_VAR_EVAL("%b", variable),                                  \
        char: PRINT_VAR_EVAL("%c", variable),                                  \
        char *: PRINT_VAR_EVAL("%s", variable),                                \
        float: PRINT_VAR_EVAL("%f", variable),                                 \
        double: PRINT_VAR_EVAL("%f", variable),                                \
        long double: PRINT_VAR_EVAL("%Lf", variable),                          \
        int8: PRINT_VAR_EVAL("%d", variable),                                  \
        int16: PRINT_VAR_EVAL("%d", variable),                                 \
        int32: PRINT_VAR_EVAL("%d", variable),                                 \
        int64: PRINT_VAR_EVAL("%lld", (long long)variable),                                \
        uint8: PRINT_VAR_EVAL("%u", variable),                                 \
        uint16: PRINT_VAR_EVAL("%u", variable),                                \
        uint32: PRINT_VAR_EVAL("%u", variable),                                \
        uint64: PRINT_VAR_EVAL("%lu", variable),                               \
        void *: PRINT_VAR_EVAL("%p", variable),                                \
        default: printf("%s = ?\n", #variable))

#endif

#if !defined(DEBUGGING)
#define DEBUGGING 0
#endif

#if !defined(FLAGS_HUGE_PAGES)
#if defined(MAP_HUGETLB) && defined(MAP_HUGE_2MB)
#define FLAGS_HUGE_PAGES MAP_HUGETLB | MAP_HUGE_2MB
#else
#define FLAGS_HUGE_PAGES 0
#endif
#endif

#if !defined(MAP_POPULATE)
#define MAP_POPULATE 0
#endif

#define UTIL_ALIGN(S, A) (((S) + ((A) - 1)) & ~((A) - 1))

#if !defined(ALIGNMENT)
#define ALIGNMENT 16ul
#endif
#if !defined(ALIGN)
#define ALIGN(x) UTIL_ALIGN(x, ALIGNMENT)
#endif

#if !defined(INTEGERS)
#define INTEGERS
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long ulonglong;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef size_t usize;
typedef ssize_t isize;
#endif

static char *notifiers[2] = {"dunstify", "notify-send"};

static void *util_memdup(const void *, const usize);
static char *xstrdup(char *);
static int32 snprintf2(char *, size_t, char *, ...);
static void error(char *, ...);
static void fatal(int) __attribute__((noreturn));
static int32 util_string_int32(int32 *, const char *);
static int util_command(const int, char **);
static uint32 util_nthreads(void);
static void util_die_notify(char *, const char *, ...)
    __attribute__((noreturn));
static void util_segv_handler(int32) __attribute__((noreturn));
static void send_signal(const char *, const int);
static char *itoa2(long, char *);
static long atoi2(char *);
static size_t util_page_size = 0;

#if OS_WINDOWS
static void *
memmem(void *haystack, size_t hay_len, void *needle, size_t needle_len) {
    uchar *h = haystack;
    uchar *n = needle;
    uchar *end = h + hay_len;
    uchar *limit = end - needle_len + 1;

    if (needle_len == 0) {
        return haystack;
    }
    if ((haystack == NULL) || (needle == NULL)) {
        return NULL;
    }
    if (hay_len < needle_len) {
        return NULL;
    }

    while (h < limit) {
        uchar *p;

        if ((p = memchr(h, n[0], (size_t)(limit - h))) == NULL) {
            return NULL;
        }

        if (memcmp(p, n, needle_len) == 0) {
            return (void *)p;
        }
        h = p + 1;
    }

    return NULL;
}
#endif

#if OS_WINDOWS
uint32
util_nthreads(void) {
    SYSTEM_INFO sysinfo;
    memset(&sysinfo, 0, sizeof(sysinfo));
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
}
#else
uint32
util_nthreads(void) {
    return (uint32)sysconf(_SC_NPROCESSORS_ONLN);
}
#endif

#if OS_WINDOWS || OS_MAC
#define basename basename2
#endif

static char *
basename2(char *path) {
    int64 left = (int64)strlen(path);
    char *fslash = NULL;
    char *bslash = NULL;
    char *p = path;

    while (left > 0) {
        int64 length;

        fslash = memchr(p, '/', (usize)left);
        if (OS_WINDOWS) {
            bslash = memchr(p, '\\', (usize)left);
        }

        if ((fslash == NULL) && (bslash == NULL)) {
            return p;
        }
        if (fslash > bslash) {
            length = fslash - p + 1;
            p = fslash + 1;
        } else {
            length = bslash - p + 1;
            p = bslash + 1;
        }

        left -= length;
    }
    return path;
}

#if OS_UNIX
static void *
xmmap_commit(int64 *size) {
    void *p;

    if (util_page_size == 0) {
        long aux;
        if ((aux = sysconf(_SC_PAGESIZE)) <= 0) {
            fprintf(stderr, "Error getting page size: %s.\n", strerror(errno));
            fatal(EXIT_FAILURE);
        }
        util_page_size = (size_t)aux;
    }

    do {
        if ((*size >= SIZEMB(2)) && FLAGS_HUGE_PAGES) {
            p = mmap(NULL, (size_t)*size, PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE
                         | FLAGS_HUGE_PAGES,
                     -1, 0);
            if (p != MAP_FAILED) {
                *size = (int64)UTIL_ALIGN((size_t)size, (size_t)SIZEMB(2));
                break;
            }
        }
        p = mmap(NULL, (size_t)*size, PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
        *size = (int64)UTIL_ALIGN((size_t)*size, util_page_size);
    } while (0);
    if (p == MAP_FAILED) {
        error("Error in mmap(%zu): %s.\n", *size, strerror(errno));
        fatal(EXIT_FAILURE);
    }
    return p;
}
static void
xmunmap(void *p, int64 size) {
    if (munmap(p, (size_t)size) < 0) {
        error("Error in munmap(%p, %lld): %s.\n", p, size, strerror(errno));
    }
    return;
}
#else
static void *
xmmap_commit(int64 *size) {
    void *p;

    if (util_page_size == 0) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        util_page_size = si.dwPageSize;
        if (util_page_size <= 0) {
            fprintf(stderr, "Error getting page size.\n");
            fatal(EXIT_FAILURE);
        }
    }

    p = VirtualAlloc(NULL, (size_t)*size, MEM_COMMIT | MEM_RESERVE,
                     PAGE_READWRITE);
    if (p == NULL) {
        fprintf(stderr, "Error in VirtualAlloc(%lld): %lu.\n", (long long)*size,
                GetLastError());
        fatal(EXIT_FAILURE);
    }
    return p;
}
static void
xmunmap(void *p, size_t size) {
    (void)size;
    if (!VirtualFree(p, 0, MEM_RELEASE)) {
        fprintf(stderr, "Error in VirtualFree(%p): %lu.\n", p, GetLastError());
    }
    return;
}
#endif

static void *
xmalloc(int64 size) {
    void *p;
    if (size <= 0) {
        error("Error in xmalloc: invalid size = %ld.\n", size);
        fatal(EXIT_FAILURE);
    }
    if ((p = malloc((size_t)size)) == NULL) {
        error("Failed to allocate %lld bytes.\n", (long long)size);
        fatal(EXIT_FAILURE);
    }
    return p;
}

static void *
xrealloc(void *old, const int64 size) {
    void *p;
    uint64 old_save = (uint64)old;
    if (size <= 0) {
        error("Error in xmalloc: invalid size = %lld.\n", (long long)size);
        fatal(EXIT_FAILURE);
    }
    if ((p = realloc(old, (usize)size)) == NULL) {
        error("Failed to reallocate %zu bytes from %x.\n", size, old_save);
        fatal(EXIT_FAILURE);
    }
    return p;
}

static void *
xcalloc(const size_t nmemb, const size_t size) {
    void *p;
    if ((p = calloc(nmemb, size)) == NULL) {
        error("Error allocating %zu members of %zu bytes each.\n", nmemb, size);
        fatal(EXIT_FAILURE);
    }
    return p;
}

char *
xstrdup(char *string) {
    char *p;
    size_t length;

    length = strlen(string) + 1;
    if ((p = malloc(length)) == NULL) {
        error("Error allocating %zu bytes to duplicate '%s': %s\n", length,
              string, strerror(errno));
        fatal(EXIT_FAILURE);
    }

    memcpy(p, string, length);
    return p;
}

static void
xpthread_mutex_lock(pthread_mutex_t *mutex) {
    int err;
    if ((err = pthread_mutex_lock(mutex))) {
        error("Error locking mutex %p: %s.\n", mutex, strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

static void
xpthread_mutex_unlock(pthread_mutex_t *mutex) {
    int err;
    if ((err = pthread_mutex_unlock(mutex))) {
        error("Error unlocking mutex %p: %s.\n", mutex, strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

static void
xpthread_cond_destroy(pthread_cond_t *cond) {
    int err;
    if ((err = pthread_cond_destroy(cond))) {
        error("Error destroying cond %p: %s.\n", cond, strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

static void
xpthread_mutex_destroy(pthread_mutex_t *mutex) {
    int err;
    if ((err = pthread_mutex_destroy(mutex))) {
        error("Error destroying mutex %p: %s.\n", mutex, strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

int32
snprintf2(char *buffer, size_t size, char *format, ...) {
    int n;
    va_list args;

    va_start(args, format);
    n = vsnprintf(buffer, size, format, args);
    va_end(args);

    if (n <= 0) {
        error("Error in snprintf.\n");
        fatal(EXIT_FAILURE);
    }
    if (n >= (int)size) {
        error("Error in snprintf: Buffer is too small.\n");
        fatal(EXIT_FAILURE);
    }
    return n;
}

#if OS_WINDOWS
int
util_command(const int argc, char **argv) {
    char cmdline[1024] = {0};
    int64 j = 0;
    FILE *tty;
    PROCESS_INFORMATION proc_info = {0};
    DWORD exit_code = 0;
    int64 len = (int64)strlen(argv[0]);
    char *argv0_windows;
    char *exe = ".exe";
    int64 exe_len = (int64)(strlen(exe));

    if (argc == 0 || argv == NULL) {
        error("Invalid arguments.\n");
        fatal(EXIT_FAILURE);
    }

    if (memmem(argv[0], (size_t)len + 1, exe, (size_t)exe_len + 1) == NULL) {
        argv0_windows = xmalloc(len + exe_len + 1);
        memcpy(argv0_windows, argv[0], (size_t)len);
        memcpy(argv0_windows + len, exe, (size_t)exe_len + 1);
        argv[0] = argv0_windows;
    }

    for (int i = 0; i < argc - 1; i += 1) {
        int64 len2 = (int64)strlen(argv[i]);
        if ((j + len2) >= (int64)sizeof(cmdline)) {
            error("Command line is too long.\n");
            fatal(EXIT_FAILURE);
        }

        cmdline[j] = '"';
        memcpy(&cmdline[j + 1], argv[i], (size_t)len2);
        cmdline[j + len2 + 1] = '"';
        cmdline[j + len2 + 2] = ' ';
        j += len2 + 3;
    }
    cmdline[j - 1] = '\0';

    if ((tty = freopen("CONIN$", "r", stdin)) == NULL) {
        error("Error reopening stdin: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    }

    {
        BOOL success;
        STARTUPINFO startup_info = {0};
        startup_info.cb = sizeof(startup_info);
        success = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL,
                                 &startup_info, &proc_info);
        if (!success) {
            DWORD err = GetLastError();
            error("Error running '%s': %d.\n", cmdline, err);
            if (err == ERROR_PATH_NOT_FOUND) {
                error("Path not found.\n");
            }
            return -1;
        }
    }

    if (WaitForSingleObject(proc_info.hProcess, INFINITE) != WAIT_OBJECT_0) {
        CloseHandle(proc_info.hThread);
        CloseHandle(proc_info.hProcess);
        return -1;
    }

    if (!GetExitCodeProcess(proc_info.hProcess, &exit_code)) {
        CloseHandle(proc_info.hThread);
        CloseHandle(proc_info.hProcess);
        return -1;
    }

    if (!CloseHandle(proc_info.hThread)) {
        CloseHandle(proc_info.hProcess);
        return -1;
    }

    if (!CloseHandle(proc_info.hProcess)) {
        return -1;
    }

    return (int)exit_code;
}
#else
int
util_command(const int argc, char **argv) {
    pid_t child;
    int status;

    switch (child = fork()) {
    case 0:
        if (!freopen("/dev/tty", "r", stdin)) {
            error("Error reopening stdin: %s.\n", strerror(errno));
        }
        execvp(argv[0], argv);
        error("Error running '%s", argv[0]);
        for (int i = 1; i < argc; i += 1) {
            error(" %s", argv[i]);
        }
        error("': %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    case -1:
        error("Error forking: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    default:
        if (waitpid(child, &status, 0) < 0) {
            error("Error waiting for the forked child: %s.\n", strerror(errno));
            fatal(EXIT_FAILURE);
        }
        if (!WIFEXITED(status)) {
            error("Command exited abnormally.\n");
            return -1;
        }
        return WEXITSTATUS(status);
    }
}
#endif

static void
string_from_strings(char *buffer, int32 size, char *sep, char **array,
                    int32 array_length) {
    int32 n = 0;

    for (int32 i = 0; i < (array_length - 1); i += 1) {
        int32 space = size - n;
        int32 m = snprintf(buffer + n, (size_t)space, "%s%s", array[i], sep);
        if (m <= 0) {
            error("Error in snprintf().\n");
            fatal(EXIT_FAILURE);
        }
        if (m >= space) {
            error("Error printing array, not enough space.\n");
            fatal(EXIT_FAILURE);
        }
        n += m;
    }
    {
        int32 i = array_length - 1;
        int32 space = size - n;
        int32 m = snprintf(buffer + n, (size_t)space, "%s", array[i]);
        if (m <= 0) {
            error("Error in snprintf().\n");
            fatal(EXIT_FAILURE);
        }
        if (m >= space) {
            error("Error printing array, not enough space.\n");
            fatal(EXIT_FAILURE);
        }
    }
    return;
}

void
error(char *format, ...) {
    char buffer[BUFSIZ];
    va_list args;
    int64 n;

    va_start(args, format);
    n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (n <= 0 || n >= (int32)sizeof(buffer)) {
        fprintf(stderr, "Error in vsnprintf()\n");
        fatal(EXIT_FAILURE);
    }

    buffer[n] = '\0';
    write(STDERR_FILENO, buffer, (uint32)n);
#if OS_UNIX
    fsync(STDERR_FILENO);
    fsync(STDOUT_FILENO);
#endif
    return;
}

void
fatal(int status) {
    if (DEBUGGING) {
        (void)status;
        abort();
    } else {
        exit(status);
    }
}

void
util_segv_handler(int32 unused) {
    char *message = "Memory error. Please send a bug report.\n";
    (void)unused;

    write(STDERR_FILENO, message, (uint32)strlen(message));
    for (uint32 i = 0; i < LENGTH(notifiers); i += 1) {
        execlp(notifiers[i], notifiers[i], "-u", "critical", program, message,
               NULL);
    }
    _exit(EXIT_FAILURE);
}

int32
util_string_int32(int32 *number, const char *string) {
    char *endptr;
    long x;
    errno = 0;
    x = strtol(string, &endptr, 10);
    if ((errno != 0) || (string == endptr) || (*endptr != 0)) {
        return -1;
    } else if ((x > INT32_MAX) || (x < INT32_MIN)) {
        return -1;
    } else {
        *number = (int32)x;
        return 0;
    }
}

void
util_die_notify(char *program_name, const char *format, ...) {
    int32 n;
    va_list args;
    char buffer[BUFSIZ];

    va_start(args, format);
    n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (n < 0) {
        fatal(EXIT_FAILURE);
    }

    if (n >= SIZEOF(buffer)) {
        fatal(EXIT_FAILURE);
    }

    buffer[n] = '\0';
    write(STDERR_FILENO, buffer, (uint32)n + 1);
    for (uint32 i = 0; i < LENGTH(notifiers); i += 1) {
        execlp(notifiers[i], notifiers[i], "-u", "critical", program_name,
               buffer, NULL);
    }
    fatal(EXIT_FAILURE);
}

void *
util_memdup(const void *source, const usize size) {
    void *p;
    if ((p = malloc(size)) == NULL) {
        error("Error allocating %zu bytes.\n", size);
        fatal(EXIT_FAILURE);
    }
    memcpy(p, source, size);
    return p;
}

#if OS_UNIX
static int32
util_copy_file_sync(const char *destination, const char *source) {
    int32 source_fd;
    int32 destination_fd;
    char buffer[BUFSIZ];
    isize r = 0;
    isize w = 0;

    if ((source_fd = open(source, O_RDONLY)) < 0) {
        error("Error opening %s for reading: %s.\n", source, strerror(errno));
        return -1;
    }

    if ((destination_fd
         = open(destination, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR))
        < 0) {
        error("Error opening %s for writing: %s.\n", destination,
              strerror(errno));
        close(source_fd);
        return -1;
    }

    errno = 0;
    while ((r = read(source_fd, buffer, BUFSIZ)) > 0) {
        w = write(destination_fd, buffer, (usize)r);
        if (w != r) {
            fprintf(stderr, "Error writing data to %s", destination);
            if (errno) {
                fprintf(stderr, ": %s", strerror(errno));
            }
            fprintf(stderr, ".\n");

            close(source_fd);
            close(destination_fd);
            return -1;
        }
    }

    if (r < 0) {
        error("Error reading data from %s: %s.\n", source, strerror(errno));
        close(source_fd);
        close(destination_fd);
        return -1;
    }

    close(source_fd);
    close(destination_fd);
    return 0;
}

static int32
util_copy_file_async(const char *destination, const char *source,
                     int *dest_fd) {
    int32 source_fd;

    if ((source_fd = open(source, O_RDONLY)) < 0) {
        error("Error opening %s for reading: %s.\n", source, strerror(errno));
        return -1;
    }

    if ((*dest_fd
         = open(destination, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR))
        < 0) {
        error("Error opening %s for writing: %s.\n", destination,
              strerror(errno));
        close(source_fd);
        return -1;
    }

    return source_fd;
}
#endif

#if OS_LINUX
#include <dirent.h>
void
send_signal(const char *executable, const int32 signal_number) {
    DIR *processes;
    struct dirent *process;
    int64 len = (int64)strlen(executable);

    if ((processes = opendir("/proc")) == NULL) {
        error("Error opening /proc: %s\n", strerror(errno));
        return;
    }

    while ((process = readdir(processes))) {
        char buffer[256];
        char command[256];
        int32 pid;
        int32 cmdline;
        ssize_t r;

        if (process->d_type != DT_DIR) {
            continue;
        }
        if ((pid = atoi(process->d_name)) <= 0) {
            continue;
        }

        SNPRINTF(buffer, "/proc/%s/cmdline", process->d_name);

        if ((cmdline = open(buffer, O_RDONLY)) < 0) {
            continue;
        }

        errno = 0;
        if ((r = read(cmdline, command, sizeof(command))) <= 0) {
            (void)r;
            close(cmdline);
            continue;
        }
        if (memmem(command, (size_t)r, executable, (size_t)len)) {
            if (kill(pid, signal_number) < 0) {
                error("Error sending signal %d to program %s (pid %d): %s.\n",
                      signal_number, executable, pid, strerror(errno));
            } else {
                if (DEBUGGING) {
                    error("Sended signal %d to program %s (pid %d).\n",
                          signal_number, executable, pid);
                }
            }
        }

        close(cmdline);
    }

    closedir(processes);
    return;
}
#elif OS_UNIX
void
send_signal(const char *executable, const int32 signal_number) {
    char signal_string[14];
    SNPRINTF(signal_string, "%d", signal_number);

    switch (fork()) {
    case -1:
        error("Error forking: %s\n", strerror(errno));
        return;
    case 0:
        execlp("pkill", "pkill", signal_string, executable, NULL);
        error("Error executing pkill: %s\n", strerror(errno));
        fatal(EXIT_FAILURE);
    default:
        wait(NULL);
    }
    return;
}
#else
void
send_signal(const char *executable, const int32 signal_number) {
    (void)executable;
    (void)signal_number;
    return;
}
#endif

char *
itoa2(long num, char *str) {
    int i = 0;
    bool negative = false;

    if (num < 0) {
        negative = true;
        num = -num;
    }

    do {
        str[i] = num % 10 + '0';
        i += 1;
        num /= 10;
    } while (num > 0);

    if (negative) {
        str[i] = '-';
        i += 1;
    }

    str[i] = '\0';

    for (int j = 0; j < i / 2; j += 1) {
        char temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }
    return str;
}

long
atoi2(char *str) {
    return atoi(str);
}

#if TESTING_util
#include <assert.h>

int
main(void) {
    char buffer[32];
    void *p1 = xmalloc(SIZEMB(1));
    void *p2 = xcalloc(10, SIZEMB(1));
    char *p3;
    char *string = __FILE__;

    bool var_bool = true;
    char var_char = 'c';
    char *var_string = "a nice string";
    void *var_voidptr = NULL;
    float var_float = 0.5f;
    double var_double = 0.5;
    long double var_longdouble = 0.5L;
    int8 var_int8 = INT8_MAX;
    int16 var_int16 = INT16_MAX;
    int32 var_int32 = INT32_MAX;
    int64 var_int64 = INT64_MAX;
    uint8 var_uint8 = UINT8_MAX;
    uint16 var_uint16 = UINT16_MAX;
    uint32 var_uint32 = UINT32_MAX;
    uint64 var_uint64 = UINT64_MAX;

    char *paths[] = {
        "/aaaa/bbbb/cccc", "/aa/bb/cc", "/a/b/c",    "a/b/c",
        "a/b/cccc",        "a/bb/cccc", "aaaa/cccc",
    };
    char *bases[] = {
        "cccc", "cc", "c", "c", "cccc", "cccc", "cccc",
    };

#if defined(__clang__)
    PRINT_VAR(var_bool);
    PRINT_VAR(var_char);
    PRINT_VAR(var_string);
    PRINT_VAR(var_voidptr);
    PRINT_VAR(var_float);
    PRINT_VAR(var_double);
    PRINT_VAR(var_longdouble);
    PRINT_VAR(var_int8);
    PRINT_VAR(var_int16);
    PRINT_VAR(var_int32);
    PRINT_VAR(var_int64);
    PRINT_VAR(var_uint8);
    PRINT_VAR(var_uint16);
    PRINT_VAR(var_uint32);
    PRINT_VAR(var_uint64);
#endif

    memset(p1, 0, SIZEMB(1));
    memcpy(p1, string, strlen(string));
    memset(p2, 0, SIZEMB(1));
    p3 = xstrdup(p1);

    assert(!strcmp(string, p3));

    srand((uint)time(NULL));
    for (int i = 0; i < 10; i += 1) {
        int n = rand() - RAND_MAX / 2;
        assert(atoi2(itoa2(n, buffer)) == n);
    }

    for (int64 i = 0; i < LENGTH(paths); i += 1) {
        char *path = paths[i];
        assert(!strcmp(basename2(path), bases[i]));
    }

    if (OS_WINDOWS) {
        char *path2 = "aa\\cc";
        assert(!strcmp(basename2(path2), "cc"));
    }

    free(p1);
    free(p2);
    free(p3);
    exit(EXIT_SUCCESS);
}

#endif

#endif
