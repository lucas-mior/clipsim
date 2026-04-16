#if !defined(MEMORY_C)
#define MEMORY_C

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include "platform_detection.h"
#include "base_macros.h"
#include "primitives.h"

static int64 memory_page_size = 0;

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_memory 1
#elif !defined(TESTING_memory)
#define TESTING_memory 0
#endif

#define MEM_FREED 0xDC
#define MEM_MALLOCED_UNINITIALIZED 0xCD
#define MEM_DONT_READ 0xBD

#if !defined(DEBUGGING_MEMORY)
#define DEBUGGING_MEMORY DEBUGGING
#endif

static void __attribute__((format(printf, 3, 4)))
    error_impl(char *file, int32 line, char *format, ...);
#define error(...) error_impl(__FILE__, __LINE__, __VA_ARGS__)
static void fatal(int status);
INLINE int32 strlen32(char *string);

#define X64(FUNC) \
INLINE void \
CAT(FUNC, 64)(void *dest, void *source, int64 n) { \
    if (n == 0) \
        return; \
    if (DEBUGGING) { \
        if (n < 0) { \
            error("Error in %s: Invalid n = %lld\n", __func__, (llong)n); \
            fatal(EXIT_FAILURE); \
        } \
        if ((ullong)n >= (ullong)SIZE_MAX) { \
            error("Error in %s: n (%lld) is bigger than SIZEMAX\n", \
                   __func__, (llong)n); \
            fatal(EXIT_FAILURE); \
        } \
    } \
    FUNC(dest, source, (size_t)n); \
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
            error("Error in %s: Size (%lld) is bigger than SIZEMAX\n",
                  __func__, (llong)size);
            fatal(EXIT_FAILURE);
        }
    }
    memset(buffer, value, (size_t)size);
    return;
}


INLINE void *
xmalloc(int64 size) {
    void *p;
    if ((p = malloc((size_t)size)) == NULL) {
        error("Failed to allocate %lld bytes.\n", (llong)size);
        fatal(EXIT_FAILURE);
    }
    return p;
}

static void *
malloc_debug(char *file, int32 line, int64 size) {
    void *p;

    if (size <= 0) {
        error_impl(file, line,
                   "Error in %s: invalid size = %lld.\n",
                   __func__, (llong)size);
        fatal(EXIT_FAILURE);
    }
    if ((ullong)size >= (ullong)SIZE_MAX) {
        error_impl(file, line,
                   "Error in %s: Number (%lld) is bigger than SIZEMAX\n",
                   __func__, (llong)size);
        fatal(EXIT_FAILURE);
    }

    if (DEBUGGING_MEMORY) {
        error_impl(file, line, "Allocating %lld bytes...\n", (llong)size);
    }

    p = xmalloc(size);

    if (!RUNNING_ON_VALGRIND) {
        memset64(p, MEM_MALLOCED_UNINITIALIZED, size);
    }
    return p;
}

INLINE void *
xrealloc(void *old, int64 new_size) {
    void *p;
    uint64 old_save = (uint64)old;

    if ((p = realloc(old, (size_t)new_size)) == NULL) {
        error("Failed to reallocate %lld bytes from %llx.\n",
              (llong)new_size, (ullong)old_save);
        fatal(EXIT_FAILURE);
    }

    return p;
}

INLINE void *
realloc4(void *old, int64 old_capacity, int64 new_capacity, int64 obj_size) {
    int64 new_size = new_capacity*obj_size;

    if (DEBUGGING_MEMORY) {
        error("Reallocating %p: %lld to %lld objects of size %lld.\n",
              old, (llong)old_capacity, (llong)new_capacity, (llong)obj_size);
    }

    return xrealloc(old, new_size);
}

static void *
realloc_debug(char *file, int32 line,
              void *old, int64 old_capacity, int64 new_capacity, int64 obj_size) {
    int64 new_size;
    (void)old_capacity;

    if (obj_size <= 0) {
        error_impl(file, line,
                   "Error in %s: invalid object size = %lld.\n",
                   __func__, (llong)obj_size);
        fatal(EXIT_FAILURE);
    }
    if ((ullong)SIZE_MAX / (ullong)obj_size < (ullong)new_capacity) {
        error_impl(file, line,
                   "Error in %s: Number (%lld) is bigger than SIZEMAX\n",
                   __func__, (llong)obj_size);
        fatal(EXIT_FAILURE);
    }
    if (DEBUGGING_MEMORY) {
        error_impl(file, line,
                   "Reallocating %p: %lld to %lld objects of size %lld.\n",
                   old, (llong)old_capacity, (llong)new_capacity, (llong)obj_size);
    }

    new_size = new_capacity*obj_size;
    return xrealloc(old, new_size);
}

