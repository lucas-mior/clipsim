/*
 * Copyright (C) 2025 Mior, Lucas;
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the*License,
 * or (at your option) any later version.
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
#include <float.h>

#include "platform_detection.h"

#include "generic.c"
#include "minmax.c"
#include "base_macros.h"
#include "assert.c"
#include "memory.c"

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_util 1
#elif !defined(TESTING_util)
#define TESTING_util 0
#endif

static int32 snprintf2(char *buffer, int64 size, char *format, ...);
static void __attribute__((format(printf, 3, 4)))
    error_impl(char *file, int32 line, char *format, ...);
#define error(...) error_impl(__FILE__, __LINE__, __VA_ARGS__)

#if !TESTING_util
static char *program;
#else
static char *program = __FILE__;
#endif
static int32 program_len __attribute__((unused));

static bool timezone_initialized = false;
static time_t timezone_offset = 0;

#if !defined(SNPRINTF)
#define SNPRINTF(BUFFER, FORMAT, ...) \
    snprintf2(BUFFER, sizeof(BUFFER), FORMAT, __VA_ARGS__)
#endif
#if !defined(STRFTIME)
#define STRFTIME(BUFFER, FORMAT, TIME) \
    strftime2(BUFFER, sizeof(BUFFER), FORMAT, TIME)
#endif

#define STRUCT_ARRAY_SIZE(struct_object, ArrayType, array_length) \
    (int64)(SIZEOF(*(struct_object)) + ((array_length)*SIZEOF(ArrayType)))

#define STRING_FROM_ARRAY(BUFFER, SEP, ARRAY, LENGTH) \
_Generic((ARRAY), \
    double *: string_from_doubles, \
    char **: string_from_strings \
)(BUFFER, sizeof(BUFFER), SEP, ARRAY, LENGTH)

#define SFA_TYPE char *
#define SFA_NAME strings
#define SFA_FORMAT "%s"
#include "sfa.h"

#define SFA_TYPE double
#define SFA_NAME doubles
#define SFA_FORMAT "%f"
#include "sfa.h"

#if !defined(DEBUGGING)
#define DEBUGGING 0
#endif

#ifndef RELEASING
#define RELEASING 0
#endif

#if !defined(ERROR_NOTIFY)
#define ERROR_NOTIFY 0
#endif

#if DEBUGGING || TESTING_util
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wc11-extensions"
#pragma clang diagnostic ignored "-Wformat"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#endif

#endif

static char *notifiers[2] = {"dunstify", "notify-send"};

static void error_async_safe(char *message);
static void fatal(int) __attribute__((noreturn));
static void util_segv_handler(int32) __attribute__((noreturn));
static int32 itoa2(char *, int32, llong);
static long atoi2(char *);
INLINE void *memchr64(void *pointer, int32 value, int64 size);
INLINE int memcmp64(void *left, void *right, int64 size);

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

        if ((p = memchr64(h, n[0], limit - h)) == NULL) {
            return NULL;
        }

        if (memcmp64(p, n, (int64)needle_len) == 0) {
            return (void *)p;
        }
        h = p + 1;
    }

    return NULL;
}
#endif

extern void *memrchr(const void *pointer, int32 character_to_find, size_t size);
void *
memrchr(const void *pointer, int32 character_to_find, size_t size) {
    uchar *buffer = (uchar *)pointer;
    uchar target_byte = (uchar)character_to_find;

    for (long i = (long)(size - 1); i >= 0; i -= 1) {
        if (buffer[i] == target_byte) {
            return (void *)(buffer + i);
        }
    }

    return NULL;
}

INLINE void *
memmem64(void *haystack, int64 hay_len, void *needle, int64 needle_len) {
    void *result;

    if (hay_len <= 0) {
        return NULL;
    }
    if (needle_len <= 0) {
        return NULL;
    }

    result = memmem(haystack, (size_t)hay_len, needle, (size_t)needle_len);
    return result;
}

INLINE void *
memchr64(void *pointer, int32 value, int64 size) {
    if (DEBUGGING) {
        if (size < 0) {
            error("Error in %s: Invalid size = %lld\n", __func__, (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    if (size == 0) {
        return 0;
    }
    return memchr(pointer, value, (size_t)size);
}

INLINE void *
memrchr64(void *pointer, int32 value, int64 size) {
    if (DEBUGGING) {
        if (size < 0) {
            error("Error in %s: Invalid size = %lld\n", __func__, (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    if (size == 0) {
        return 0;
    }
    return memrchr(pointer, value, (size_t)size);
}

INLINE int32
strlen32(char *string) {
    int32 length;
    size_t len = strlen(string);

    if (DEBUGGING) {
        if (len >= MAXOF(length)) {
            error("Error: string (%.*s ...) is too long.\n", 50, string);
            fatal(EXIT_FAILURE);
        }
    }

    length = (int32)len;
    return length;
}

INLINE int
strncmp32(char *left, char *right, int64 size) {
    int result;
    if (size == 0) {
        return 0;
    }
    if (DEBUGGING) {
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error in %s: Size (%lld) is bigger than SIZEMAX\n",
                  __func__, (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    result = strncmp(left, right, (size_t)size);
    return result;
}

INLINE char *
begins_with(char *string, char *literal, int32 length) {
    if (strncmp32(literal, string, length) == 0) {
        return string + length;
    } else {
        return NULL;
    }
}

#define BEGINS_WITH_2(LONG, SHORT) \
        begins_with(LONG, SHORT, strlen32(SHORT))
#define BEGINS_WITH_3(LONG, SHORT, LEN) \
        begins_with(LONG, SHORT, LEN)
#define BEGINS_WITH(...) SELECT_ON_NUM_ARGS(BEGINS_WITH_, __VA_ARGS__)

INLINE int
memcmp64(void *left, void *right, int64 size) {
    int result;
    if (size == 0) {
        return 0;
    }
    if (DEBUGGING) {
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error in %s: Size (%lld) is bigger than SIZEMAX\n",
                  __func__, (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    result = memcmp(left, right, (size_t)size);
    return result;
}

#define X64(FUNC, TYPE) \
INLINE int64 \
CAT(FUNC, 64)(int fd, void *buffer, int64 size) { \
    TYPE instance = 0; \
    ssize_t w; \
    (void)instance; \
    if (size == 0) \
        return 0; \
    if (size < 0) {\
        error("Error in %s: Invalid size = %lld\n", __func__, (llong)size); \
        fatal(EXIT_FAILURE); \
    } \
    if ((ullong)size >= (ullong)MAXOF(instance)) { \
        error("Error in %s: Size (%lld) is too big for %s\n", __func__, \
              (llong)size, #FUNC); \
        fatal(EXIT_FAILURE); \
    } \
    w = FUNC(fd, buffer, (TYPE)size); \
    return (int64)w; \
}

#if OS_WINDOWS
X64(write, uint)
X64(read, uint)
#else
X64(write, size_t)
X64(read, size_t)
#endif

#undef X64

static void
write_all(int fd, char *buffer, int64 left) {
    int64 written = 0;
    int64 w;

    while (left > 0) {
#if OS_WINDOWS
        if ((w = write(fd, buffer + written, (uint)left)) <= 0)
#else
        if ((w = write(fd, buffer + written, (size_t)left)) <= 0)
#endif
        {
            fprintf(stderr, "Error writing: %s.\n", strerror(errno));
            fatal(EXIT_FAILURE);
        }
        left -= w;
        written += w;
    }
    return;
}

#define X64(FUNC) \
INLINE int64 \
CAT(FUNC, 64)(void *buffer, int64 size, int64 n, FILE *file) { \
    size_t rw; \
    if ((size_t)size >= (SIZE_MAX/(size_t)n)) { \
        error("Error in %s: Overflow (%lld*%lld)\n", \
              __func__, (llong)size, (llong)n); \
        fatal(EXIT_FAILURE); \
    } \
    if ((size <= 0) || (n <= 0)) { \
        error("Error in %s: Invalid size(%lld) or n(%lld)\n", \
              __func__, (llong)size, (llong)n); \
        fatal(EXIT_FAILURE); \
    } \
    if ((ullong)size >= (ullong)SIZE_MAX) { \
        error("Error in %s: Size (%lld) is bigger than SIZEMAX\n", \
              __func__, (llong)size); \
        fatal(EXIT_FAILURE); \
    } \
    if ((ullong)n >= (ullong)SIZE_MAX) { \
        error("Error in %s: Number (%lld) is bigger than SIZEMAX\n", \
              __func__, (llong)size); \
        fatal(EXIT_FAILURE); \
    } \
    rw = FUNC(buffer, (size_t)size, (size_t)n, file); \
    return (int64)rw; \
}

X64(fwrite)
X64(fread)

#undef X64

static void
qsort64(void *base, int64 n, int64 size,
        int (*compar)(const void *, const void *)) {
    if (DEBUGGING) {
        if ((size_t)size >= (SIZE_MAX / (size_t)n)) {
            error("Error in %s: Overflow (%lld*%lld)\n",
                  __func__, (llong)size, (llong)n);
            fatal(EXIT_FAILURE);
        }
        if ((size <= 0) || (n <= 0)) {
            error("Error in %s: Invalid size(%lld) or n(%lld)\n",
                  __func__, (llong)size, (llong)n);
            fatal(EXIT_FAILURE);
        }
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error in %s: Size (%lld) is bigger than SIZEMAX\n",
                  __func__, (llong)size);
            fatal(EXIT_FAILURE);
        }
        if ((ullong)n >= (ullong)SIZE_MAX) {
            error("Error in %s: Number (%lld) is bigger than SIZEMAX\n",
                  __func__, (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    qsort(base, (size_t)n, (size_t)size, compar);
    return;
}

#if OS_WINDOWS
static int32
util_nthreads(void) {
    SYSTEM_INFO sysinfo;
    memset64(&sysinfo, 0, SIZEOF(sysinfo));
    GetSystemInfo(&sysinfo);
    return (int32)sysinfo.dwNumberOfProcessors;
}
#else
static int32
util_nthreads(void) {
    return (int32)sysconf(_SC_NPROCESSORS_ONLN);
}
#endif

#if OS_WINDOWS || OS_MAC
#define basename basename2
#endif

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

static void
xpthread_create(pthread_t *thread, pthread_attr_t *attr,
                void *(*function)(void *), void *arg) {
    int err;
    if ((err = pthread_create(thread, attr, function, arg))) {
        error("Error creating thread: %s.\n", strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

static void
xpthread_join(pthread_t *thread, void **thread_return) {
    int err;
    if ((err = pthread_join(*thread, thread_return))) {
        error("Error joining thread: %s.\n", strerror(err));
        fatal(EXIT_FAILURE);
    }
    *thread = 0;
    return;
}

static int32 __attribute__((format(printf, 3, 4)))
snprintf2(char *buffer, int64 size, char *format, ...) {
    int n;
    va_list args;

    va_start(args, format);
    n = vsnprintf(buffer, (size_t)size, format, args);
    va_end(args);

    if ((n < 0) || (n >= size)) {
        fprintf(stderr, "Error in vsnprintf(\"%s\") (n = %lld)\n",
                        format, (llong)n);
        fatal(EXIT_FAILURE);
    }
    return n;
}

int32
itoa2(char *str, int32 size, llong num) {
    int i = 0;
    bool negative = false;

    if (size < 22) {
        error("Error in itoa2: buffer is too small.\n");
        fatal(EXIT_FAILURE);
    }

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

    for (long j = 0; j < i / 2; j += 1) {
        char temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }

    // this is here because of gcc -fanalyzer
    assert(i < 22);

    return i;
}

#define ITOA(buffer, num) itoa2(buffer, sizeof(buffer), num)

long
atoi2(char *str) {
    return atoi(str);
}

static int64
strftime2(char *buffer, int64 size, char *format, struct tm *time_info) {
    int64 n;

    n = (int64)strftime(buffer, (size_t)size, format, time_info);
    if ((n <= 0) || (n >= size)) {
        error("Error in strftime(%s) (n = %lld).\n", format, (llong)n);
        fatal(EXIT_FAILURE);
    }
    return n;
}

static int32
util_filename_from(char *buffer, int64 size, int fd) {
#if OS_LINUX
    char linkpath[64];
    ssize_t len;

    SNPRINTF(linkpath, "/proc/self/fd/%d", fd);
    if ((len = readlink(linkpath, buffer, (size_t)(size - 1))) < 0) {
        return -1;
    }
    buffer[len] = '\0';
    return 0;
#elif OS_MAC
    static char buffer2[MAXPATHLEN];
    int64 len;

    if (fcntl(fd, F_GETPATH, buffer2) < 0) {
        return -1;
    }
    len = MIN(strlen32(buffer2), size - 1);
    memcpy64(buffer, buffer2, len + 1);
    return 0;
#elif OS_WINDOWS
    HANDLE h;
    DWORD len;
    intptr_t h2 = _get_osfhandle(fd);

    if ((h = (HANDLE)h2) == INVALID_HANDLE_VALUE) {
        return -1;
    }

    len = GetFinalPathNameByHandleA(h, buffer, (DWORD)size,
                                    FILE_NAME_NORMALIZED);

    if ((len <= 0) || (len >= size)) {
        return -1;
    }

    if (strncmp32(buffer, "\\\\?\\", 4) == 0) {
        memmove64(buffer, buffer + 4, len - 3);
    }

    return 0;
#else
    (void)size;
    (void)fd;
    (void)buffer;
    return -1;
#endif
}

#if OS_WINDOWS
static int
strerror_r(int errnum, char *buffer, size_t size) {
    char *error_message = strerror(errnum);
    int32 len = strlen32(error_message);
    memcpy64(buffer, error_message, (llong)MIN(len + 1, size - 1));
    buffer[size] = '\0';
    return 0;
}
#endif

static int
xclose(char *file, int line, int *fd, char *fd_var_name, char *filename) {
#if DEBUGGING
    char buffer[4096];

    if (filename == NULL) {
        if (util_filename_from(buffer, sizeof(buffer), *fd) < 0) {
            filename = fd_var_name;
        } else {
            filename = buffer;
        }
    }
#else
    if (filename == NULL) {
        filename = fd_var_name;
    }
#endif

    if (close(*fd) < 0) {
        char error_buffer[4096];
        char itoa_buffer[32];
        ITOA(itoa_buffer, line);

        strerror_r(errno, error_buffer, sizeof(error_buffer));

        error_async_safe(file);
        error_async_safe(":");
        error_async_safe(itoa_buffer);
        error_async_safe(" Error closing ");
        error_async_safe(filename);
        error_async_safe(": ");
        error_async_safe(error_buffer);
        error_async_safe(".\n");

        *fd = -1;
        return -1;
    }
    *fd = -1;
    return 0;
}

#define XCLOSE_1(FD)       xclose(__FILE__, __LINE__, FD, #FD, NULL)
#define XCLOSE_2(FD, NAME) xclose(__FILE__, __LINE__, FD, #FD, NAME)
#define XCLOSE(...) SELECT_ON_NUM_ARGS(XCLOSE_, __VA_ARGS__)

static int
xunlink(char *filename) {
    if (unlink(filename) < 0) {
        error("Error in unlink(%s): %s.\n", filename, strerror(errno));
        return -1;
    }
    return 0;
}

#if OS_WINDOWS
static int
util_command(int argc, char **argv) {
    char cmdline[BUFSIZ] = {0};
    FILE *tty;
    PROCESS_INFORMATION proc_info = {0};
    DWORD exit_code = 0;
    int64 len0 = strlen32(argv[0]);
    char argv0_windows[BUFSIZ];
    char *argv0 = argv[0];

    if (len0 >= BUFSIZ) {
        error("Invalid arguments.\n");
        fatal(EXIT_FAILURE);
    }

    if (argc == 0 || argv == NULL) {
        error("Invalid arguments.\n");
        fatal(EXIT_FAILURE);
    }

    {
        char *exe = ".exe";
        int64 exe_len = (int64)(strlen32(exe));
        if (memmem64(argv[0], len0 + 1, exe, exe_len + 1) == NULL) {
            memcpy64(argv0_windows, argv[0], len0);
            memcpy64(argv0_windows + len0, exe, exe_len + 1);
            argv[0] = argv0_windows;
        }
    }

    {
        int64 j = 0;
        for (int i = 0; i < argc - 1; i += 1) {
            int64 len2 = strlen32(argv[i]);
            if ((j + len2) >= SIZEOF(cmdline)) {
                error("Command line is too long.\n");
                fatal(EXIT_FAILURE);
            }

            cmdline[j] = '"';
            memcpy64(&cmdline[j + 1], argv[i], len2);
            cmdline[j + len2 + 1] = '"';
            cmdline[j + len2 + 2] = ' ';
            j += len2 + 3;
        }
        cmdline[j - 1] = '\0';
    }

    argv[0] = argv0;

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
            error("Error running '%s': %llu.\n", cmdline, (ullong)err);
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
static int
util_command(int argc, char **argv) {
    pid_t child;
    int status;
    (void)argc;

    switch (child = fork()) {
    case 0:
        if (!freopen("/dev/tty", "r", stdin)) {
            error("Error reopening stdin: %s.\n", strerror(errno));
        }
        execvp(argv[0], argv);
        error("\nError executing '%s", argv[0]);
        for (int j = 1; j < argc; j += 1) {
            error(" %s", argv[j]);
        }
        error("': %s.\n", strerror(errno));
        exit(2);
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

static int
util_command_launch(int argc, char **argv) {
    (void)argc;

    switch (fork()) {
    case 0:
        if (setsid() < 0) {
            error("Error in setsid: %s.\n", strerror(errno));
        }
        execvp(argv[0], argv);
        error("\nError executing '%s", argv[0]);
        for (int j = 1; j < argc; j += 1) {
            error(" %s", argv[j]);
        }
        error("': %s.\n", strerror(errno));
        return -1;
    case -1:
        error("Error forking: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    default:
        return 0;
    }
}
#endif

void __attribute__((format(printf, 3, 4)))
error_impl(char *file, int32 line, char *format, ...) {
    char buffer[BUFSIZ];
    char *big_buffer = NULL;
    char *pbuffer = buffer;
    va_list args;
    int32 n;
    int32 m = SIZEOF(buffer);
    int32 p;
    char fileline[64];

    va_start(args, format);
    n = vsnprintf(buffer, (size_t)m, format, args);

    if (n >= m) {
        if (RELEASING) {
            m = n + 1;
            big_buffer = malloc((size_t)m);
            assert(big_buffer);
            n = vsnprintf(big_buffer, (size_t)m, format, args);
            pbuffer = big_buffer;
        } else {
            fprintf(stderr, "Error in vsnprintf(\"%s\") (n = %lld).\n",
                            format, (llong)n);
            fatal(EXIT_FAILURE);
        }
    }

    va_end(args);

    if ((n < 0) || (n >= m)) {
        fprintf(stderr, "Error in vsnprintf(\"%s\") (n = %lld).\n",
                        format, (llong)n);
        fatal(EXIT_FAILURE);
    }

    if (!RELEASING) {
        p = SNPRINTF(fileline, "%s:%d: ", file, line);
    } else {
        p = 0;
    }

    if (p) {
        write_all(STDERR_FILENO, fileline, p);
    }
    write_all(STDERR_FILENO, pbuffer, n);
#if OS_UNIX
    fsync(STDERR_FILENO);
    fsync(STDOUT_FILENO);
#endif

#if ERROR_NOTIFY
#if OS_WINDOWS
#error "ERROR_NOTIFY is defined but unsupported for windows."
#endif
    switch (fork()) {
    case 0:
        for (uint32 i = 0; i < LENGTH(notifiers); i += 1) {
            execlp(notifiers[i],
                   notifiers[i], "-u", "critical", program, pbuffer, NULL);
        }
        fprintf(stderr, "Error executing notifier: %s.\n", strerror(errno));
        exit(EXIT_FAILURE);
    case -1:
        fprintf(stderr, "Error forking: %s.\n", strerror(errno));
        break;
    default:
        break;
    }
#endif

    if (big_buffer) {
        free2(big_buffer, m);
    }
    return;
}

static void
error_async_safe(char *message) {
    int32 len = strlen32(message);
    write_all(STDERR_FILENO, message, len);
    return;
}

void
fatal(int status) {
    if (DEBUGGING) {
        (void)status;
        TRAP();
    } else {
        exit(status);
    }
}

void
util_segv_handler(int32 unused) {
    char *message = "Memory error. Please send a bug report.\n";
    (void)unused;

    write64(STDERR_FILENO, message, (uint32)strlen32(message));
    for (uint32 i = 0; i < LENGTH(notifiers); i += 1) {
        execlp(notifiers[i],
               notifiers[i], "-u", "critical", program, message, NULL);
    }
    _exit(EXIT_FAILURE);
}

static int32
util_string_int32(int32 *number, char *string) {
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

static void __attribute__((noreturn))
util_die_notify(char *program_name, char *format, ...) {
    int32 n;
    va_list args;
    char buffer[BUFSIZ];

    va_start(args, format);
    n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if ((n < 0) || (n >= SIZEOF(buffer))) {
        fatal(EXIT_FAILURE);
    }

    buffer[n] = '\0';
    write64(STDERR_FILENO, buffer, (uint32)n + 1);
    for (uint32 i = 0; i < LENGTH(notifiers); i += 1) {
        execlp(notifiers[i], notifiers[i], "-u", "critical", program_name,
               buffer, NULL);
    }
    fatal(EXIT_FAILURE);
}

#if OS_UNIX
static int32
util_copy_file_sync(char *destination, char *source) {
    int32 source_fd;
    int32 destination_fd;
    char buffer[BUFSIZ];
    ssize_t r = 0;
    ssize_t w = 0;

    if ((source_fd = open(source, O_RDONLY)) < 0) {
        error("Error opening %s for reading: %s.\n", source, strerror(errno));
        return -1;
    }

    if ((destination_fd
             = open(destination,
                    O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
        error("Error opening %s for writing: %s.\n",
              destination, strerror(errno));
        XCLOSE(&source_fd, source);
        return -1;
    }

    errno = 0;
    while ((r = read64(source_fd, buffer, BUFSIZ)) > 0) {
        w = write64(destination_fd, buffer, r);
        if (w != r) {
            fprintf(stderr, "Error writing data to %s", destination);
            if (errno) {
                fprintf(stderr, ": %s", strerror(errno));
            }
            fprintf(stderr, ".\n");

            XCLOSE(&source_fd, source);
            XCLOSE(&destination_fd, destination);
            return -1;
        }
    }

    if (r < 0) {
        error("Error reading data from %s: %s.\n", source, strerror(errno));
        XCLOSE(&source_fd, source);
        XCLOSE(&destination_fd, destination);
        return -1;
    }

    XCLOSE(&source_fd, source);
    XCLOSE(&destination_fd, destination);
    return 0;
}


#if !defined(MAX_FILES_COPY)
#define MAX_FILES_COPY 256
#endif

typedef struct UtilCopyFilesAsync {
    struct pollfd pipes[MAX_FILES_COPY];
    int dests[MAX_FILES_COPY];
    int32 nfds;
    int32 unused;
} UtilCopyFilesAsync;

static int32
util_copy_file_async(char *destination, char *source, int *dest_fd) {
    int32 source_fd;
    int32 fadvise_err;

    if ((source_fd = open(source, O_RDONLY)) < 0) {
        error("Error opening %s for reading: %s.\n", source, strerror(errno));
        return -1;
    }

    if ((*dest_fd
         = open(destination, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR))
        < 0) {
        error("Error opening %s for writing: %s.\n",
              destination, strerror(errno));
        XCLOSE(&source_fd, source);
        return -1;
    }

    if ((fadvise_err = posix_fadvise(pipes[nfds].fd,
                                     0, 0,
                                     POSIX_FADV_WILLNEED)) < 0) {
        error("Error in posix_fadvise(POSIX_FADV_WILLNEED): %s.\n",
              strerror(fadvise_err));
    }

    return source_fd;
}

static void *
util_copy_file_async_thread(void *arg) {
    UtilCopyFilesAsync *copy_files = arg;
    struct pollfd *pipes = copy_files->pipes;
    int *dests = copy_files->dests;
    int32 left = copy_files->nfds;

    if (copy_files->nfds >= LENGTH(copy_files->pipes)) {
        error("Error in %s:"
              " too many files for UtilCopyFilesAsync definition.\n",
              __func__);
        fatal(EXIT_FAILURE);
    }

    while (left > 0) {
        char buffer[BUFSIZ];
        int64 r;
        int64 w;
        int64 n;

        n = poll(pipes, (nfds_t)copy_files->nfds, 1000);
        if (n == 0) {
            break;
        }
        if (n < 0) {
            error("Error in poll(nfds=%lld): %s.\n",
                  (llong)copy_files->nfds, strerror(errno));
            break;
        }
        for (int32 i = 0; i < copy_files->nfds; i += 1) {
            if (n <= 0) {
                break;
            }
            if (!(pipes[i].revents & POLL_IN)) {
                pipes[i].revents = 0;
                continue;
            }
            n -= 1;
            while ((r = read64(pipes[i].fd, buffer, sizeof(buffer))) > 0) {
                if ((w = write64(dests[i], buffer, r)) != r) {
                    if (w < 0) {
                        error("Error writing: %s.\n", strerror(errno));
                    }
                    break;
                }
            }
            if (r < 0) {
                error("Error reading: %s.\n", strerror(errno));
            }
            XCLOSE(&dests[i]);
            XCLOSE(&pipes[i].fd);

            left -= 1;
            pipes[i].revents = 0;
        }
    }
    free2(copy_files, sizeof(*copy_files));
    pthread_exit(NULL);
    return NULL;
}

#endif

#if OS_LINUX
#include <dirent.h>
static void
send_signal(char *executable, int32 signal_number) {
    DIR *processes;
    struct dirent *process;
    int64 len = strlen32(executable);

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
        char *last;

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
        if ((r = read64(cmdline, command, sizeof(command))) <= 0) {
            (void)r;
            XCLOSE(&cmdline, buffer);
            continue;
        }
        XCLOSE(&cmdline, buffer);

        if (memmem64(command, r, executable, len)) {
            if ((last = memchr64(command, '\0', r))) {
                r = last - command;
                if (!memmem64(command, r, executable, len)) {
                    continue;
                }
            }
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
    }

    closedir(processes);
    return;
}
#elif OS_UNIX
static void
send_signal(char *executable, int32 signal_number) {
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
static void
send_signal(char *executable, int32 signal_number) {
    (void)executable;
    (void)signal_number;
    return;
}
#endif

static bool
util_equal_files(char *filename_a, char *filename_b) {
    int fd_a;
    int fd_b = -1;
    char buffer_a[BUFSIZ];
    char buffer_b[BUFSIZ];
    int64 total_r = 0;
    int64 ra;
    struct stat stat_a;
    struct stat stat_b;
    bool equal = false;

    if ((fd_a = open(filename_a, O_RDONLY)) < 0) {
        error("Error opening %s: %s.\n", filename_a, strerror(errno));
        equal = false;
        goto out;
    }
    if ((fd_b = open(filename_b, O_RDONLY)) < 0) {
        error("Error opening %s: %s.\n", filename_b, strerror(errno));
        equal = false;
        goto out;
    }

    if (fstat(fd_a, &stat_a) < 0) {
        error("Error in stat(%s): %s.\n", filename_a, strerror(errno));
        equal = false;
        goto out;
    }
    if (fstat(fd_b, &stat_b) < 0) {
        error("Error in stat(%s): %s.\n", filename_b, strerror(errno));
        equal = false;
        goto out;
    }
    if (stat_a.st_size != stat_b.st_size) {
        equal = false;
        goto out;
    }
    if ((stat_a.st_dev == stat_b.st_dev) && (stat_a.st_ino == stat_b.st_ino)) {
        equal = true;
        goto out;
    }

#if OS_UNIX
    do {
        if (stat_a.st_size > 0) {
            void *map_a;
            void *map_b;

            map_a = mmap(NULL, (size_t)stat_a.st_size,
                         PROT_READ, MAP_PRIVATE, fd_a, 0);
            if (map_a == MAP_FAILED) {
                error("Error in mmap(%s): %s\n", filename_a, strerror(errno));
                break;
            }
            map_b = mmap(NULL, (size_t)stat_a.st_size,
                         PROT_READ, MAP_PRIVATE, fd_b, 0);
            if (map_b == MAP_FAILED) {
                error("Error in mmap(%s): %s\n", filename_b, strerror(errno));
                xmunmap(map_a, stat_a.st_size);
                break;
            }

            if (memcmp64(map_a, map_b, stat_a.st_size)) {
                equal = false;
            } else {
                equal = true;
            }

            xmunmap(map_a, stat_a.st_size);
            xmunmap(map_b, stat_b.st_size);
            goto out;
        } else {
            equal = true;
            goto out;
        }
    } while (0);
#endif
    while ((ra = read64(fd_a, buffer_a, sizeof(buffer_a))) > 0) {
        int64 rb;
        if ((rb = read64(fd_b, buffer_b, sizeof(buffer_b))) != ra) {
            if (rb < 0) {
                error("Error reading from %s: %s", filename_b, strerror(errno));
            }
            equal = false;
            goto out;
        }
        if (memcmp64(buffer_a, buffer_b, ra)) {
            equal = false;
            goto out;
        }
        total_r += ra;
    }
    if (ra < 0) {
        error("Error reading from %s: %s", filename_a, strerror(errno));
    }
    if (total_r == stat_a.st_size) {
        equal = true;
        goto out;
    }
out:
    XCLOSE(&fd_a, filename_a);
    XCLOSE(&fd_b, filename_b);
    return equal;
}

INLINE double
rad2deg(double radians) {
    double RAD2DEG = 180.0 / 3.141592653589793;
    return radians*RAD2DEG;
}

INLINE double
deg2rad(double degrees) {
    double DEG2RAD = 3.141592653589793 / 180.0;
    return degrees*DEG2RAD;
}

static int32
bytes_pretty(char *buffer, int64 raw) {
    char *suffixes[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    double aux_pretty;
    int64 i;
    int32 n;
    char *comma;

    if (raw < 0) {
        *buffer = '\0';
        return 0;
    }

    if (raw <= 1023) {
        n = snprintf2(buffer, 16, "%lldB", (llong)raw);
        return n;
    }

    aux_pretty = (double)raw;
    i = 0;
    while ((aux_pretty >= 1024.0) && (i < LENGTH(suffixes))) {
        aux_pretty /= 1024.0;
        i += 1;
    }

    if (aux_pretty >= 1000) {
        n = snprintf2(buffer, 16, "%.1f%s", aux_pretty, suffixes[i]);
    } else if (aux_pretty >= 100) {
        n = snprintf2(buffer, 16, "%.2f%s", aux_pretty, suffixes[i]);
    } else if (aux_pretty >= 10) {
        n = snprintf2(buffer, 16, "%.3f%s", aux_pretty, suffixes[i]);
    } else {
        n = snprintf2(buffer, 16, "%.4f%s", aux_pretty, suffixes[i]);
    }

    if ((comma = memchr64(buffer, ',', n))) {
        *comma = '.';
    }

    return n;
}

static void
normalize(char *path, int32 *length) {
    char *p;
    int64 off = 0;

    if (*length < 0) {
        *length = strlen32(path);
    }

    while ((p = memmem64(path + off, *length - off, "//", 2))) {
        off = p - path;

        memmove64(&p[0], &p[1], *length - off);
        *length -= 1;
    }

    while ((path[0] == '.') && (path[1] == '/') && (*length > 2)) {
        memmove64(&path[0], &path[2], *length - 1);
        *length -= 2;
    }

    while ((*length >= 2) && (path[*length - 2] == '/') && (path[*length - 1] == '.')) {
        path[*length - 1] = '\0';
        *length -= 1;
    }

    off = 0;
    while ((p = memmem64(path + off, *length - off, "/./", 3))) {
        off = p - path;

        memmove64(&p[1], &p[3], *length - off - 2);
        *length -= 2;
    }

    return;
}

static char *
basename2(char *path, int32 *full_length, int32 *base_len) {
    int32 left;
    char *end;
    char *fslash = NULL;
    char *bslash = NULL;
    char *p = path;

    normalize(path, full_length);

    left = *full_length;
    end = path + left - 1;

    if (left == 1) {
        if (base_len) {
            *base_len = 1;
        }
        return p;
    }

    while (left > 0) {
        int64 length;

        fslash = memchr64(p, '/', left);
        if (OS_WINDOWS) {
            bslash = memchr64(p, '\\', left);
        }

        if ((fslash == NULL) && (bslash == NULL)) {
            if (base_len) {
                *base_len = *full_length - (int32)(p - path);
            }
            return p;
        }
        if ((fslash == end) || (bslash == end)) {
            if (base_len) {
                *base_len = *full_length - (int32)(p - path);
            }
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

    if (base_len) {
        *base_len = *full_length;
    }
    return path;
}

static int32
dirname2(char *buffer, char *path, int32 *path_len) {
    char *last_slash;
    int32 dir_length;
    if (*path_len < 0) {
        *path_len = strlen32(path);
    }

    normalize(path, path_len);

    if (*path_len == 1) {
        if (*path == '/') {
            sprintf(buffer, "/");
        } else {
            sprintf(buffer, ".");
        }
        return 1;
    }

    if ((last_slash = memrchr64(path, '/', *path_len - 1)) == NULL) {
        sprintf(buffer, ".");
        return 1;
    }

    dir_length = (int32)(last_slash - path);
    if (dir_length == 0) {
        dir_length = 1;
    }

    if (buffer != path) {
        memcpy64(buffer, path, dir_length);
    }

    buffer[dir_length] = '\0';
    return dir_length;
}

static void
print_timings(char *file, int32 line, char *func,
              int64 nitems, struct timespec t0, struct timespec t1) {
    llong seconds = t1.tv_sec - t0.tv_sec;
    llong nanos = t1.tv_nsec - t0.tv_nsec;

    double total_seconds = (double)seconds + (double)nanos / 1.0e9;
    double micros_per = 1e6*(total_seconds / (double)nitems);

    printf("\ntime elapsed %s:%d:%s\n", file, line, func);
    printf("%gs = %gus per item.\n\n", total_seconds, micros_per);
    return;
}
#define PRINT_TIMINGS_3(N, T0, T1) \
        print_timings(__FILE__, __LINE__, (char *)__func__, N, T0, T1)
#define PRINT_TIMINGS_4(N, T0, T1, NAME) \
        print_timings(__FILE__, __LINE__, NAME, N, T0, T1)
#define PRINT_TIMINGS(...) SELECT_ON_NUM_ARGS(PRINT_TIMINGS_, __VA_ARGS__)

#if OS_UNIX

#define XSIGNAL(NAME) [NAME] = #NAME
static char *signal_names[] = {
    XSIGNAL(SIGABRT),
    XSIGNAL(SIGALRM),
    XSIGNAL(SIGVTALRM),
    XSIGNAL(SIGPROF),
    XSIGNAL(SIGBUS),
    XSIGNAL(SIGCHLD),
    XSIGNAL(SIGCONT),
    XSIGNAL(SIGFPE),
    XSIGNAL(SIGHUP),
    XSIGNAL(SIGILL),
    XSIGNAL(SIGINT),
    XSIGNAL(SIGKILL),
    XSIGNAL(SIGPIPE),
#if defined(SIGPOLL)
    XSIGNAL(SIGPOLL),
#endif
    XSIGNAL(SIGQUIT),
    XSIGNAL(SIGSEGV),
    XSIGNAL(SIGSTOP),
    XSIGNAL(SIGSYS),
    XSIGNAL(SIGTERM),
    XSIGNAL(SIGTSTP),
    XSIGNAL(SIGTTIN),
    XSIGNAL(SIGTTOU),
    XSIGNAL(SIGTRAP),
    XSIGNAL(SIGURG),
    XSIGNAL(SIGUSR1),
    XSIGNAL(SIGUSR2),
    XSIGNAL(SIGXCPU),
    XSIGNAL(SIGXFSZ),
};
#undef XSIGNAL

static void
xpipe(int array[2]) {
    if (pipe(array) < 0) {
        error("Error creating pipe: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    }
    return;
}

static void
xdup2(int fd1, int fd2) {
    if (dup2(fd1, fd2) < 0) {
        error("Error in dup2: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    }
    return;
}

static void
xkill(pid_t pid, int signum) {
    if (kill(pid, signum) < 0) {
        error("Error sending signal %d=%s to %d: %s.\n",
              signum, signal_names[signum], pid, strerror(errno));
    }
    return;
}
#endif

#if OS_UNIX
static void
timezone_init(void) {
    time_t current_time;
    struct tm local_tm;
    struct tm gm_tm;

    current_time = time(NULL);
    localtime_r(&current_time, &local_tm);
    gmtime_r(&current_time, &gm_tm);

    timezone_offset = (local_tm.tm_hour - gm_tm.tm_hour)*3600;
    timezone_offset += (local_tm.tm_min - gm_tm.tm_min)*60;

    if (local_tm.tm_year < gm_tm.tm_year) {
        timezone_offset -= 24*3600;
    } else if (local_tm.tm_year > gm_tm.tm_year) {
        timezone_offset += 24*3600;
    } else if (local_tm.tm_yday < gm_tm.tm_yday) {
        timezone_offset -= 24*3600;
    } else if (local_tm.tm_yday > gm_tm.tm_yday) {
        timezone_offset += 24*3600;
    }

    timezone_initialized = true;
    return;
}
#endif

static ullong here_counter = 0;

#define HERE do { \
    fprintf(stderr, "\n===== HERE(%llu): %s:%d (%s)\n", \
                    here_counter++, __FILE__, __LINE__, __func__); \
} while (0)

#define NCALLS(INTERVAL) do { \
    static int64 ncalls_ncalls = 1; \
    if ((ncalls_ncalls % INTERVAL) == 0) { \
        fprintf(stderr, "%s:%d:%s: called %lld times\n", \
                        __FILE__, __LINE__, __func__, (llong)ncalls_ncalls); \
    } \
    ncalls_ncalls += 1; \
} while (0)

#if 0 == TESTING_util
static inline void
util_functions_sink(void) {
    (void)here_counter;
    (void)util_segv_handler;
    (void)util_nthreads;
    (void)util_filename_from;
    (void)util_command;
    (void)util_string_int32;
    (void)util_die_notify;
#if OS_UNIX
    (void)util_copy_file_sync;
    (void)util_copy_file_async;
    (void)util_copy_file_async_thread;
    (void)util_command_launch;
#endif
    (void)util_equal_files;

    (void)malloc_debug;
    (void)realloc_debug;
    (void)free_debug;

    (void)send_signal;
    (void)atoi2;
#if OS_UNIX
    (void)timezone_init;
#endif
    (void)dirname2;
    (void)basename2;
    (void)string_from_doubles;
    (void)string_from_strings;
    (void)strftime2;
    (void)bytes_pretty;
    (void)qsort64;
    (void)print_timings;

    (void)xmmap_commit;
    (void)xstrdup;
#if OS_UNIX
    (void)xkill;
    (void)xdup2;
    (void)xpipe;
#endif
    (void)xmemdup;
    (void)xunlink;

    (void)xpthread_mutex_lock;
    (void)xpthread_mutex_unlock;
    (void)xpthread_cond_destroy;
    (void)xpthread_mutex_destroy;
    (void)xpthread_create;
    (void)xpthread_join;
    return;
}
#endif

#if TESTING_util

#define ENUM_NAME WeekDay
#define ENUM_BITFLAGS 0
#define ENUM_PREFIX_ WEEK_DAY_
#define ENUM_FIELDS \
    X(SUNDAY, 0)                   \
    X(MONDAY)                      \
    X(TUESDAY, 10)                 \
    X(WEDNESDAY)                   \
    X(THURSDAY)                    \
    X(FRIDAY, 5)                   \
    X(SATURDAY, 20)
#include "xenums.c"


#define ENUM_NAME PowerOfTwo
#define ENUM_BITFLAGS 1
#define ENUM_PREFIX_ POWER_OF2_
#define ENUM_FIELDS \
    X(ONE)          \
    X(TWO)          \
    X(FOUR)         \
    X(EIGHT)        \
    X(SIXTEEN)      \
    X(THIRTY2)
#include "xenums.c"


static void
write_file(char *path, void *data, int64 len) {
    int fd;

    if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
        error("Error opening %s: %s.\n", path, strerror(errno));
        fatal(EXIT_FAILURE);
    }
    if (write64(fd, data, len) != len) {
        error("Error in write: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    }
    XCLOSE(&fd, path);
    return;
}
#define WRITE_FILE(PATH, STRING) write_file(PATH, STRING, strlen32(STRING))

static sig_atomic_t received_signal = false;
static void
signal_handler(int signal_number) {
    (void)signal_number;
    received_signal = true;
    return;
}

static int
util_test_qsort_cmp(const void *a, const void *b) {
    int32 va = *(const int32 *)a;
    int32 vb = *(const int32 *)b;
    if (va < vb) {
        return -1;
    }
    if (va > vb) {
        return 1;
    }
    return 0;
}

int
main(int argc, char **argv) {
    char *s1 = "aaaabbbb";
    struct timespec t0;
    struct timespec t1;

    (void)argc;
    (void)argv;
    (void)here_counter;

    ASSERT(BEGINS_WITH(s1, "aaaa"));
    ASSERT(BEGINS_WITH(s1, "aaaabbbb"));

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
#if OS_UNIX
    timezone_init();
#endif

    {
        int a = 10;
        int b = 20;

        SWAP(a, b);
        ASSERT_EQUAL(a, 20);
        ASSERT_EQUAL(b, 10);

        ASSERT_EQUAL(ALIGN_POWER_OF_2(7, 16), 16);
        ASSERT_EQUAL(ALIGN_POWER_OF_2(16, 16), 16);
        ASSERT_EQUAL(ALIGN_POWER_OF_2(17, 16), 32);
        ASSERT_EQUAL(ALIGN16(7), 16);
    }

    for (enum WeekDay day = WEEK_DAY_MONDAY; day <= WEEK_DAY_LAST; day += 1) {
        printf("enum[%d] = %s\n", day, WEEK_DAY_str(day));
    }

    printf("\n");

    for (uint x = 0; x < POWER_OF2_LAST; x += 1) {
        char *value_name = POWER_OF2_str((enum PowerOfTwo)x);
        printf("enum[%d] = %s\n", x, value_name);
    }

    if (OS_LINUX && !DEBUGGING) {
        struct sigaction signal_action;
        signal_action.sa_handler = signal_handler;
        sigemptyset(&signal_action.sa_mask);
        signal_action.sa_flags = SA_RESTART;
        if (sigaction(SIGUSR1, &signal_action, NULL) != 0) {
            error2("Error in sigaction: %s.\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        send_signal(argv[0], SIGUSR1);
        ASSERT(received_signal);
    }

    srand((uint)time(NULL));
    for (int i = 0; i < 10; i += 1) {
        int n = rand() - RAND_MAX / 2;
        char itoa_buffer[32];
        ITOA(itoa_buffer, n);
        ASSERT_EQUAL(atoi2(itoa_buffer), n);
    }

    {
        int32 n;
        ASSERT_EQUAL(util_string_int32(&n, "12345"), 0);
        ASSERT_EQUAL(n, 12345);
        ASSERT_EQUAL(util_string_int32(&n, "-54321"), 0);
        ASSERT_EQUAL(n, -54321);
        ASSERT_EQUAL(util_string_int32(&n, "2147483648"), -1);
        ASSERT_EQUAL(util_string_int32(&n, "notanumber"), -1);
    }

    {
        char b[32];
        bytes_pretty(b, 512);
        ASSERT_EQUAL((char *)b, "512B");
        bytes_pretty(b, 1024);
        ASSERT_EQUAL((char *)b, "1.0000kB");
        bytes_pretty(b, SIZEMB(2));
        ASSERT_EQUAL((char *)b, "2.0000MB");
    }

    {
        int32 arr[] = {10, 5, 20, 1};
        qsort64(arr, 4, sizeof(int32), util_test_qsort_cmp);
        ASSERT_EQUAL(arr[0], 1);
        ASSERT_EQUAL(arr[1], 5);
        ASSERT_EQUAL(arr[2], 10);
        ASSERT_EQUAL(arr[3], 20);
    }

    {
        char b[64];
        char *strs[] = {"one", "two", "three"};
        double dbls[] = {1.1, 2.2};
        string_from_strings(b, sizeof(b), "|", strs, 3);
        ASSERT_EQUAL(b, "one|two|three");
        string_from_doubles(b, sizeof(b), ",", dbls, 2);
        ASSERT_NOT_EQUAL(strlen32(b), 0);
    }

    {
        char *src = "memdup_test";
        char *dup = xmemdup(src, 12);
        ASSERT_EQUAL(src, dup);
        ASSERT_NOT_EQUAL((void *)src, (void *)dup);
        free2(dup, 12);
    }

    {
        char b[128];
        struct tm fixed_time;
        fixed_time.tm_year = 126; // 2026
        fixed_time.tm_mon = 2;   // March
        fixed_time.tm_mday = 25;
        fixed_time.tm_hour = 12;
        fixed_time.tm_min = 0;
        fixed_time.tm_sec = 0;
        strftime2(b, sizeof(b), "%Y-%m-%d", &fixed_time);
        ASSERT_EQUAL(b, "2026-03-25");
    }

    {
        char paths[][30] = {
            "/aaaa/bbbb/cccc", "/aa/bb/cc",  "/a/b/c",    "a/b//c",
            "a/b/cccc",        "a/bb/cccc", "aaaa//cccc", "/aaaa",
            "/",               "//",          "//a/",        "/a/b///",
            "./",              "..",          "././",      "./a/",
        };
        char *bases[20] = {
            "cccc",            "cc",          "c",         "c",
            "cccc",            "cccc",        "cccc",      "aaaa",
            "/",               "/",           "a/",        "b/",
            "./",              "..",          "./",        "a/",
        };
        char *dirs[20] = {
            "/aaaa/bbbb",      "/aa/bb",      "/a/b",      "a/b",
            "a/b",              "a/bb",        "aaaa",      "/",
            "/",               "/",           "/",         "/a",
            ".",               ".",           ".",         ".",
        };
        char *normalized[20] = {
            "/aaaa/bbbb/cccc", "/aa/bb/cc",   "/a/b/c",    "a/b/c",
            "a/b/cccc",        "a/bb/cccc",   "aaaa/cccc", "/aaaa",
            "/",               "/",           "/a/",        "/a/b/",
            "./",              "..",          "./",        "a/",
        };
        for (int64 i = 0; i < LENGTH(paths); i += 1) {
            char *path = xstrdup(paths[i]);
            char *base = bases[i];
            int32 path_len = strlen32(path);
            ASSERT_EQUAL(basename2(path, &path_len, NULL), base);
            free2(path, path_len + 1);
        }
        for (int64 i = 0; i < LENGTH(paths); i += 1) {
            char *copy = xstrdup(paths[i]);
            int len = strlen32(copy);
            normalize(copy, &len);
            ASSERT_EQUAL(copy, normalized[i]);
            free2(copy, len + 1);
        }

        for (int64 i = 0; i < LENGTH(paths); i += 1) {
            char dir_buffer[4096];
            int32 path_len = strlen32(paths[i]);
            dirname2(dir_buffer, paths[i], &path_len);
            ASSERT_EQUAL(dir_buffer, dirs[i]);
        }
        {
            char dir_buffer[128] = "a/b/c";
            int32 path_len = strlen32(dir_buffer);
            dirname2(dir_buffer, dir_buffer, &path_len);
            ASSERT_EQUAL(dir_buffer, "a/b");
        }
    }

    if (OS_WINDOWS) {
        char *path2 = "aa\\cc";
        int32 path_len;
        ASSERT_EQUAL(basename2(path2, &path_len, NULL), "cc");
    }

    {
        char *a = "/tmp/afile";
        char *b = "/tmp/bfile";

        WRITE_FILE(a, "hello world");
        WRITE_FILE(b, "hello world");
        ASSERT(util_equal_files(a, b));

        WRITE_FILE(a, "hello world");
        WRITE_FILE(b, "hello worlx");
        ASSERT(!util_equal_files(a, b));

        WRITE_FILE(a, "short");
        WRITE_FILE(b, "shorter");
        ASSERT(!util_equal_files(a, b));

        WRITE_FILE(a, "");
        WRITE_FILE(b, "");
        ASSERT(util_equal_files(a, b));
    }

    {
        char characters[] = "abcdefghijklmnopqrstuvwxyz1234567890";
        char buffer2[4096];
        char name2[256];
        char buffer3[4096];
        char *name = "/tmp/test";
        int fd;

        if ((fd = open(name,
                       O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
            error("Error opening %s: %s.\n", name, strerror(errno));
            fatal(EXIT_FAILURE);
        }

        util_filename_from(buffer2, sizeof(buffer2), fd);
        ASSERT_EQUAL(realpath(name, buffer3), buffer2);
        xunlink(name);

        XCLOSE(&fd);

        for (int32 i = 0; i < (SIZEOF(name2) - 1); i += 1) {
            uint32 c = (uint32)rand() % (sizeof(characters) - 1);
            name2[i] = characters[c];
        }
        name2[SIZEOF(name2) - 1] = '\0';

        if ((fd = open(name2,
                       O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
            error("Error opening %s: %s.\n", name2, strerror(errno));
            fatal(EXIT_FAILURE);
        }

        util_filename_from(buffer2, sizeof(buffer2), fd);
        ASSERT_EQUAL(realpath(name2, buffer3), buffer2);
        XCLOSE(&fd);
        xunlink(name2);
    }

    ASSERT_EQUAL(deg2rad(180.0), 3.141592653589793);
    ASSERT_EQUAL(rad2deg(3.141592653589793), 180.0);
    ASSERT_MORE(util_nthreads(), 0);

    NCALLS(1);

    (void)util_segv_handler;
    (void)util_command;
    (void)util_die_notify;
#if OS_UNIX
    (void)util_copy_file_sync;
    (void)util_copy_file_async;
    (void)util_copy_file_async_thread;
#endif
    (void)util_command_launch;

    (void)malloc_debug;
    (void)realloc_debug;
    (void)free_debug;
    (void)free2_;

    (void)xmmap_commit;
    (void)xkill;
    (void)xdup2;
    (void)xpipe;
    (void)xunlink;

    (void)xpthread_mutex_lock;
    (void)xpthread_mutex_unlock;
    (void)xpthread_cond_destroy;
    (void)xpthread_mutex_destroy;
    (void)xpthread_create;
    (void)xpthread_join;

    (void)fwrite64;
    (void)fread64;

    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    PRINT_TIMINGS(1, t0, t1);
    exit(EXIT_SUCCESS);
}

#endif

#endif /* UTIL_C */
