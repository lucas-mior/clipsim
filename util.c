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

#if defined(__WIN32__)
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/wait.h>
#endif

#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ == 0
#define TESTING_util 1
#elif !defined(TESTING_util)
#define TESTING_util 0
#endif

#if !defined(SIZEKB)
#define SIZEKB(X) ((size_t)(X)*1024ul)
#define SIZEMB(X) ((size_t)(X)*1024ul*1024ul)
#define SIZEGB(X) ((size_t)(X)*1024ul*1024ul*1024ul)
#endif

#if !defined(LENGTH)
#define LENGTH(x) (isize)((sizeof(x) / sizeof(*x)))
#endif
#if !defined(SNPRINTF)
#define SNPRINTF(BUFFER, FORMAT, ...)                                          \
    snprintf2(BUFFER, sizeof(BUFFER), FORMAT, __VA_ARGS__)
#endif
#if !defined(STRING_FROM_STRINGS)
#define STRING_FROM_STRINGS(BUFFER, SEP, ARRAY, LENGTH)                        \
    string_from_strings(BUFFER, sizeof(BUFFER), SEP, ARRAY, LENGTH)
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

static void *xmmap_commit(size_t *);
static void xmunmap(void *, size_t);
static void *xcalloc(const size_t, const size_t);
static void *xmalloc(const size_t);
static void *xrealloc(void *, const size_t);
static void *util_memdup(const void *, const usize);
static char *xstrdup(char *);
static int32 snprintf2(char *, size_t, char *, ...);
static void error(char *, ...);
static void fatal(int) __attribute__((noreturn));
static void string_from_strings(char *, int32, char *, char **, int32);
static int32 util_copy_file(const char *, const char *);
static int32 util_string_int32(int32 *, const char *);
static int util_command(const int, char **);
static uint32 util_nthreads(void);
static void util_die_notify(const char *, ...) __attribute__((noreturn));
static void util_segv_handler(int32) __attribute__((noreturn));
static void send_signal(const char *, const int);
static char *itoa2(long, char *);
static long atoi2(char *);
static size_t util_page_size = 0;

#if defined(__WIN32__)
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

#if !defined(__WIN32__)
void *
xmmap_commit(size_t *size) {
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
            p = mmap(NULL, *size, PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE
                         | FLAGS_HUGE_PAGES,
                     -1, 0);
            if (p != MAP_FAILED) {
                *size = UTIL_ALIGN(*size, SIZEMB(2));
                break;
            }
        }
        p = mmap(NULL, *size, PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
        *size = UTIL_ALIGN(*size, util_page_size);
    } while (0);
    if (p == MAP_FAILED) {
        error("Error in mmap(%zu): %s.\n", *size, strerror(errno));
        fatal(EXIT_FAILURE);
    }
    return p;
}
void
xmunmap(void *p, size_t size) {
    if (munmap(p, size) < 0) {
        error("Error in munmap(%p, %zu): %s.\n", p, size, strerror(errno));
    }
    return;
}
#else
void *
xmmap_commit(size_t *size) {
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

    p = VirtualAlloc(NULL, *size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (p == NULL) {
        fprintf(stderr, "Error in VirtualAlloc(%zu): %lu.\n", *size,
                GetLastError());
        fatal(EXIT_FAILURE);
    }
    return p;
}
void
xmunmap(void *p, size_t size) {
    (void)size;
    if (!VirtualFree(p, 0, MEM_RELEASE)) {
        fprintf(stderr, "Error in VirtualFree(%p): %lu.\n", p, GetLastError());
    }
    return;
}
#endif

void *
xmalloc(const size_t size) {
    void *p;
    if ((p = malloc(size)) == NULL) {
        error("Failed to allocate %zu bytes.\n", size);
        fatal(EXIT_FAILURE);
    }
    return p;
}

void *
xrealloc(void *old, const size_t size) {
    void *p;
    if ((p = realloc(old, size)) == NULL) {
        error("Failed to reallocate %zu bytes from %p.\n", size, old);
        fatal(EXIT_FAILURE);
    }
    return p;
}

void *
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

#if defined(__WIN32__)
int
util_command(const int argc, char **argv) {
    char *cmdline;
    uint32 len = 1;

    if (argc == 0 || argv == NULL) {
        error("Invalid arguments.\n");
        fatal(EXIT_FAILURE);
    }

    for (int i = 0; i < argc - 1; i += 1) {
        len += strlen(argv[i]) + 3;
    }
    cmdline = xmalloc(len);

    cmdline[0] = '\0';
    for (int i = 0; i < (argc - 1); i += 1) {
        strcat(cmdline, "\"");
        strcat(cmdline, argv[i]);
        strcat(cmdline, "\"");
        strcat(cmdline, " ");
    }

    FILE *tty = freopen("CONIN$", "r", stdin);
    if (!tty) {
        error("Error reopening stdin: %s.\n", strerror(errno));
        free(cmdline);
        fatal(EXIT_FAILURE);
    }

    STARTUPINFO si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};

    BOOL success = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL,
                                  NULL, &si, &pi);

    if (!success) {
        error("Error running '%s", argv[0]);
        for (int i = 1; i < (argc - 1); i += 1) {
            error(" %s", argv[i]);
        }
        error("': %lu.\n", GetLastError());
        free(cmdline);
        fatal(EXIT_FAILURE);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    free(cmdline);
    return 0;
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
            fatal(EXIT_FAILURE);
        }
        return WEXITSTATUS(status);
    }
}
#endif