static void
free_debug(char *file, int32 line, void *pointer, int64 size) {
    if (size < 0) {
        error_impl(file, line,
                   "Error: freeing allocation of negative size = %lld.\n",
                   (llong)size);
        fatal(EXIT_FAILURE);
    }
    if (DEBUGGING_MEMORY && pointer && size) {
        error_impl(file, line,
                   "Freeing %p of size %lld\n", pointer, (llong)size);
        if (!RUNNING_ON_VALGRIND) {
            memset64(pointer, MEM_FREED, size);
        }
    }
    free(pointer);
    return;
}

INLINE void
free2_(void *pointer, int64 size) {
    (void)size;
    if (pointer) {
        free(pointer);
    }
    return;
}

#if DEBUGGING_MEMORY
#define malloc2(size) \
    malloc_debug(__FILE__, __LINE__, size)
#define realloc2(old, old_capacity, new_capacity, obj_size) \
    realloc_debug(__FILE__, __LINE__, old, old_capacity, new_capacity, obj_size)
#define free2(pointer, size) \
    free_debug(__FILE__, __LINE__, pointer, size)
#else
#define malloc2(size) \
    xmalloc(size)
#define realloc2(old, old_capacity, new_capacity, obj_size) \
    realloc4(old, old_capacity, new_capacity, obj_size)
#define free2(pointer, size) \
    free2_(pointer, size)
#endif

#if OS_UNIX
static void *
xmmap_commit(int64 *size) {
    void *p;

    if (RUNNING_ON_VALGRIND) {
        p = malloc2(*size);
        memset64(p, 0, *size);
        return p;
    }
    if (memory_page_size == 0) {
        long aux;
        if ((aux = sysconf(_SC_PAGESIZE)) <= 0) {
            error("Error getting page size: %s.\n", strerror(errno));
            fatal(EXIT_FAILURE);
        }
        memory_page_size = aux;
    }

    do {
        if ((*size >= SIZEMB(2)) && FLAGS_HUGE_PAGES) {
            p = mmap(NULL, (size_t)*size, PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE
                         | FLAGS_HUGE_PAGES,
                     -1, 0);
            if (p != MAP_FAILED) {
                *size = ALIGN_POWER_OF_2(*size, SIZEMB(2));
                break;
            }
        }
        p = mmap(NULL, (size_t)*size, PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
        *size = ALIGN_POWER_OF_2(*size, memory_page_size);
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
        free2(p, size);
        return;
    }
    if (munmap(p, (size_t)size) < 0) {
        error("Error in munmap(%p, %lld): %s.\n",
              p, (llong)size, strerror(errno));
        fatal(EXIT_FAILURE);
    }
    return;
}
#else
static void *
xmmap_commit(int64 *size) {
    void *p;

    if (RUNNING_ON_VALGRIND) {
        p = malloc2(*size);
        memset64(p, 0, *size);
        return p;
    }
    if (memory_page_size == 0) {
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        memory_page_size = system_info.dwPageSize;
        if (memory_page_size <= 0) {
            fprintf(stderr, "Error getting page size.\n");
            fatal(EXIT_FAILURE);
        }
    }

    p = VirtualAlloc(NULL, (size_t)*size, MEM_COMMIT | MEM_RESERVE,
                     PAGE_READWRITE);
    if (p == NULL) {
        fprintf(stderr, "Error in VirtualAlloc(%lld): %lu.\n",
                        (llong)*size, GetLastError());
        fatal(EXIT_FAILURE);
    }
    return p;
}
static void
xmunmap(void *p, size_t size) {
    (void)size;
    if (RUNNING_ON_VALGRIND) {
        free2(p, (int64)size);
        return;
    }
    if (!VirtualFree(p, 0, MEM_RELEASE)) {
        fprintf(stderr, "Error in VirtualFree(%p): %lu.\n", p, GetLastError());
    }
    return;
}
#endif

static void *
xmemdup(void *source, int64 size) {
    void *p = malloc2(size);
    memcpy64(p, source, size);
    return p;
}

static char *
xstrdup(char *string) {
    char *p;
    int64 length;

    length = strlen32(string) + 1;
    if ((p = malloc2(length)) == NULL) {
        error("Error allocating %lld bytes to duplicate '%s': %s\n",
              (llong)length, string, strerror(errno));
        fatal(EXIT_FAILURE);
    }

    memcpy64(p, string, length);
    return p;
}

#if TESTING_memory
#include "util.c"
int main(void) {
    exit(EXIT_SUCCESS);
}
#endif

#endif /* MEMORY_C */
