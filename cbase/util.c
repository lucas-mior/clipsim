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

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

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
#include <dirent.h>
#include <ctype.h>

#include "platform_detection.h"

#include "generic.c"
#include "minmax.c"
#include "base_macros.h"
#include "assert.c"
#include "memory.c"
#include "utf8.c"

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_util 1
#elif !defined(TESTING_util)
#define TESTING_util 0
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#if !TESTING_util
static char *program;
#else
static char *program = __FILE__;
#endif
static int32 program_len UNUSED;

static bool timezone_initialized = false;
static time_t timezone_offset = 0;

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

#define CLAMP(VAR, VMIN, VMAX) \
_Generic((VAR),                \
    float: clamp_double,       \
    double: clamp_double,      \
    default: clamp_int64       \
)(VAR, VMIN, VMAX)

#define SQUARE(VAR)           \
_Generic((VAR),               \
    float: square_double,     \
    double: square_double,    \
    default: square_int64     \
)(VAR)

#define CLAMP_TYPE double
#include "clamp.h"

#define CLAMP_TYPE int64
#include "clamp.h"

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
static char *basename2(char *path, int32 *full_length, int32 *base_len);

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

INLINE void *
memrchr64(void *pointer, int32 value, int64 size) {
    uchar *buffer;
    uchar target;

    if (DEBUGGING) {
        if (size < 0) {
            error("Error: Invalid size = %lld\n", (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    if (size == 0) {
        return NULL;
    }

    buffer = pointer;
    target = (uchar)value;
    for (int64 i = size - 1; i >= 0; i -= 1) {
        if (buffer[i] == target) {
            return buffer + i;
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

#define MEMMEM_3(LONG, LONG_LEN, SHORT) \
        memmem64(LONG, LONG_LEN, SHORT, strlen32(SHORT))
#define MEMMEM_4(LONG, LONG_LEN, SHORT, LEN) \
        memmem64(LONG, LONG_LEN, SHORT, LEN)
#define MEMMEM(...) SELECT_ON_NUM_ARGS(MEMMEM_, __VA_ARGS__)

INLINE bool
strequal(char *s1, char *s2) {
    return !strcmp(s1, s2);
}

static bool
strequal2(char *a, int32 a_len, char *b, int32 b_len) {
    if (a_len != b_len) {
        return false;
    }
    if (memcmp64(a, b, a_len)) {
        return false;
    }

    return true;
}

#define strequal2_3(A, A_LEN, B) \
        strequal2(A, A_LEN, B, strlen32(B))
#define strequal2_4(A, A_LEN, B, B_LEN) \
        strequal2(A, A_LEN, B, B_LEN)
#define STREQUAL(...) SELECT_ON_NUM_ARGS(strequal2_, __VA_ARGS__)

INLINE void *
memchr64(void *pointer, int32 value, int64 size) {
    if (DEBUGGING) {
        if (size < 0) {
            error("Error: Invalid size = %lld\n", (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    if (size == 0) {
        return 0;
    }
    return memchr(pointer, value, (size_t)size);
}

static int32
optional_strlen32(char *string) {
    if (string == NULL) {
        return 0;
    }
    return strlen32(string);
}

static int32
strlen32(char *string) {
    int32 length;
    size_t len;

    ASSERT(string);
    len = strlen(string);

    if (DEBUGGING) {
        if (len >= MAXOF(length)) {
            error("Error: string (%.*s ...) is too long.\n", 50, string);
            fatal(EXIT_FAILURE);
        }
    }

    length = (int32)len;
    return length;
}

INLINE char *
strncpy32(char *dest, char *source, int64 space) {
    if (DEBUGGING) {
        if (space <= 0) {
            error("Error: string (%.*s ...) is too long.\n", 50, source);
            fatal(EXIT_FAILURE);
        }
        if ((ullong)space >= SIZE_MAX) {
            error("Error: space is too large.\n");
            fatal(EXIT_FAILURE);
        }
    }

    return strncpy(dest, source, (size_t)space);
}

INLINE int
strncmp32(char *left, char *right, int64 size) {
    int result;
    if (size == 0) {
        return 0;
    }
    if (DEBUGGING) {
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error: Size (%lld) is bigger than SIZEMAX\n", (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    result = strncmp(left, right, (size_t)size);
    return result;
}

INLINE char *
begins_with(char *string, int32 string_len, char *literal, int32 length) {
    if (string_len < length) {
        return NULL;
    }
    if (!memcmp64(string, literal, length)) {
        return string + length;
    } else {
        return NULL;
    }
}

#define BEGINS_WITH_3(STRING, STRING_LEN, PREFIX) \
        begins_with(STRING, STRING_LEN, PREFIX, strlen32(PREFIX))
#define BEGINS_WITH_4(STRING, STRING_LEN, PREFIX, PREFIX_LEN) \
        begins_with(STRING, STRING_LEN, PREFIX, PREFIX_LEN)
#define BEGINS_WITH(...) SELECT_ON_NUM_ARGS(BEGINS_WITH_, __VA_ARGS__)

INLINE char *
ends_with(char *string, int32 string_len, char *literal, int32 length) {
    if (string_len < length) {
        return NULL;
    }
    string += (string_len - length);
    if (!memcmp64(string, literal, length)) {
        return string;
    } else {
        return NULL;
    }
}

#define ENDS_WITH_3(STRING, STRING_LEN, SUFFIX) \
        ends_with(STRING, STRING_LEN, SUFFIX, strlen32(SUFFIX))
#define ENDS_WITH_4(STRING, STRING_LEN, SUFFIX, SUFFIX_LEN) \
        ends_with(STRING, STRING_LEN, SUFFIX, SUFFIX_LEN)
#define ENDS_WITH(...) SELECT_ON_NUM_ARGS(ENDS_WITH_, __VA_ARGS__)

INLINE int
memcmp64(void *left, void *right, int64 size) {
    if (size == 0) {
        return 0;
    }
    if (DEBUGGING) {
        if (size < 0) {
            error("Error: size=%lld < 0.\n", (llong)size);
        }
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error: Size (%lld) is bigger than SIZEMAX\n", (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    return memcmp(left, right, (size_t)size);
}

static char *
remove_escape_sequences(char *data, int32 *data_len) {
    int32 old_len = *data_len;
    int32 read_index = 0;
    int32 write_index = 0;

    while (read_index < old_len) {
        if (data[read_index] != '\033') {
            data[write_index++] = data[read_index++];
            continue;
        }

        read_index += 1;

        if (read_index >= old_len) {
            break;
        }

        if (data[read_index] == '[') {
            read_index += 1;

            while (read_index < old_len) {
                uchar c = (uchar)data[read_index++];

                if ((c >= 0x40) && (c <= 0x7e)) {
                    break;
                }
            }
        } else {
            read_index += 1;
        }
    }

    data[write_index] = '\0';
    *data_len = write_index;
    data = realloc2(data, old_len + 1, *data_len + 1, SIZEOF(*data));

    return data;
}

static int32
random_ascii_string(char *buffer, int32 capacity, int32 min_len) {
    int32 max_len = capacity - 1;
    int32 len = min_len;
    int32 range;

    if (capacity <= 0) {
        return 0;
    }

    if (len > max_len) {
        len = max_len;
    }

    range = max_len - len + 1;
    if (range > 1) {
        len = len + (rand() % range);
    }

    for (int32 i = 0; i < len; i += 1) {
        int32 ascii_val = 32 + (rand() % 95);
        buffer[i] = (char)ascii_val;
    }
    buffer[len] = '\0';

    return len;
}

static void
write_all(int fd, char *buffer, int64 left) {
    int64 written = 0;
    int64 w;

    while (left > 0) {
        if ((w = write(fd, buffer + written, (RW_TYPE)left)) <= 0) {
            fprintf(stderr, "Error writing: %s.\n", strerror(errno));
            if (errno == EINTR) {
                continue;
            }
            fatal(EXIT_FAILURE);
        }
        left -= w;
        written += w;
    }
    return;
}

#define RW_FUNCTION write
#include "rw_function.h"

#define RW_FUNCTION read
#include "rw_function.h"

static void
qsort64(void *base, int64 n, int64 size,
        int (*compar)(void *, void *)) {
    int (*compar_consted)(const void *, const void *);
    compar_consted = (int (*)(const void *, const void *)) compar;
    if (DEBUGGING) {
        if ((size <= 0) || (n <= 0)) {
            error("Error: Invalid size(%lld) or n(%lld)\n",
                  (llong)size, (llong)n);
            fatal(EXIT_FAILURE);
        }
        if ((size_t)size >= (SIZE_MAX / (size_t)n)) {
            error("Error: Overflow (%lld*%lld)\n", (llong)size, (llong)n);
            fatal(EXIT_FAILURE);
        }
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error: Size (%lld) is bigger than SIZEMAX\n", (llong)size);
            fatal(EXIT_FAILURE);
        }
        if ((ullong)n >= (ullong)SIZE_MAX) {
            error("Error: Number (%lld) is bigger than SIZEMAX\n", (llong)n);
            fatal(EXIT_FAILURE);
        }
    }
    qsort(base, (size_t)n, (size_t)size, compar_consted);
    return;
}

#if OS_WINDOWS
static int32
util_nthreads(void) {
    SYSTEM_INFO sysinfo = {0};
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
        error("Error locking mutex %p: %s.\n", (void *)mutex, strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

static void
xpthread_mutex_unlock(pthread_mutex_t *mutex) {
    int err;
    if ((err = pthread_mutex_unlock(mutex))) {
        error("Error unlocking mutex %p: %s.\n", (void *)mutex, strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

static void
xpthread_cond_destroy(pthread_cond_t *cond) {
    int err;
    if ((err = pthread_cond_destroy(cond))) {
        error("Error destroying cond %p: %s.\n", (void *)cond, strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

static void
xpthread_mutex_destroy(pthread_mutex_t *mutex) {
    int err;
    if ((err = pthread_mutex_destroy(mutex))) {
        error("Error destroying mutex %p: %s.\n", (void *)mutex, strerror(err));
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

    ASSERT_MORE_EQUAL(size, 0);
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
    ullong magnitude;
    int i = 0;
    bool negative = false;

    if (size < 22) {
        error("Error in itoa2: buffer is too small.\n");
        fatal(EXIT_FAILURE);
    }

    if (num < 0) {
        negative = true;
        magnitude = (ullong)(-(num + 1)) + 1;
    } else {
        magnitude = (ullong)num;
    }

    do {
        str[i] = (char)(magnitude % 10 + '0');
        i += 1;
        magnitude /= 10;
    } while (magnitude > 0);

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
    ASSERT(i < 22);

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
        error("Error in strftime(\"%s\") (n = %lld).\n", format, (llong)n);
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
    buffer[len] = '\0';
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

    ASSERT_MORE(size, 0);
    memcpy64(buffer, error_message, (llong)MIN(len + 1, size - 1));
    buffer[size - 1] = '\0';

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

    if (*fd < 0) {
        return 0;
    }

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

#define XCLOSE_1(FD) xclose(__FILE__, __LINE__, FD, #FD, NULL)
#define XCLOSE_2(FD, NAME) xclose(__FILE__, __LINE__, FD, #FD, NAME)
#define XCLOSE(...) SELECT_ON_NUM_ARGS(XCLOSE_, __VA_ARGS__)

static int
xunlink(char *filename) {
    if (unlink(filename) < 0) {
        error2("Error in unlink(%s): %s.\n", filename, strerror(errno));
        return -1;
    }
    return 0;
}

static FILE *
xfopen(char *file, int32 line, char *func, char *filename, char *mode) {
    FILE *f;
    char *mode_long = "what";

    if (strequal(mode, "w")) {
        mode_long = "writing";
    }
    if (strequal(mode, "r")) {
        mode_long = "reading";
    }
    if (strequal(mode, "r+")) {
        mode_long = "reading and writing";
    }
    if (strequal(mode, "w+")) {
        mode_long = "reading and writing";
    }
    if (strequal(mode, "a")) {
        mode_long = "appending";
    }
    if (strequal(mode, "a+")) {
        mode_long = "reading and appending";
    }

    if ((f = fopen(filename, mode)) == NULL) {
        error_impl(file, line, func, "Error opening %s for %s: %s.\n",
                   filename, mode_long, strerror(errno));
        return NULL;
    }
    return f;
}

#define XFOPEN(FILENAME, MODE) \
    xfopen(__FILE__, __LINE__, (char *)__func__, FILENAME, MODE)

static int
xfclose(char *file, int32 line, char *func, FILE *f, char *filename) {
    if (fclose(f)) {
        error_impl(file, line, func,
                   "Error closing %s: %s.\n", filename, strerror(errno));
        return -1;
    }
    return 0;
}

#define XFCLOSE(F, FILENAME) \
    xfclose(__FILE__, __LINE__, (char *)__func__, F, FILENAME)

static int
xclosedir(DIR *dir, char *dirname) {
    if (closedir(dir)) {
        error2("Error closing directory %s: %s.\n", dirname, strerror(errno));
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
        int64 exe_len = strlen32(exe);
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
#elif OS_UNIX
static int
util_command(int argc, char **argv) {
    pid_t child;
    int status;
    (void)argc;

    if (DEBUGGING) {
        char cmd[4096];
        STRING_FROM_ARRAY(cmd, " ", argv, argc);
        error("Executing\n%s\n", cmd);
    }

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
        _exit(2);
    case -1:
        error("Error forking: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    default:
        while (waitpid(child, &status, 0) < 0) {
            error("Error waiting for the forked child: %s.\n", strerror(errno));
            if (errno == EINTR) {
                continue;
            }
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
    char cmd[4096];
    (void)argc;

    switch (fork()) {
    case -1:
        error("Error forking: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    case 0:
        if (setsid() < 0) {
            error("Error in setsid: %s.\n", strerror(errno));
        }
        execvp(argv[0], argv);
        STRING_FROM_ARRAY(cmd, " ", argv, argc);
        error("\nError executing\n%s\n%s.", cmd, strerror(errno));
        _exit(EXIT_FAILURE);
    default:
        return 0;
    }
}
#endif

void __attribute__((format(printf, 4, 5)))
error_impl(char *file, int32 line, char *func, char *format, ...) {
    char buffer[BUFSIZ];
    char *big_buffer = NULL;
    char *pbuffer = buffer;
    va_list args;
    va_list args_copy;
    int32 n;
    int32 m = SIZEOF(buffer);
    int32 p;
    char fileline[256];
    char file2[4096];
    int32 file_len = strlen32(file);
    int32 base_len;

    if (file_len >= SIZEOF(file2)) {
        error2("File name is too long.\n");
        fatal(EXIT_FAILURE);
    }
    memcpy(file2, file, (size_t)(file_len + 1));

    file = basename2(file2, &file_len, &base_len);

    va_start(args, format);
    va_copy(args_copy, args);
    n = vsnprintf(buffer, (size_t)m, format, args_copy);
    va_end(args_copy);

    if (n >= m) {
        m = n + 1;
        big_buffer = xmalloc(m, false);
        n = vsnprintf(big_buffer, (size_t)m, format, args);
        pbuffer = big_buffer;
    }

    va_end(args);

    if ((n < 0) || (n >= m)) {
        fprintf(stderr,
                "%s:%d %s(): Error in vsnprintf(\"%s\") (n = %lld).\n",
                file, line, func, format, (llong)n);
        fatal(EXIT_FAILURE);
    }

    if (!RELEASING) {
        p = SNPRINTF(fileline, "%s:%d %s():", file, line, func);
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

void __attribute((noreturn))
fatal(int status) {
#if defined(__EMSCRIPTEN__)
    char stack[8192];
    int32 flags = EM_LOG_C_STACK
                  |EM_LOG_JS_STACK
                  |EM_LOG_DEMANGLE
                  |EM_LOG_NO_PATHS;

    emscripten_get_callstack(flags, stack, SIZEOF(stack));
    error2("fatal(%d) call stack:\n%s\n", status, stack);
#endif
    if (DEBUGGING) {
        (void)status;
        raise(SIGILL);
        exit(status);
    } else {
        exit(status);
    }
}

void
util_segv_handler(int32 unused) {
    char *message = "Memory error. Please send a bug report.\n";
    (void)unused;

    write64(STDERR_FILENO, message, strlen32(message));
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

    if ((source_fd = open(source, O_RDONLY)) < 0) {
        error("Error opening %s for reading: %s.\n", source, strerror(errno));
        return -1;
    }

    if ((*dest_fd = open(destination,
                         O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)) < 0) {
        error("Error opening %s for writing: %s.\n",
              destination, strerror(errno));
        XCLOSE(&source_fd, source);
        return -1;
    }

#if 0
    int32 fadvise_err;
    if ((fadvise_err = posix_fadvise(source_fd,
                                     0, 0,
                                     POSIX_FADV_WILLNEED)) < 0) {
        error("Error in posix_fadvise(POSIX_FADV_WILLNEED): %s.\n",
              strerror(fadvise_err));
    }
#endif

    return source_fd;
}

static void
util_copy_file_async_parsed(UtilCopyFilesAsync *copy_files) {
    struct pollfd *pipes = copy_files->pipes;
    int *dests = copy_files->dests;
    int32 left = copy_files->nfds;

    if (copy_files->nfds > LENGTH(copy_files->pipes)) {
        error("Error too many files for UtilCopyFilesAsync definition.\n");
        fatal(EXIT_FAILURE);
    }

    while (left > 0) {
        char buffer[BUFSIZ];
        int64 r;
        int64 w;
        int64 n;

        n = poll(pipes, (nfds_t)copy_files->nfds, 1000);
        if (n == 0) {
            continue;
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
    return;
}

static void *
util_copy_file_async_thread(void *arg) {
    UtilCopyFilesAsync *copy_files = arg;
    util_copy_file_async_parsed(copy_files);
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

#if !OS_WINDOWS
static bool
util_file_exists(char *filename) {
#if defined(O_PATH) && defined(O_NOFOLLOW)
    // this should be faster than lstat()
    {
        int32 fd;
        if (((fd = open(filename, O_PATH | O_NOFOLLOW)) < 0)
                && (errno == ENOENT)) {
            return false;
        } else {
            close(fd);
            return true;
        }
    }
#else
    {
        struct stat statbuf;
        if ((lstat(filename, &statbuf) < 0) && (errno == ENOENT)) {
            return false;
        } else {
            return true;
        }
    }
#endif
}
#else
static bool
util_file_exists(char *filename) {
    DWORD attributes;

    attributes = GetFileAttributesA(filename);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    return true;
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

    while ((*length >= 2)
           && (path[*length - 2] == '/')
           && (path[*length - 1] == '.')) {
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
    ASSERT_MORE(*full_length, 0);
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
        if ((uintptr_t)fslash > (uintptr_t)bslash) {
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

static char *
path_basename(char *path, int32 path_len) {
    int32 slash = -1;
    int32 start;

    for (int32 i = 0; i < path_len; i += 1) {
        if (path[i] == '/') {
            slash = i;
        }
    }

    start = slash + 1;
    return xstrndup(path + start, path_len - start);
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
    printf("%gs = %gus per item.\n", total_seconds, micros_per);
    return;
}

static double
timediff(struct timespec t0, struct timespec t1) {
    llong sec = t1.tv_sec - t0.tv_sec;
    llong nsec = t1.tv_nsec - t0.tv_nsec;
    double diff = (double)sec + (double)nsec*1e-9;
    return diff;
}

static void
catfile(int where, char *file) {
    int fd;
    char buffer[4096];
    int64 r;

    if ((fd = open(file, O_RDONLY)) < 0) {
        error("Error opening %s: %s.\n", file, strerror(errno));
        fatal(EXIT_FAILURE);
    }

    printf("\n");
    while ((r = read64(fd, buffer, SIZEOF(buffer))) > 0) {
        write_all(where, buffer, r);
    }
    if (r < 0) {
        error("Error reading %s: %s.\n", file, strerror(errno));
        fatal(EXIT_FAILURE);
    }

    XCLOSE(&fd, file);
    return;
}

#if !OS_WINDOWS

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

#endif /* !OS_WINDOWS */

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

#define GETENV(VAR) do { \
    if ((VAR = getenv(#VAR)) == NULL) { \
        if (DEBUGGING) { \
            error_impl(__FILE__, __LINE__, (char *)__func__, \
                       RED("%s") " is not defined.", #VAR); \
        } \
    } else { \
        int32 len = strlen32(VAR); \
        char *copy = malloc2(len + 1); \
        memcpy64(copy, VAR, len + 1); \
        VAR = copy; \
    } \
} while (0)

static char *
read_entire_file(char *path, int32 *file_len) {
    FILE *fp;
    int64 len;
    char *data;
    int64 r;

    if ((fp = fopen(path, "rb")) == NULL) {
        error("Error opening "RED("%s")" for reading: %s",
              path, strerror(errno));
        fatal(EXIT_FAILURE);
    }
    if (fseek(fp, 0, SEEK_END)) {
        error("Error seeking end of %s: %s.\n", path, strerror(errno));
        fclose(fp);
        fatal(EXIT_FAILURE);
    }
    if ((len = ftell(fp)) < 0) {
        error("Error in ftell(%s): %s.\n", path, strerror(errno));
        fclose(fp);
        fatal(EXIT_FAILURE);
    }
    if (fseek(fp, 0, SEEK_SET)) {
        error("Error rewinding %s: %s.\n", path, strerror(errno));
        fclose(fp);
        fatal(EXIT_FAILURE);
    }

    data = malloc2(len + 1);
    if (len > 0) {
        r = fread64(data, 1, len, fp);
        if (r != len) {
            error("Error reading "RED("%s")": %s.\n", path, strerror(errno));
            fatal(EXIT_FAILURE);
        }
    } else {
        r = 0;
    }
    data[r] = '\0';
    XFCLOSE(fp, path);

    if (r >= MAXOF(*file_len)) {
        error("Only files up to 2GB are supported.\n");
        fatal(EXIT_FAILURE);
    }
    *file_len = (int32)r;
    return data;
}

static void
write_entire_file(char *path, char *text, int64 text_len) {
    FILE *file;

    if (text_len < 0) {
        error("Error writing negative length %lld to %s.",
              (llong)text_len, path);
        fatal(EXIT_FAILURE);
    }

    if ((file = fopen(path, "wb")) == NULL) {
        error("Error opening %s for writing: %s", path, strerror(errno));
        fatal(EXIT_FAILURE);
    }

    if ((text_len > 0) && (fwrite64(text, 1, text_len, file) != text_len)) {
        error("Error writing %lld bytes to %s: %s.",
              (llong)text_len, path, strerror(errno));
        fatal(EXIT_FAILURE);
    }

    XFCLOSE(file, path);
    return;
}

#define STR_BUILDER_INITIAL_CAPACITY 16

static void
sb_init(StrBuilder *str_builder) {
    str_builder->data = NULL;
    str_builder->len = 0;
    str_builder->cap = 0;
    return;
}

static void
sb_free(StrBuilder *str_builder) {
    if (str_builder->data) {
        free2(str_builder->data, str_builder->cap);
    }

    sb_init(str_builder);
    return;
}

static void
sb_clear(StrBuilder *str_builder) {
    str_builder->len = 0;
    if (str_builder->data) {
        str_builder->data[0] = '\0';
    }
    return;
}

static bool
sb_copy(StrBuilder *dest, StrBuilder *source) {
    if (dest == NULL) {
        return false;
    }
    if (dest == source) {
        return true;
    }
    if (source == NULL) {
        sb_free(dest);
        return true;
    }

    sb_clear(dest);
    sb_append(dest, source->data, source->len);
    return true;
}

static void
sb_move(StrBuilder *dest, StrBuilder *source) {
    if (dest == NULL) {
        return;
    }
    if (dest == source) {
        return;
    }

    sb_free(dest);
    if (source == NULL) {
        sb_init(dest);
        return;
    }

    *dest = *source;
    sb_init(source);
    return;
}

static bool
sb_set(StrBuilder *str_builder, char *data, int32 data_len) {
    if (str_builder == NULL) {
        return false;
    }
    if (data_len < 0) {
        return false;
    }
    if ((data == NULL) && (data_len > 0)) {
        return false;
    }
    if ((data == str_builder->data) && str_builder->data) {
        if (data_len > str_builder->len) {
            return false;
        }
        str_builder->len = data_len;
        str_builder->data[data_len] = '\0';
        return true;
    }

    sb_clear(str_builder);
    sb_append(str_builder, data, data_len);
    return true;
}

static void
sb_reserve(StrBuilder *str_builder, int32 extra) {
    int64 needed;
    int64 new_cap;
    int32 old_cap;

    if (extra <= 0) {
        return;
    }

    needed = (int64)str_builder->len + extra + 1;
    if (str_builder->data && (needed <= str_builder->cap)) {
        return;
    }
    if (needed >= MAXOF(str_builder->cap)) {
        error("StrBuilder only supports strings shorter than 2GB.\n");
        fatal(EXIT_FAILURE);
    }

    old_cap = str_builder->cap;
    if (str_builder->data == NULL) {
        old_cap = 0;
    }

    new_cap = str_builder->cap;
    if (new_cap <= 0) {
        new_cap = STR_BUILDER_INITIAL_CAPACITY;
    }
    while (new_cap < needed) {
        new_cap *= 2;
    }
    if (new_cap >= MAXOF(str_builder->cap)) {
        new_cap = needed;
    }

    str_builder->data = realloc2(str_builder->data, old_cap, new_cap,
                                 SIZEOF(*str_builder->data));
    str_builder->cap = (int32)new_cap;
    return;
}

static void
sb_append(StrBuilder *str_builder, char *data, int32 data_len) {
    bool aliases = false;
    int32 data_offset = 0;

    if (data_len <= 0) {
        return;
    }

    if (data == str_builder->data) {
        aliases = true;
    } else if (str_builder->data) {
        uintptr_t data_address = (uintptr_t)data;
        uintptr_t start = (uintptr_t)str_builder->data;

        if (data_address >= start) {
            uintptr_t offset = data_address - start;

            if (offset < (uint32)str_builder->cap) {
                aliases = true;
                data_offset = (int32)offset;
            }
        }
    }

    sb_reserve(str_builder, data_len);
    if (aliases) {
        data = str_builder->data + data_offset;
        memmove64(str_builder->data + str_builder->len, data, data_len);
    } else {
        memcpy64(str_builder->data + str_builder->len, data, data_len);
    }
    str_builder->len += data_len;
    str_builder->data[str_builder->len] = '\0';

    return;
}

static void
sb_append_byte(StrBuilder *str_builder, char byte) {
    sb_reserve(str_builder, 1);
    str_builder->data[str_builder->len] = byte;
    str_builder->len += 1;
    str_builder->data[str_builder->len] = '\0';
    return;
}

#define SB_APPEND_2(BUILER, STRING) \
        sb_append(BUILER, STRING, strlen32(STRING))
#define SB_APPEND_3(BUILER, STRING, LEN) \
        sb_append(BUILER, STRING, (int32)LEN)
#define SB_APPEND(...) SELECT_ON_NUM_ARGS(SB_APPEND_, __VA_ARGS__)

static void
sb_printf(StrBuilder *str_builder, char *fmt, ...) {
    va_list ap;
    va_list ap2;
    int32 n;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0) {
        va_end(ap2);
        error("Error formatting \"%s\".", fmt);
        fatal(EXIT_FAILURE);
    }
    if (n == 0) {
        va_end(ap2);
        return;
    }

    sb_reserve(str_builder, n);
    vsnprintf(str_builder->data + str_builder->len, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    str_builder->len += n;
    return;
}

static char *
sb_steal(StrBuilder *str_builder, int32 *len, int32 *cap) {
    char *data = str_builder->data;

    if (len) {
        *len = str_builder->len;
    }
    if (cap) {
        *cap = str_builder->cap;
    }

    sb_init(str_builder);
    return data;
}

static char *
sb_steal_exact(StrBuilder *str_builder, int32 *len) {
    char *data;
    int32 data_len;
    int32 cap;

    data = sb_steal(str_builder, &data_len, &cap);
    if (cap != data_len + 1) {
        data = realloc2(data, cap, data_len + 1, SIZEOF(*data));
    }
    data[data_len] = '\0';

    if (len) {
        *len = data_len;
    }
    return data;
}

static void
str_builder_array_init(StrBuilderArray *array) {
    array->items = NULL;
    array->len = 0;
    array->cap = 0;
    return;
}

static void
str_builder_array_clear(StrBuilderArray *array) {
    if (array == NULL) {
        return;
    }

    for (int32 i = 0; i < array->len; i += 1) {
        sb_free(&array->items[i]);
    }
    array->len = 0;
    return;
}

static void
str_builder_array_destroy(StrBuilderArray *array) {
    if (array == NULL) {
        return;
    }

    str_builder_array_clear(array);
    if (array->items) {
        free2(array->items, array->cap*SIZEOF(*array->items));
    }
    str_builder_array_init(array);
    return;
}

static bool
str_builder_array_copy(StrBuilderArray *dest, StrBuilderArray *source) {
    StrBuilderArray replacement;

    if (dest == NULL) {
        return false;
    }
    if (dest == source) {
        return true;
    }

    str_builder_array_init(&replacement);
    if (source) {
        if (!str_builder_array_reserve(&replacement, source->len)) {
            str_builder_array_destroy(&replacement);
            return false;
        }
        for (int32 i = 0; i < source->len; i += 1) {
            if (!str_builder_array_append_copy(&replacement,
                                               &source->items[i])) {
                str_builder_array_destroy(&replacement);
                return false;
            }
        }
    }

    str_builder_array_destroy(dest);
    *dest = replacement;
    return true;
}

static void
str_builder_array_move(StrBuilderArray *dest, StrBuilderArray *source) {
    if (dest == NULL) {
        return;
    }
    if (dest == source) {
        return;
    }

    str_builder_array_destroy(dest);
    if (source == NULL) {
        str_builder_array_init(dest);
        return;
    }
    *dest = *source;
    str_builder_array_init(source);
    return;
}

static void
str_builder_array_swap(StrBuilderArray *left, StrBuilderArray *right) {
    StrBuilderArray temp;

    if (left == NULL) {
        return;
    }
    if (right == NULL) {
        return;
    }

    temp = *left;
    *left = *right;
    *right = temp;
    return;
}

static bool
str_builder_array_reserve(StrBuilderArray *array, int32 extra) {
    int64 needed;
    int32 old_cap;
    int32 new_cap;

    if (array == NULL) {
        return false;
    }
    if (extra <= 0) {
        return true;
    }

    needed = (int64)array->len + extra;
    if (needed <= array->cap) {
        return true;
    }
    if (needed >= MAXOF(array->cap)) {
        error("StrBuilderArray only supports fewer than 2GB items.\n");
        fatal(EXIT_FAILURE);
    }

    old_cap = array->cap;
    new_cap = array->cap;
    if (new_cap <= 0) {
        new_cap = 8;
    }

    if (needed >= (MAXOF(new_cap)/2)) {
        new_cap = (int32)needed;
    } else {
        while (new_cap < needed) {
            new_cap *= 2;
        }
    }

    array->items = realloc2(array->items, old_cap, new_cap,
                            SIZEOF(*array->items));
    array->cap = new_cap;
    return true;
}

static StrBuilder *
str_builder_array_append(StrBuilderArray *array) {
    StrBuilder *item;

    if (!str_builder_array_reserve(array, 1)) {
        return NULL;
    }

    item = &array->items[array->len];
    array->len += 1;
    sb_init(item);
    return item;
}

static bool
str_builder_array_append_copy(StrBuilderArray *array, StrBuilder *item) {
    StrBuilder *dest;

    if (item == NULL) {
        return false;
    }

    dest = str_builder_array_append(array);
    if (dest == NULL) {
        return false;
    }
    if (!sb_copy(dest, item)) {
        array->len -= 1;
        sb_free(dest);
        return false;
    }
    return true;
}

static bool
parse_option(char **parsed, char *arg, char *option_name) {
    char name_equal[256];
    char *tmp;
    int32 length = SNPRINTF(name_equal, "%s=", option_name);
    int32 arg_len;
    if (arg == NULL) {
        return false;
    }
    arg_len = strlen32(arg);

    if ((tmp = BEGINS_WITH(arg, arg_len, name_equal, length))) {
        *parsed = tmp;
        return true;
    }
    return false;
}

static bool
is_ident_start_char(char c) {
    return isalpha((uint8)c) || c == '_';
}

static bool
is_ident_char(char c) {
    return isalnum((uint8)c) || c == '_';
}

static void
warn(char *fmt, ...) {
    va_list ap;

    fprintf(stderr, "%s: "RED ("warning:"), program);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    return;
}

typedef struct Command {
    char **argv;
    int32 *argvs_lens;
    int32 argc;
    int32 cap;
} Command;

typedef struct CommandResult {
    char *output;
    int32 output_len;
    int32 status;
} CommandResult;

static void
command_print(Command *command) {
    printf(RED("%s"), command->argv[0]);
    for (int32 i = 1; i < command->argc; i += 1) {
        printf(" %s", command->argv[i]);
    }
    printf("\n");
    return;
}

static char *
command_str(Command *command, int32 *len) {
    char buffer[4096];
    *len = STRING_FROM_ARRAY(buffer, " ", command->argv, command->argc);
    return xmemdup(buffer, *len + 1);
}

#if OS_UNIX
static bool
command_run_sync(Command *command, int *exit_status) {
    pid_t child;
    int32 len;
    int status;

    switch (child = fork()) {
    case -1:
        error("Error forking: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    case 0:
        execvp(command->argv[0], command->argv);
        error("Error executing "RED("%s")": %s.\n",
              command_str(command, &len), strerror(errno));
        _exit(EXIT_FAILURE);
    default:
        while (waitpid(child, &status, 0) < 0) {
            if (errno != EINTR) {
                error("Error waiting for child: %s.\n", strerror(errno));
                return false;
            }
        }
    }

    if (exit_status) {
        if (WIFEXITED(status)) {
            *exit_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            *exit_status = 128 + WTERMSIG(status);
        } else {
            *exit_status = 127;
        }
    }

    return true;
}

static CommandResult
command_run_capture(Command *command, char *cwd) {
    int32 pipefd[2];
    pid_t pid;
    char *output;
    int64 len = 0;
    int64 cap = 4096;
    int32 status = 127;
    CommandResult result;

    xpipe(pipefd);

    switch (pid = fork()) {
    case -1:
        XCLOSE(&pipefd[0]);
        XCLOSE(&pipefd[1]);
        error("Error forking: %s", strerror(errno));
        fatal(EXIT_FAILURE);
    case 0:
        XCLOSE(&pipefd[0]);

        if (cwd && chdir(cwd) != 0) {
            perror("chdir");
            _exit(127);
        }

        xdup2(pipefd[1], STDOUT_FILENO);
        xdup2(pipefd[1], STDERR_FILENO);
        XCLOSE(&pipefd[1]);

        execvp(command->argv[0], command->argv);
        error("Error executing %s: %s.\n", command->argv[0], strerror(errno));
        _exit(127);
    default:
        XCLOSE(&pipefd[1]);
        break;
    }

    output = malloc2(cap);
    for (;;) {
        int64 nread;

        if (len >= (cap/2)) {
            int64 old_cap = cap;

            cap *= 2;
            output = realloc2(output, old_cap, cap, SIZEOF(*output));
        }

        if ((nread = read64(pipefd[0], output + len, cap - len - 1)) > 0) {
            len += nread;
            continue;
        }
        if (nread == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }

        free2(output, cap);
        XCLOSE(&pipefd[0]);
        error("read from child failed: %s", strerror(errno));
        fatal(EXIT_FAILURE);
    }
    output[len] = '\0';
    if (len + 1 != cap) {
        output = realloc2(output, cap, len + 1, SIZEOF(output[0]));
    }
    XCLOSE(&pipefd[0]);

    while (waitpid(pid, &status, 0) < 0) {
        free2(output, len + 1);
        error("Error waiting for child: %s", strerror(errno));
        if (errno == EINTR) {
            continue;
        }
        fatal(EXIT_FAILURE);
    }

    if (WIFEXITED(status)) {
        status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        status = 128 + WTERMSIG(status);
    } else {
        status = 127;
    }

    if (len >= MAXOF(result.output_len)) {
        error("Output is too long.\n");
        fatal(EXIT_FAILURE);
    }

    return (CommandResult){
        .output = output,
        .output_len = (int32)len,
        .status = status,
    };
}
#endif

static void
command_result_free(CommandResult *result) {
    if (result->output) {
        free2(result->output, result->output_len + 1);
    }

    result->output = NULL;
    result->output_len = 0;
    result->status = 0;

    return;
}

static void
command_push(Command *command, char *argument) {

    if (command->cap <= command->argc + 1) {
        int32 oldcap = command->cap;

        command->cap += 16;
        command->argv = realloc2(command->argv,
                                 oldcap, command->cap,
                                 SIZEOF(*command->argv));
    }
    command->argv[command->argc++] = xstrdup(argument);
    command->argv[command->argc] = NULL;
    return;
}

static void
command_argv0_set(Command *command, char *argument) {
    free2(command->argv[0], strlen32(command->argv[0]) + 1);
    command->argv[0] = xstrdup(argument);
    return;
}

static void
command_reset(Command *command) {
    for (int32 i = 0; i < command->argc; i += 1) {
        free2(command->argv[i], strlen32(command->argv[i]) + 1);
    }
    command->argc = 0;
    return;
}

static void
command_free(Command *command) {
    command_reset(command);
    free2(command->argv, command->cap*SIZEOF(*command->argv));
    return;
}

static void
command_printf(Command *command, char *fmt, ...) {
    va_list ap;
    va_list ap2;
    int32 n;
    char *argument;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0) {
        va_end(ap2);
        error("Error formatting \"%s\".", fmt);
        fatal(EXIT_FAILURE);
    }

    argument = malloc2(n + 1);
    vsnprintf(argument, (size_t)n + 1, fmt, ap2);
    va_end(ap2);

    command_push(command, argument);

    free2(argument, n + 1);

    return;
}

#define PARSE_OPTION(arg, name) \
    if (parse_option(&name, arg, #name)) { \
        continue; \
    }

#if 0 == TESTING_util
static inline void
util_functions_sink(void) {
    (void)here_counter;
    (void)strequal;
    (void)read_entire_file;
    (void)write_entire_file;
    (void)sb_printf;
    (void)command_result_free;
    (void)command_argv0_set;
    (void)command_free;
    (void)command_printf;
    (void)util_segv_handler;
    (void)util_nthreads;
    (void)util_filename_from;
    (void)util_string_int32;
    (void)util_die_notify;
    (void)remove_escape_sequences;
    (void)xfclose;
    (void)xclosedir;
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
    (void)command_run_capture;
    (void)command_run_sync;
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
    (void)strncmp32;

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

    (void)random_ascii_string;
    (void)strncpy32;
    (void)ends_with;
#if OS_WINDOWS || OS_UNIX
    (void)util_command;
#endif
    (void)rad2deg;
    (void)deg2rad;
    (void)path_basename;
    (void)timediff;
    (void)catfile;
    (void)parse_option;
    (void)command_print;
    return;
}
#endif

#if TESTING_util

#define ENUM_NAME WeekDay
#define ENUM_BITFLAGS 0
#define ENUM_PREFIX_ WEEK_DAY_
#define ENUM_FIELDS \
    X(WEEK_DAY_SUNDAY, 0)          \
    X(WEEK_DAY_MONDAY)             \
    X(WEEK_DAY_TUESDAY, 10)        \
    X(WEEK_DAY_WEDNESDAY)          \
    X(WEEK_DAY_THURSDAY)           \
    X(WEEK_DAY_FRIDAY, 5)          \
    X(WEEK_DAY_SATURDAY, 20)
#include "xenums.c"

#define ENUM_NAME PowerOfTwo
#define ENUM_BITFLAGS 1
#define ENUM_PREFIX_ POWER_OF2_
#define ENUM_FIELDS \
    X(POWER_OF2_ONE)     \
    X(POWER_OF2_TWO)     \
    X(POWER_OF2_FOUR)    \
    X(POWER_OF2_EIGHT)   \
    X(POWER_OF2_SIXTEEN) \
    X(POWER_OF2_THIRTY2)
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
util_test_qsort_cmp(void *a, void *b) {
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

    ASSERT(BEGINS_WITH(s1, strlen32(s1), "aaaa"));
    ASSERT(BEGINS_WITH(s1, strlen32(s1), "aaaabbbb"));
    ASSERT(!BEGINS_WITH(s1, strlen32(s1), "bbbb"));
    ASSERT(!BEGINS_WITH(s1, strlen32(s1), "aaaabbbbb"));

    ASSERT(ENDS_WITH(s1, strlen32(s1), "bbbb"));
    ASSERT(ENDS_WITH(s1, strlen32(s1), "aaaabbbb"));
    ASSERT(!ENDS_WITH(s1, strlen32(s1), "aaaa"));
    ASSERT(!ENDS_WITH(s1, strlen32(s1), "aaaaabbbbb"));

    {
        StrBuilder builder = {0};
        int32 old_cap;

        SB_APPEND(&builder, "0123456789abcde");
        old_cap = builder.cap;
        sb_append(&builder, builder.data + 1, builder.len - 1);
        ASSERT_MORE(builder.cap, old_cap);
        ASSERT_EQUAL(builder.data,
                     "0123456789abcde123456789abcde");
        sb_free(&builder);
    }

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
            int32 path_len0 = strlen32(paths[i]);
            char *path = xstrdup(paths[i]);
            char *base = bases[i];
            int32 path_len = strlen32(path);
            ASSERT_EQUAL(basename2(path, &path_len, NULL), base);
            free2(path, path_len0 + 1);
        }
        for (int64 i = 0; i < LENGTH(paths); i += 1) {
            char *copy = xstrdup(paths[i]);
            int len = strlen32(copy);
            int len0 = strlen32(copy);
            normalize(copy, &len);
            ASSERT_EQUAL(copy, normalized[i]);
            free2(copy, len0 + 1);
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

    ASSERT_EQUAL(CLAMP(0.0, -0.1, 0.1),   0.0);
    ASSERT_EQUAL(CLAMP(0.2, -0.1, 0.1),   0.1);
    ASSERT_EQUAL(CLAMP(-0.2, -0.1, 0.1), -0.1);

    ASSERT_EQUAL(CLAMP(+0, -1, +1), +0);
    ASSERT_EQUAL(CLAMP(+2, -1, +1), +1);
    ASSERT_EQUAL(CLAMP(-2, -1, +1), -1);

    {
        Command cmd = {0};
        CommandResult result;
        int32 len;

        command_push(&cmd, "echo");
        command_printf(&cmd, "--val=%d", 123);
        command_push(&cmd, "test");

        ASSERT_EQUAL(cmd.argc, 3);
        ASSERT_EQUAL(cmd.argv[0], "echo");
        ASSERT_EQUAL(cmd.argv[1], "--val=123");
        ASSERT_EQUAL(cmd.argv[2], "test");
        ASSERT_EQUAL(command_str(&cmd, &len), "echo --val=123 test");
        command_print(&cmd);

        command_reset(&cmd);
        ASSERT_EQUAL(cmd.argc, 0);

        command_push(&cmd, "sh");
        command_push(&cmd, "-c");
        command_push(&cmd, "printf stdout; printf stderr >&2; exit 7");
        result = command_run_capture(&cmd, NULL);
        ASSERT_EQUAL(result.output, "stdoutstderr");
        ASSERT_EQUAL(result.output_len, 12);
        ASSERT_EQUAL(result.status, 7);
        command_result_free(&result);
        ASSERT_EQUAL(result.output, NULL);
        ASSERT_EQUAL(result.output_len, 0);
        ASSERT_EQUAL(result.status, 0);

        command_reset(&cmd);
        ASSERT_EQUAL(cmd.argc, 0);

        if (cmd.cap > 0) {
            free2(cmd.argv, cmd.cap * SIZEOF(*cmd.argv));
        }
    }

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
    (void)program_len;

    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    PRINT_TIMINGS(1, t0, t1);
    exit(EXIT_SUCCESS);
}

#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif /* UTIL_C */