void
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
    int32 n;

    va_start(args, format);
    n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (n <= 0 || n >= (int32)sizeof(buffer)) {
        fprintf(stderr, "Error in vsnprintf()\n");
        fatal(EXIT_FAILURE);
    }

    buffer[n] = '\0';
    write(STDERR_FILENO, buffer, (size_t)n);
#if !defined(__WIN32__)
    fsync(STDERR_FILENO);
    fsync(STDOUT_FILENO);
#endif
    return;
}

void
fatal(int status) {
#if defined(DEBUGGING)
    (void)status;
    abort();
#else
    exit(status);
#endif
}

void
util_segv_handler(int32 unused) {
    char *message = "Memory error. Please send a bug report.\n";
    (void)unused;

    (void)write(STDERR_FILENO, message, strlen(message));
    for (uint i = 0; i < LENGTH(notifiers); i += 1) {
        execlp(notifiers[i], notifiers[i], "-u", "critical", "clipsim", message,
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
util_die_notify(const char *format, ...) {
    int32 n;
    va_list args;
    char buffer[BUFSIZ];

    va_start(args, format);
    n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (n < 0) {
        fatal(EXIT_FAILURE);
    }

    if (n >= (int32)sizeof(buffer)) {
        fatal(EXIT_FAILURE);
    }

    buffer[n] = '\0';
    (void)write(STDERR_FILENO, buffer, (usize)n + 1);
    for (uint i = 0; i < LENGTH(notifiers); i += 1) {
        execlp(notifiers[i], notifiers[i], "-u", "critical", "clipsim", buffer,
               NULL);
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

int32
util_copy_file(const char *destination, const char *source) {
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

#if defined(__linux__)
#include <dirent.h>
void
send_signal(const char *executable, const int32 signal_number) {
    DIR *processes;
    struct dirent *process;

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
            if (DEBUGGING) {
                error("Error: %s is not directory.\n", process->d_name);
            }
            continue;
        }
        if ((pid = atoi(process->d_name)) <= 0) {
            if (DEBUGGING) {
                error("Error: atoi(%s) <= 0.\n", process->d_name);
            }
            continue;
        }

        SNPRINTF(buffer, "/proc/%s/cmdline", process->d_name);

        if ((cmdline = open(buffer, O_RDONLY)) < 0) {
            if (errno != ENOENT || DEBUGGING) {
                error("Error opening %s: %s.\n", buffer, strerror(errno));
                continue;
            }
            if (errno != ENOENT) {
                fatal(EXIT_FAILURE);
            }
        }

        errno = 0;
        if ((r = read(cmdline, command, sizeof(command))) <= 0) {
            if (DEBUGGING) {
                error("Error reading from %s", buffer);
                if (r < 0) {
                    error(": %s", strerror(errno));
                }
                error(".\n");
            }
            (void)r;
            close(cmdline);
            continue;
        }
        if (!strcmp(command, executable)) {
            if (kill(pid, signal_number) < 0) {
                error("Error sending signal %d to program %s (pid %d): %s.\n",
                      signal_number, executable, pid, strerror(errno));
            } else {
                if (DEBUGGING) {
                    error("Sended signal %d to program %s (pid %d): %s.\n",
                          signal_number, executable, pid);
                }
            }
        }

        close(cmdline);
    }

    closedir(processes);
    return;
}
#else
#if !defined(__WIN32__)
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

    memset(p1, 0, SIZEMB(1));
    memcpy(p1, string, strlen(string));
    memset(p2, 0, SIZEMB(1));
    p3 = xstrdup(p1);

    error("%s == %s is working? %b\n", string, p3, !strcmp(string, p3));

    srand((uint)time(NULL));
    for (int i = 0; i < 10; i += 1) {
        int n = rand() - RAND_MAX / 2;
        assert(atoi2(itoa2(n, buffer)) == n);
    }

    free(p1);
    free(p2);
    free(p3);
    exit(EXIT_SUCCESS);
}

#endif

#endif
