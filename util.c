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
#include <assert.h>
#include <float.h>

#include "generic.c"

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
#include <poll.h>
#endif

#if !defined(DEBUGGING)
#define DEBUGGING 0
#endif

#if !defined(ERROR_NOTIFY)
#define ERROR_NOTIFY 0
#endif

#if defined(__has_include) && __has_include(<valgrind/valgrind.h>)
#include <valgrind/valgrind.h>
#else
#define RUNNING_ON_VALGRIND 0
#endif

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_util 1
#elif !defined(TESTING_util)
#define TESTING_util 0
#endif

#if TESTING_util
static char *program = __FILE__;
#else
static char *program;
#endif

static void __attribute__((format(printf, 1, 2))) error(char *format, ...);

#define SIZEOF(X) ((int64)sizeof(X))

#if !defined(SIZEKB)
#define SIZEKB(X) ((int64)(X)*1024ll)
#define SIZEMB(X) ((int64)(X)*1024ll*1024ll)
#define SIZEGB(X) ((int64)(X)*1024ll*1024ll*1024ll)
#endif

#if !defined(LENGTH)
#define LENGTH(x) (int64)((sizeof(x) / sizeof(*x)))
#endif
#if !defined(SNPRINTF)
#define SNPRINTF(BUFFER, FORMAT, ...)                                          \
    snprintf2(BUFFER, sizeof(BUFFER), FORMAT, __VA_ARGS__)
#endif

#define STRING_FROM_ARRAY(BUFFER, SEP, ARRAY, LENGTH) \
_Generic((ARRAY), \
    double *: string_from_doubles, \
    char **: string_from_strings \
)(BUFFER, sizeof(BUFFER), SEP, ARRAY, LENGTH)

#if !defined(DEBUGGING)
#define DEBUGGING 0
#endif

#include "assert.c"

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

#if !defined(INLINE)
#if defined(__GNUC__)
#define INLINE static inline __attribute__((always_inline))
#else
#define INLINE static inline
#endif
#endif

#if DEBUGGING || TESTING_util
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wc11-extensions"
#pragma clang diagnostic ignored "-Wformat"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#endif

// clang-format off

#endif

#define UTIL_ALIGN_UINT(SIZE, A) (int64)(((SIZE) + ((A) - 1)) & ~((A) - 1))

#define UTIL_ALIGN(SIZE, A) \
_Generic((SIZE), \
    ullong: UTIL_ALIGN_UINT((ullong)SIZE, (ullong)A), \
    ulong:  UTIL_ALIGN_UINT((ulong)SIZE,  (ulong)A),  \
    uint:   UTIL_ALIGN_UINT((uint)SIZE,   (uint)A),   \
    llong:  UTIL_ALIGN_UINT((ullong)SIZE, (ullong)A), \
    long:   UTIL_ALIGN_UINT((ulong)SIZE,  (ulong)A),  \
    int:    UTIL_ALIGN_UINT((uint)SIZE,   (uint)A)   \
)

#if !defined(ALIGNMENT)
#define ALIGNMENT 16ul
#endif
#if !defined(ALIGN)
#define ALIGN(x) UTIL_ALIGN(x, ALIGNMENT)
#endif

#if !defined(MIN)
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

// clang-format on

static char *notifiers[2] = {"dunstify", "notify-send"};
static int64 util_page_size = 0;

static void error(char *, ...);
static void fatal(int) __attribute__((noreturn));
static void util_segv_handler(int32) __attribute__((noreturn));
static char *itoa2(long, char *);
static long atoi2(char *);
INLINE void *memchr64(void *pointer, int32 value, int64 size);
static int xclose(int *fd, char *filename);

#if !defined(CAT)
#define CAT_(a, b) a##b
#define CAT(a, b) CAT_(a, b)
#endif

#define NUM_ARGS_(_1, _2, _3, _4, _5, _6, _7, _8, n, ...) n
#define NUM_ARGS(...) NUM_ARGS_(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1, x)
#define SELECT_ON_NUM_ARGS(macro, ...) \
    CAT(macro, NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)

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

        if (memcmp(p, n, needle_len) == 0) {
            return (void *)p;
        }
        h = p + 1;
    }

    return NULL;
}
#endif

#define X64(func) \
INLINE void \
CAT(func, 64)(void *dest, void *source, int64 size) { \
    if (size == 0) \
        return; \
    if (DEBUGGING) { \
        if (size < 0) { \
            error("Error in %s: Invalid size = %lld\n", __func__, (llong)size); \
            fatal(EXIT_FAILURE); \
        } \
        if ((ullong)size >= (ullong)SIZE_MAX) { \
            error("Error in %s: Size (%lld) is bigger than SIZEMAX\n", \
                   __func__, (llong)size); \
            fatal(EXIT_FAILURE); \
        } \
    } \
    func(dest, source, (size_t)size); \
    return; \
}

X64(memcpy)
X64(memmove)
#undef X64

INLINE void
memset64(void *buffer, int value, int64 size) {
    if (size == 0) {
        return;
    }
    if (DEBUGGING) {
        if (size < 0) {
            error("Error in %s: Invalid size = %lld\n", __func__, (llong)size);
            fatal(EXIT_FAILURE);
        }
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error in %s: Size (%lld) is bigger than SIZEMAX\n", __func__,
                  (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    memset(buffer, value, (size_t)size);
    return;
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
        if (size <= 0) {
            error("Error in %s: Invalid size = %lld\n", __func__, (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    return memchr(pointer, value, (size_t)size);
}

INLINE int64
strlen64(char *string) {
    size_t len = strlen(string);
    return (int64)len;
}

INLINE int64
strnlen64(char *string, int64 size) {
    size_t len;
    if (DEBUGGING) {
        if (size <= 0) {
            error("Error in %s: Invalid size = %lld\n", __func__, (llong)size);
            fatal(EXIT_FAILURE);
        }
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error in %s: Size (%lld) is bigger than SIZEMAX\n", __func__,
                  (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    len = strnlen(string, (size_t)size);
    return (int64)len;
}

INLINE int
strncmp64(char *left, char *right, int64 size) {
    int result;
    if (size == 0) {
        return 0;
    }
    if (DEBUGGING) {
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error in %s: Size (%lld) is bigger than SIZEMAX\n", __func__,
                  (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    result = strncmp(left, right, (size_t)size);
    return result;
}

INLINE int
memcmp64(void *left, void *right, int64 size) {
    int result;
    if (size == 0) {
        return 0;
    }
    if (DEBUGGING) {
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error in %s: Size (%lld) is bigger than SIZEMAX\n", __func__,
                  (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    result = memcmp(left, right, (size_t)size);
    return result;
}

#define X64(func, TYPE) \
INLINE int64 \
CAT(func, 64)(int fd, void *buffer, int64 size) { \
    TYPE instance; \
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
              (llong)size, #func); \
        fatal(EXIT_FAILURE); \
    } \
    w = func(fd, buffer, (TYPE)size); \
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

#define X64(func) \
INLINE int64 \
CAT(func, 64)(void *buffer, int64 size, int64 n, FILE *file) { \
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
    rw = func(buffer, (size_t)size, (size_t)n, file); \
    return (int64)rw; \
}

X64(fwrite)
X64(fread)

#undef X64

#if OS_WINDOWS
static uint32
util_nthreads(void) {
    SYSTEM_INFO sysinfo;
    memset64(&sysinfo, 0, SIZEOF(sysinfo));
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
}
#else
static uint32
util_nthreads(void) {
    return (uint32)sysconf(_SC_NPROCESSORS_ONLN);
}
#endif

#if OS_WINDOWS || OS_MAC
#define basename basename2
#endif

static char *
basename2(char *path) {
    int64 left = strlen64(path);
    char *fslash = NULL;
    char *bslash = NULL;
    char *p = path;

    while (left > 0) {
        int64 length;

        fslash = memchr64(p, '/', left);
        if (OS_WINDOWS) {
            bslash = memchr64(p, '\\', left);
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

INLINE void *
xmalloc(int64 size) {
    void *p;

    if (DEBUGGING) {
        if (size <= 0) {
            error("Error in xmalloc: invalid size = %lld.\n", (llong)size);
            fatal(EXIT_FAILURE);
        }
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error in xmalloc: Number (%lld) is bigger than SIZEMAX\n",
                  (llong)size);
            fatal(EXIT_FAILURE);
        }
    }

    if ((p = malloc((size_t)size)) == NULL) {
        error("Failed to allocate %lld bytes.\n", (llong)size);
        fatal(EXIT_FAILURE);
    }

    if (DEBUGGING && !RUNNING_ON_VALGRIND) {
        memset64(p, 0xCD, size);
    }
    return p;
}

INLINE void *
xrealloc(void *old, int64 size) {
    void *p;
    uint64 old_save = (uint64)old;

    if (DEBUGGING) {
        if (size <= 0) {
            error("Error in xrealloc: invalid size = %lld.\n", (long long)size);
            fatal(EXIT_FAILURE);
        }
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error in xrealloc: Number (%lld) is bigger than SIZEMAX\n",
                  (llong)size);
            fatal(EXIT_FAILURE);
        }
    }

    if ((p = realloc(old, (size_t)size)) == NULL) {
        error("Failed to reallocate %lld bytes from %llx.\n", (llong)size,
              (ullong)old_save);
        fatal(EXIT_FAILURE);
    }

    return p;
}

static void *
xcalloc(size_t nmemb, size_t size) {
    void *p;
    if ((p = calloc(nmemb, size)) == NULL) {
        error("Error allocating %zu members of %zu bytes each.\n", nmemb, size);
        fatal(EXIT_FAILURE);
    }
    return p;
}

static char *
xstrdup(char *string) {
    char *p;
    int64 length;

    length = strlen64(string) + 1;
    if ((p = malloc((size_t)length)) == NULL) {
        error("Error allocating %lld bytes to duplicate '%s': %s\n",
              (llong)length, string, strerror(errno));
        fatal(EXIT_FAILURE);
    }

    memcpy64(p, string, length);
    return p;
}

#if OS_UNIX
static void *
xmmap_commit(int64 *size) {
    void *p;

    if (RUNNING_ON_VALGRIND) {
        p = xmalloc(*size);
        memset64(p, 0, *size);
        return p;
    }
    if (util_page_size == 0) {
        long aux;
        if ((aux = sysconf(_SC_PAGESIZE)) <= 0) {
            fprintf(stderr, "Error getting page size: %s.\n", strerror(errno));
            fatal(EXIT_FAILURE);
        }
        util_page_size = aux;
    }

    do {
        if ((*size >= SIZEMB(2)) && FLAGS_HUGE_PAGES) {
            p = mmap(NULL, (size_t)*size, PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE
                         | FLAGS_HUGE_PAGES,
                     -1, 0);
            if (p != MAP_FAILED) {
                *size = UTIL_ALIGN(*size, SIZEMB(2));
                break;
            }
        }
        p = mmap(NULL, (size_t)*size, PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
        *size = UTIL_ALIGN(*size, util_page_size);
    } while (0);
    if (p == MAP_FAILED) {
        error("Error in mmap(%lld): %s.\n", (llong)*size, strerror(errno));
        fatal(EXIT_FAILURE);
    }
    return p;
}
static void
xmunmap(void *p, int64 size) {
    if (RUNNING_ON_VALGRIND) {
        free(p);
        return;
    }
    if (munmap(p, (size_t)size) < 0) {
        error("Error in munmap(%p, %lld): %s.\n", p, (llong)size,
              strerror(errno));
    }
    return;
}
#else
static void *
xmmap_commit(int64 *size) {
    void *p;

    if (RUNNING_ON_VALGRIND) {
        p = xmalloc(*size);
        memset64(p, 0, *size);
        return p;
    }
    if (util_page_size == 0) {
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        util_page_size = system_info.dwPageSize;
        if (util_page_size <= 0) {
            fprintf(stderr, "Error getting page size.\n");
            fatal(EXIT_FAILURE);
        }
    }

    p = VirtualAlloc(NULL, (size_t)*size, MEM_COMMIT | MEM_RESERVE,
                     PAGE_READWRITE);
    if (p == NULL) {
        fprintf(stderr, "Error in VirtualAlloc(%lld): %lu.\n", (llong)*size,
                GetLastError());
        fatal(EXIT_FAILURE);
    }
    return p;
}
static void
xmunmap(void *p, size_t size) {
    (void)size;
    if (RUNNING_ON_VALGRIND) {
        free(p);
        return;
    }
    if (!VirtualFree(p, 0, MEM_RELEASE)) {
        fprintf(stderr, "Error in VirtualFree(%p): %lu.\n", p, GetLastError());
    }
    return;
}
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
xpthread_join(pthread_t thread, void **thread_return) {
    int err;
    if ((err = pthread_join(thread, thread_return))) {
        error("Error joining thread: %s.\n", strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

static int32 __attribute__((format(printf, 3, 4)))
snprintf2(char *buffer, int64 size, char *format, ...) {
    int n;
    va_list args;

    va_start(args, format);
    n = vsnprintf(buffer, (size_t)size, format, args);
    va_end(args);

    if (n <= 0) {
        error("Error in snprintf %s.\n", format);
        fatal(EXIT_FAILURE);
    }
    if (n >= size) {
        error("Error in snprintf %s: Buffer is too small.\n", format);
        fatal(EXIT_FAILURE);
    }
    return n;
}

static void
util_filename_from(char *buffer, int64 size, int fd) {
    char *unknown = "<unknown filename>";
    int64 unknown_len = strlen64(unknown);
#if OS_LINUX
    char linkpath[64];
    ssize_t len;

    SNPRINTF(linkpath, "/proc/self/fd/%d", fd);
    if ((len = readlink(linkpath, buffer, (size_t)(size - 1))) < 0) {
        memcpy64(buffer, unknown, unknown_len + 1);
        return;
    }
    buffer[len] = '\0';
    return;
#elif OS_MAC
    static char buffer2[PATH_MAX];
    int64 len;

    if (fcntl(fd, F_GETPATH, buffer2) < 0) {
        memcpy64(buffer, unknown, unknown_len + 1);
        return;
    }
    len = MIN(strlen64(buffer2), size - 1);
    memcpy64(buffer, buffer2, len + 1);
    return;
#elif OS_WINDOWS
    HANDLE h;
    DWORD len;
    intptr_t h2 = _get_osfhandle(fd);

    if ((h = (HANDLE)h2) == INVALID_HANDLE_VALUE) {
        memcpy64(buffer, unknown, unknown_len + 1);
        return;
    }

    len = GetFinalPathNameByHandleA(h, buffer, (DWORD)size,
                                    FILE_NAME_NORMALIZED);

    if ((len <= 0) || (len >= size)) {
        memcpy64(buffer, unknown, unknown_len + 1);
        return;
    }

    if (strncmp(buffer, "\\\\?\\", 4) == 0) {
        memmove(buffer, buffer + 4, len - 3);
    }

    return;
#else
    (void)size;
    (void)fd;
    memcpy(buffer, unknown, unknown_len + 1);
    return;
#endif
}

static int
xclose(int *fd, char *filename) {
    if (close(*fd) < 0) {
        char buffer[4096];
        if (filename == NULL) {
            util_filename_from(buffer, sizeof(buffer), *fd);
            filename = buffer;
        }
        error("Error closing %s: %s.\n", filename, strerror(errno));
        *fd = -1;
        return -1;
    }
    *fd = -1;
    return 0;
}

#define xclose_1(...) xclose(__VA_ARGS__, NULL)
#define xclose_2(...) xclose(__VA_ARGS__)
#define XCLOSE(...) SELECT_ON_NUM_ARGS(xclose_, __VA_ARGS__)

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
    char cmdline[1024] = {0};
    FILE *tty;
    PROCESS_INFORMATION proc_info = {0};
    DWORD exit_code = 0;
    int64 len = strlen64(argv[0]);
    char argv0_windows[BUFSIZ];
    char *argv0 = argv[0];

    if (len >= BUFSIZ) {
        error("Invalid arguments.\n");
        fatal(EXIT_FAILURE);
    }

    if (argc == 0 || argv == NULL) {
        error("Invalid arguments.\n");
        fatal(EXIT_FAILURE);
    }

    {
        char *exe = ".exe";
        int64 exe_len = (int64)(strlen64(exe));
        if (memmem64(argv[0], len + 1, exe, exe_len + 1) == NULL) {
            memcpy64(argv0_windows, argv[0], len);
            memcpy64(argv0_windows + len, exe, exe_len + 1);
            argv[0] = argv0_windows;
        }
    }

    {
        int64 j = 0;
        for (int i = 0; i < argc - 1; i += 1) {
            int64 len2 = strlen64(argv[i]);
            if ((j + len2) >= (int64)sizeof(cmdline)) {
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
#endif

#define GENERATE_STRING_FROM_ARRAY(NAME, TYPE, FORMAT) \
static void \
string_from_##NAME(char *buffer, int32 size, \
                   char *sep, TYPE array, int32 array_length) { \
    int32 n = 0; \
    for (int32 i = 0; i < (array_length - 1); i += 1) { \
        int32 space = size - n; \
        int32 m = snprintf2(buffer + n, space, FORMAT"%s", array[i], sep); \
        n += m; \
    } \
    { \
        int32 i = array_length - 1; \
        int32 space = size - n; \
        snprintf2(buffer + n, space, FORMAT, array[i]); \
    } \
    return; \
}

GENERATE_STRING_FROM_ARRAY(strings, char **, "%s")
GENERATE_STRING_FROM_ARRAY(doubles, double *, "%f")

void __attribute__((format(printf, 1, 2)))
error(char *format, ...) {
    char buffer[BUFSIZ];
    va_list args;
    int64 n;

    va_start(args, format);
    n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (n <= 0) {
        fprintf(stderr, "Error in vsnprintf(%s)\n", format);
        fatal(EXIT_FAILURE);
    }
    if (n >= SIZEOF(buffer)) {
        fprintf(stderr, "Error in vsnprintf(%s): Buffer too small.\n", format);
        fatal(EXIT_FAILURE);
    }

    buffer[n] = '\0';
#if OS_WINDOWS
    write(STDERR_FILENO, buffer, (uint)n);
#else
    write(STDERR_FILENO, buffer, (size_t)n);
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
            execlp(notifiers[i], notifiers[i], "-u", "critical", program,
                   buffer, NULL);
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

    return;
}

void
fatal(int status) {
    if (DEBUGGING) {
        (void)status;
        trap();
    } else {
        exit(status);
    }
}

// clang-format off
void
util_segv_handler(int32 unused) {
    char *message = "Memory error. Please send a bug report.\n";
    (void)unused;

    write64(STDERR_FILENO, message, (uint32)strlen64(message));
    for (uint32 i = 0; i < LENGTH(notifiers); i += 1) {
        execlp(notifiers[i],
               notifiers[i], "-u", "critical", program, message, NULL);
    }
    _exit(EXIT_FAILURE);
}
// clang-format on

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

    if (n < 0) {
        fatal(EXIT_FAILURE);
    }
    if (n >= SIZEOF(buffer)) {
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

static void *
util_memdup(void *source, int64 size) {
    void *p = xmalloc(size);
    memcpy64(p, source, size);
    return p;
}

// clang-format off

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
        error("Error opening %s for writing: %s.\n", destination,
              strerror(errno));
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

// clang-format on

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

    if ((*dest_fd
         = open(destination, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR))
        < 0) {
        error("Error opening %s for writing: %s.\n", destination,
              strerror(errno));
        XCLOSE(&source_fd, source);
        return -1;
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
            error("Error in poll(nfds=%lld): %s.\n", (llong)copy_files->nfds,
                  strerror(errno));
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
    free(copy_files);
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
    int64 len = strlen64(executable);

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

static bool
util_equal_files(char *filename_a, char *filename_b) {
    int fd_a;
    int fd_b = -1;
    char buffer_a[BUFSIZ];
    char buffer_b[BUFSIZ];
    int64 total_r = 0;
    int64 ra;
    int64 rb;
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
    if (stat_a.st_dev == stat_b.st_dev && stat_a.st_ino == stat_b.st_ino) {
        equal = true;
        goto out;
    }

#if OS_UNIX
    do {
        if (stat_a.st_size > 0) {
            void *map_a;
            void *map_b;

            // clang-format off
            map_a = mmap(NULL, (size_t)stat_a.st_size,
                         PROT_READ, MAP_PRIVATE, fd_a, 0);
            if (map_a == MAP_FAILED) {
                error("Error in mmap(%s): %s\n", filename_a, strerror(errno));
                break;
            }
            map_b = mmap(NULL, (size_t)stat_a.st_size,
                         PROT_READ, MAP_PRIVATE, fd_b, 0);
            // clang-format on
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
    const double RAD2DEG = 180.0 / 3.141592653589793;
    return radians*RAD2DEG;
}

INLINE double
deg2rad(double degrees) {
    const double DEG2RAD = 3.141592653589793 / 180.0;
    return degrees*DEG2RAD;
}

#if TESTING_util

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
#define WRITE_FILE(PATH, STRING) write_file(PATH, STRING, strlen64(STRING))

static volatile sig_atomic_t received_signal = false;
static void
signal_handler(int signal_number) {
    (void)signal_number;
    received_signal = true;
    return;
}

int
main(int argc, char **argv) {
    char buffer[32];
    void *p1 = xmalloc(SIZEMB(1));
    void *p2 = xcalloc(10, SIZEMB(1));
    char *p3;
    char *string = __FILE__;

    char *paths[] = {
        "/aaaa/bbbb/cccc", "/aa/bb/cc", "/a/b/c",    "a/b/c",
        "a/b/cccc",        "a/bb/cccc", "aaaa/cccc",
    };
    char *bases[] = {
        "cccc", "cc", "c", "c", "cccc", "cccc", "cccc",
    };
    (void)argc;

    if (OS_LINUX) {
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

    memset64(p1, 0, SIZEMB(1));
    memcpy64(p1, string, strlen64(string));
    memset64(p2, 0, SIZEMB(1));
    p3 = xstrdup(p1);

    assert(!strcmp(string, p3));

    srand((uint)time(NULL));
    for (int i = 0; i < 10; i += 1) {
        int n = rand() - RAND_MAX / 2;
        ASSERT_EQUAL(atoi2(itoa2(n, buffer)), n);
    }

    for (int64 i = 0; i < LENGTH(paths); i += 1) {
        char *path = paths[i];
        assert(!strcmp(basename2(path), bases[i]));
    }

    if (OS_WINDOWS) {
        char *path2 = "aa\\cc";
        assert(!strcmp(basename2(path2), "cc"));
    }

    {
        char *a = "/tmp/afile";
        char *b = "/tmp/bfile";

        WRITE_FILE(a, "hello world");
        WRITE_FILE(b, "hello world");
        assert(util_equal_files(a, b));

        WRITE_FILE(a, "hello world");
        WRITE_FILE(b, "hello worlx");
        assert(!util_equal_files(a, b));

        WRITE_FILE(a, "short");
        WRITE_FILE(b, "shorter");
        assert(!util_equal_files(a, b));

        WRITE_FILE(a, "");
        WRITE_FILE(b, "");
        assert(util_equal_files(a, b));

        /* Uncomment below to trigger error */
        /* WRITE_FILE(a, "data"); */
        /* xunlink(b); */
        /* error("Expected error below:\n"); */
        /* assert(!util_equal_files(a, b)); */
    }

    {
        // clang-format off
        const char characters[] = "abcdefghijklmnopqrstuvwxyz1234567890";
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
        // clang-format on
    }

    free(p1);
    free(p2);
    free(p3);

    ASSERT_EQUAL(deg2rad(180.0), 3.141592653589793);
    ASSERT_EQUAL(rad2deg(3.141592653589793), 180.0);

    exit(EXIT_SUCCESS);
}

#endif

#endif
