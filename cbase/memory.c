#if !defined(MEMORY_C)
#define MEMORY_C

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include "platform_detection.h"
#include "base_macros.h"
#include "primitives.h"
#include "rapidhash.h"

static int64 memory_page_size = 0;

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_memory 1
#elif !defined(TESTING_memory)
#define TESTING_memory 0
#endif

#if TESTING_memory
#define DEBUGGING_MEMORY 1
#define MEMORY_CHECK_USE_AFTER_FREE 1
#endif

#if !defined(MEMORY_CHECK_USE_AFTER_FREE)
#define MEMORY_CHECK_USE_AFTER_FREE 0
#endif

#if !defined(DEBUGGING_MEMORY)
#define DEBUGGING_MEMORY DEBUGGING
#endif

typedef struct DebugAllocInfo {
    int64 size;
    char *file;
    char *func;
    int32 line;
    int32 reallocated; /* -1 : freed
                        *  0 : malloced once
                        *  1 : realloced once
                        *  2 : realloced twice
                        * and so on */
} DebugAllocInfo;

#define HASH_KEY_TYPE void *
#define HASH_KEY_FIXED_LEN 1
#define HASH_VALUE_TYPE DebugAllocInfo
#define HASH_TYPE alloc_map
#define HASH_PADDING_TYPE2 uint32
#define HASH_PADDING_TYPE uint32
#include "hash.c"

static struct Hash_alloc_map *allocations = NULL;
static pthread_mutex_t allocations_mutex = PTHREAD_MUTEX_INITIALIZER;

#define X64(FUNC) \
INLINE void \
CAT(FUNC, 64)(void *dest, void *source, int64 n) { \
    if (n == 0) \
        return; \
    if (DEBUGGING) { \
        if (n < 0) { \
            error("Error: Invalid n = %lld\n", (llong)n); \
            fatal(EXIT_FAILURE); \
        } \
        if ((ullong)n >= (ullong)SIZE_MAX) { \
            error("Error: n (%lld) is bigger than SIZEMAX\n", (llong)n); \
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
            error("Error: Invalid size = %lld\n", (llong)size);
            fatal(EXIT_FAILURE);
        }
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error: Size (%lld) is bigger than SIZEMAX\n", (llong)size);
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

static void
memory_check(void) {
    if (RUNNING_ON_VALGRIND) {
        return;
    }

    pthread_mutex_lock(&allocations_mutex);
    if (allocations) {
        for (uint32 i = 0; i < allocations->capacity; i += 1) {
            Bucket_alloc_map *bucket = &allocations->array[i];
            uchar *p;
            int64 size;

            if (bucket->slot_state != HASH_SLOT_USED) {
                continue;
            }

            p = (uchar *)bucket->key;
            size = bucket->value.size;

            for (int32 j = 0; j < 8; j += 1) {
                if (p[-8 + j] != 0xDC) {
                    error_impl(bucket->value.file, bucket->value.line, bucket->value.func,
                               "Memory underflow detected in pointer %p (size %lld).\n",
                               (void *)p, (llong)size);
                    fatal(EXIT_FAILURE);
                }
            }

            for (int32 j = 0; j < 8; j += 1) {
                if (p[size + j] != 0xDC) {
                    error_impl(bucket->value.file, bucket->value.line, bucket->value.func,
                               "Memory overflow detected in pointer %p (size %lld).\n",
                               (void *)p, (llong)size);
                    fatal(EXIT_FAILURE);
                }
            }

            if (MEMORY_CHECK_USE_AFTER_FREE
                    && (bucket->value.reallocated == -1)) {
                for (int64 j = 0; j < size; j += 1) {
                    if (p[j] != 0xCD) {
                        error_impl(bucket->value.file, bucket->value.line, bucket->value.func,
                                   "Use after free detected in pointer %p (size %lld).\n",
                                   (void *)p, (llong)size);
                        fatal(EXIT_FAILURE);
                    }
                }
            }
        }
    }
    pthread_mutex_unlock(&allocations_mutex);
    return;
}

static void *
malloc_debug(char *file, int32 line, char *func, int64 size) {
    void *p;
    uchar *ptr;
    void *base_p;

    if (RUNNING_ON_VALGRIND) {
        return malloc((size_t)size);
    }

    if (size <= 0) {
        error_impl(file, line, func,
                   "Error: invalid size = %lld.\n", (llong)size);
        fatal(EXIT_FAILURE);
    }
    if ((ullong)size >= (ullong)SIZE_MAX) {
        error_impl(file, line, func,
                   "Error: Number (%lld) is bigger than SIZEMAX\n",
                   (llong)size);
        fatal(EXIT_FAILURE);
    }

    base_p = xmalloc(size + 16);
    ptr = (uchar *)base_p;

    for (int32 j = 0; j < 8; j += 1) {
        ptr[j] = 0xDC;
    }
    for (int32 j = 0; j < 8; j += 1) {
        ptr[8 + size + j] = 0xDC;
    }

    p = ptr + 8;
    memset64(p, 0xCD, size);

    {
        DebugAllocInfo info;
        info.size = size;
        info.file = file;
        info.line = line;
        info.func = func;
        info.reallocated = 0;

        pthread_mutex_lock(&allocations_mutex);
        if (allocations == NULL) {
            allocations = hash_create_alloc_map(1024, "DebugAllocations");
        }
        hash_insert_alloc_map(allocations, &p, info);
        pthread_mutex_unlock(&allocations_mutex);
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
    (void)old_capacity;

    return xrealloc(old, new_size);
}

static void *
realloc_debug(char *file, int32 line, char *func,
              void *old, int64 old_capacity, int64 new_capacity, int64 obj_size) {
    int64 old_size;
    int64 new_size;
    void *p;
    void *old_base;
    void *base_p;
    uchar *ptr;
    (void)old_capacity;

    if (RUNNING_ON_VALGRIND) {
        return realloc(old, (size_t)(new_capacity*obj_size));
    }

    if (obj_size <= 0) {
        error_impl(file, line, func,
                   "Error: invalid object size = %lld.\n", (llong)obj_size);
        fatal(EXIT_FAILURE);
    }
    if ((ullong)SIZE_MAX / (ullong)obj_size < (ullong)new_capacity) {
        error_impl(file, line, func,
                   "Error: Number (%lld) is bigger than SIZEMAX\n",
                   (llong)obj_size);
        fatal(EXIT_FAILURE);
    }

    old_size = old_capacity*obj_size;
    new_size = new_capacity*obj_size;

    {
        DebugAllocInfo info;
        info.size = new_size;
        info.file = file;
        info.line = line;
        info.func = func;
        info.reallocated = 0;

        pthread_mutex_lock(&allocations_mutex);

        if ((old != NULL) && (allocations == NULL)) {
            error_impl(file, line, func,
                       "Reallocating invalid pointer %p.", old);
            fatal(EXIT_FAILURE);
        } else if (allocations == NULL) {
            allocations = hash_create_alloc_map(1024, "DebugAllocations");
        }
        if (old != NULL) {
            DebugAllocInfo old_info;
            if (!hash_lookup_alloc_map(allocations, &old, &old_info)) {
                error_impl(file, line, func,
                           "Reallocating invalid pointer %p.\n", old);
                fatal(EXIT_FAILURE);
            }
            if (old_info.reallocated == -1) {
                error_impl(file, line, func,
                           "Reallocating freed pointer %p.\n", old);
                fatal(EXIT_FAILURE);
            }
            if (old_info.size != old_size) {
                error_impl(file, line, func,
                           "Reallocation old size does not match size"
                           " allocated on %s:%d: %s(): %lld != %lld\n",
                           old_info.file, old_info.line, old_info.func,
                           (llong)old_info.size, (llong)old_size);
                fatal(EXIT_FAILURE);
            }

            for (int32 j = 0; j < 8; j += 1) {
                if (((uchar *)old)[-8 + j] != 0xDC) {
                    error_impl(file, line, func,
                               "Memory underflow detected before realloc in %p.\n", old);
                    error_impl(old_info.file, old_info.line, old_info.func,
                               "Allocated here (old size = %lld bytes).\n",
                               (llong)old_size);
                    fatal(EXIT_FAILURE);
                }
            }
            for (int32 j = 0; j < 8; j += 1) {
                if (((uchar *)old)[old_size + j] != 0xDC) {
                    error_impl(file, line, func,
                               "Memory overflow detected before realloc in %p.\n", old);
                    error_impl(old_info.file, old_info.line, old_info.func,
                               "Allocated here (old size = %lld bytes).\n",
                               (llong)old_size);
                    fatal(EXIT_FAILURE);
                }
            }

            info.reallocated = old_info.reallocated + 1;
            hash_remove_alloc_map(allocations, &old);
        }

        old_base = old ? ((uchar *)old - 8) : NULL;
        base_p = xrealloc(old_base, new_size + 16);
        ptr = (uchar *)base_p;

        for (int32 j = 0; j < 8; j += 1) {
            ptr[j] = 0xDC;
        }
        for (int32 j = 0; j < 8; j += 1) {
            ptr[8 + new_size + j] = 0xDC;
        }

        p = ptr + 8;
        hash_insert_alloc_map(allocations, &p, info);

        pthread_mutex_unlock(&allocations_mutex);
    }

    return p;
}

static void *
realloc_flex_debug(char *file, int32 line, char *func,
                   void *old, int64 struct_size,
                   int64 old_capacity, int64 new_capacity, int64 obj_size) {
    void *p;
    void *old_base;
    void *base_p;
    uchar *ptr;
    int64 total_size = struct_size + new_capacity*obj_size;
    int64 old_size = struct_size + old_capacity*obj_size;

    if (RUNNING_ON_VALGRIND) {
        return realloc(old, (size_t)total_size);
    }

    if (new_capacity <= 0) {
        error_impl(file, line, func,
                   "Error: invalid object size = %lld.\n", (llong)new_capacity);
        fatal(EXIT_FAILURE);
    }

    {
        DebugAllocInfo info;

        info.size = total_size;
        info.file = file;
        info.line = line;
        info.func = func;
        info.reallocated = 0;

        pthread_mutex_lock(&allocations_mutex);

        if ((old != NULL) && (allocations == NULL)) {
            error_impl(file, line, func,
                       "Reallocating invalid pointer %p.", old);
            fatal(EXIT_FAILURE);
        } else if (allocations == NULL) {
            allocations = hash_create_alloc_map(1024, "DebugAllocations");
        }
        if (old != NULL) {
            DebugAllocInfo old_info;
            if (!hash_lookup_alloc_map(allocations, &old, &old_info)) {
                error_impl(file, line, func,
                           "Reallocating invalid pointer %p.\n", old);
                fatal(EXIT_FAILURE);
            }
            if (old_info.reallocated == -1) {
                error_impl(file, line, func,
                           "Reallocating freed pointer %p.\n", old);
                fatal(EXIT_FAILURE);
            }
            if (old_info.size != old_size) {
                error_impl(file, line, func,
                           "Reallocation old size does not match size"
                           " allocated on %s:%d: %s(): %lld != %lld\n",
                           old_info.file, old_info.line, old_info.func,
                           (llong)old_info.size, (llong)old_size);
                fatal(EXIT_FAILURE);
            }

            for (int32 j = 0; j < 8; j += 1) {
                if (((uchar *)old)[-8 + j] != 0xDC) {
                    error_impl(file, line, func,
                               "Memory underflow detected before realloc in %p.\n", old);
                    error_impl(old_info.file, old_info.line, old_info.func,
                               "Allocated here (old size = %lld bytes).\n",
                               (llong)old_size);
                    fatal(EXIT_FAILURE);
                }
            }
            for (int32 j = 0; j < 8; j += 1) {
                if (((uchar *)old)[old_size + j] != 0xDC) {
                    error_impl(file, line, func,
                               "Memory overflow detected before realloc in %p.\n", old);
                    error_impl(old_info.file, old_info.line, old_info.func,
                               "Allocated here (old size = %lld bytes).\n",
                               (llong)old_size);
                    fatal(EXIT_FAILURE);
                }
            }

            info.reallocated = old_info.reallocated + 1;
            hash_remove_alloc_map(allocations, &old);
        }

        old_base = old ? ((uchar *)old - 8) : NULL;
        base_p = xrealloc(old_base, total_size + 16);
        ptr = (uchar *)base_p;

        for (int32 j = 0; j < 8; j += 1) {
            ptr[j] = 0xDC;
        }
        for (int32 j = 0; j < 8; j += 1) {
            ptr[8 + total_size + j] = 0xDC;
        }

        p = ptr + 8;
        hash_insert_alloc_map(allocations, &p, info);

        pthread_mutex_unlock(&allocations_mutex);
    }

    return p;
}

static void
free_debug(char *file, int32 line, char *func,
           void *pointer, int64 size) {
    DebugAllocInfo info;

    if (RUNNING_ON_VALGRIND) {
        free(pointer);
        return;
    }

    if (size < 0) {
        error_impl(file, line, func,
                   "Error: freeing allocation of negative size = %lld.\n",
                   (llong)size);
        fatal(EXIT_FAILURE);
    }

    if (pointer == NULL) {
        return;
    }

    pthread_mutex_lock(&allocations_mutex);

    if (allocations == NULL) {
        error_impl(file, line, func,
                   "Called free without having called malloc or realloc.\n");
        fatal(EXIT_FAILURE);
    }
    if (hash_lookup_alloc_map(allocations, &pointer, &info)) {
        uchar *ptr;

        if (info.reallocated == -1) {
            error_impl(file, line, func,
                       "Error: double free of pointer %p.\n", pointer);
            fatal(EXIT_FAILURE);
        }
        if (info.size != size) {
            error_impl(file, line, func,
                       "Error: size mismatch freeing %p. Expected %lld, got %lld.\n",
                       pointer, (llong)info.size, (llong)size);
            error_impl(info.file, info.line, info.func,
                       "Memory was allocated here. (reallocated = %d)\n",
                       info.reallocated);
            fatal(EXIT_FAILURE);
        }

        ptr = (uchar *)pointer;

        for (int32 j = 0; j < 8; j += 1) {
            if (ptr[-8 + j] != 0xDC) {
                error_impl(info.file, info.line, info.func,
                           "Memory underflow detected during free of %p.\n", pointer);
                fatal(EXIT_FAILURE);
            }
        }
        for (int32 j = 0; j < 8; j += 1) {
            if (ptr[size + j] != 0xDC) {
                error_impl(info.file, info.line, info.func,
                           "Memory overflow detected during free of %p.\n", pointer);
                fatal(EXIT_FAILURE);
            }
        }

        info.file = file;
        info.line = line;
        info.func = func;
        info.reallocated = -1;
        hash_remove_alloc_map(allocations, &pointer);
        hash_insert_alloc_map(allocations, &pointer, info);
        
        if (MEMORY_CHECK_USE_AFTER_FREE) {
            memset64(pointer, 0xCD, size);
        }
    } else {
        error_impl(file, line, func,
                   "Error: freeing untracked pointer %p.\n", pointer);
        fatal(EXIT_FAILURE);
    }

    pthread_mutex_unlock(&allocations_mutex);

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
    malloc_debug(__FILE__, __LINE__, (char *)__func__, size)
#define realloc2(old, old_capacity, new_capacity, obj_size) \
    realloc_debug(__FILE__, __LINE__, (char *)__func__, old, old_capacity, new_capacity, obj_size)
#define realloc_flex(old, old_capacity, new_capacity, obj_size) \
    realloc_flex_debug(__FILE__, __LINE__, (char *)__func__, old, SIZEOF(*old), old_capacity, new_capacity, obj_size)
#define free2(pointer, size) \
    free_debug(__FILE__, __LINE__, (char *)__func__, pointer, size)
#else
#define malloc2(size) \
    xmalloc(size)
#define realloc2(old, old_capacity, new_capacity, obj_size) \
    realloc4(old, old_capacity, new_capacity, obj_size)
#define realloc_flex(old, old_capacity, new_capacity, obj_size) \
    xrealloc(old, SIZEOF(*old) + obj_size*new_capacity)
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
xmunmap(void *p, int64 size) {
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
// flags: -lm
#include <signal.h>
#include <setjmp.h>
#include "util.c"

static sigjmp_buf test_jump_env;
static bool caught_expected_fail = false;

static void __attribute((noreturn))
test_expected_fail_handler(int sig) {
    (void)sig;
    caught_expected_fail = true;
    siglongjmp(test_jump_env, 1);
}

typedef struct TestFlex {
    int32 count;
    int32 padding;
    int64 items[];
} TestFlex;

#define ASSERT_EXPECTED_FATAL(BLOCK) do { \
    caught_expected_fail = false; \
    if (sigsetjmp(test_jump_env, 1) == 0) { \
        BLOCK; \
        fprintf(stderr, "Error: Code block at %s:%d did not fail as expected.\n", \
                __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
    ASSERT(caught_expected_fail); \
    printf("Successfully caught expected failure at %s:%d\n", __FILE__, __LINE__); \
} while (0)

int main(void) {
    struct sigaction sa;
    memset(&sa, 0, SIZEOF(sa));
    sa.sa_handler = test_expected_fail_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGILL, &sa, NULL) != 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    printf("--- Starting Comprehensive Memory Tests ---\n");

    {
        int64 size = 256;
        char *p = malloc2(size);
        ASSERT(p != NULL);
        printf("malloc2(256) successful.\n");

        if (DEBUGGING_MEMORY && !RUNNING_ON_VALGRIND) {
            for (int32 i = 0; i < size; i += 1) {
                ASSERT((uchar)p[i] == 0xCD);
            }
            printf("Memory correctly initialized with debug byte.\n");
        }
        memory_check();
        free2(p, size);
        printf("free2(256) successful.\n");
    }

    {
        int64 count = 100;
        int64 grow = 200;
        int64 shrink = 50;
        int64 *arr = malloc2(count*SIZEOF(int64));

        for (int32 i = 0; i < count; i += 1) {
            arr[i] = (int64)i;
        }

        memory_check();
        arr = realloc2(arr, count, grow, SIZEOF(int64));
        for (int32 i = 0; i < count; i += 1) {
            ASSERT(arr[i] == (int64)i);
        }
        printf("realloc2 (grow) preserved data.\n");

        memory_check();
        arr = realloc2(arr, grow, shrink, SIZEOF(int64));
        for (int32 i = 0; i < shrink; i += 1) {
            ASSERT(arr[i] == (int64)i);
        }
        printf("realloc2 (shrink) successful.\n");

        free2(arr, shrink*SIZEOF(int64));
    }

    {
        int64 count = 5;
        int64 grow = 10;
        int64 shrink = 3;
        int64 initial_size;
        TestFlex *flex;

        initial_size = SIZEOF(TestFlex) + (count * SIZEOF(int64));
        flex = malloc2(initial_size);
        flex->count = (int32)count;

        for (int32 i = 0; i < count; i += 1) {
            flex->items[i] = (int64)(i * 10);
        }

        memory_check();
        flex = realloc_flex(flex, count, grow, SIZEOF(int64));
        flex->count = (int32)grow;
        for (int32 i = 0; i < count; i += 1) {
            ASSERT(flex->items[i] == (int64)(i * 10));
        }
        printf("realloc_flex (grow) preserved data.\n");

        memory_check();
        flex = realloc_flex(flex, grow, shrink, SIZEOF(int64));
        flex->count = (int32)shrink;
        for (int32 i = 0; i < shrink; i += 1) {
            ASSERT(flex->items[i] == (int64)(i * 10));
        }
        printf("realloc_flex (shrink) successful.\n");

        free2(flex, SIZEOF(*flex) + (shrink*SIZEOF(int64)));
    }

    {
        char *original = "Comprehensive memory test string";
        char *mem_dup;
        char *dup = xstrdup(original);
        int64 len = strlen32(original) + 1;

        ASSERT(dup != original);
        ASSERT(strcmp(dup, original) == 0);
        printf("xstrdup successful.\n");

        mem_dup = xmemdup(dup, len);
        ASSERT(mem_dup != dup);
        ASSERT(memcmp(mem_dup, dup, (size_t)len) == 0);
        printf("xmemdup successful.\n");

        free2(dup, len);
        free2(mem_dup, len);
    }

#if OS_LINUX
    printf("\n--- Starting Failure Case Tests ---\n");

    {
        void *untracked = (void *)0xDEADBEEF;
        ASSERT_EXPECTED_FATAL({
            free2(untracked, 16);
        });
        pthread_mutex_unlock(&allocations_mutex);
    }

    {
        int64 size = 64;
        uchar *p = malloc2(size);
        ASSERT_EXPECTED_FATAL({
            free2(p, size + 1); // Incorrect size
        });
        pthread_mutex_unlock(&allocations_mutex);
        free(p - 8); 
    }

    {
        int64 size = 16;
        uchar *p = malloc2(size);
        free2(p, size);
        ASSERT_EXPECTED_FATAL({
            free2(p, size); // Double free
        });
        pthread_mutex_unlock(&allocations_mutex);
        free(p - 8);
    }

    {
        int64 count = 10;
        int64 *arr = malloc2(count*SIZEOF(int64));
        ASSERT_EXPECTED_FATAL({
            // Realloc with wrong old size
            realloc2(arr, count + 5, 20, SIZEOF(int64));
        });
        pthread_mutex_unlock(&allocations_mutex);
        free2(arr, count*SIZEOF(int64));
    }

    {
        int64 count = 5;
        int64 initial_size;
        TestFlex *flex;

        initial_size = SIZEOF(TestFlex) + (count * SIZEOF(int64));
        flex = malloc2(initial_size);

        ASSERT_EXPECTED_FATAL({
            // Realloc flex with wrong old capacity
            realloc_flex(flex, count + 2, 10, SIZEOF(int64));
        });
        pthread_mutex_unlock(&allocations_mutex);
        free2(flex, initial_size);
    }

    {
        void *invalid_ptr = (void *)0xBAADF00D;
        ASSERT_EXPECTED_FATAL({
            // Realloc with untracked pointer
            realloc2(invalid_ptr, 1, 10, SIZEOF(int64));
        });
        pthread_mutex_unlock(&allocations_mutex);
    }

    {
        int64 size = 64;
        uchar *p = malloc2(size);
        ASSERT_EXPECTED_FATAL({
            p[-8] = 0x00; // Corrupt underflow canary
            memory_check();
        });
        pthread_mutex_unlock(&allocations_mutex);
        free(p - 8); 
    }

    {
        int64 size = 64;
        uchar *p = malloc2(size);
        ASSERT_EXPECTED_FATAL({
            p[size] = 0x00; // Corrupt overflow canary
            memory_check();
        });
        pthread_mutex_unlock(&allocations_mutex);
        free(p - 8); 
    }

    {
        int64 size = 32;
        uchar *p = malloc2(size);
        ASSERT_EXPECTED_FATAL({
            p[-4] = 0xFF; // Corrupt underflow canary
            free2(p, size);
        });
        pthread_mutex_unlock(&allocations_mutex);
        free(p - 8);
    }

    {
        int64 size = 32;
        uchar *p = malloc2(size);
        ASSERT_EXPECTED_FATAL({
            p[size] = 0xFF; // Corrupt overflow canary
            free2(p, size);
        });
        pthread_mutex_unlock(&allocations_mutex);
        free(p - 8);
    }

    {
        int64 size = 32;
        uchar *p = malloc2(size);
        free2(p, size);
        ASSERT_EXPECTED_FATAL({
            p[0] = 0xAA; // Use after free
            memory_check();
        });
        pthread_mutex_unlock(&allocations_mutex);
        free(p - 8);
    }
#endif

    fsync(STDOUT_FILENO);
    fsync(STDERR_FILENO);
    printf("\nAll memory tests passed.\n");
    return EXIT_SUCCESS;
}

#endif

#endif /* MEMORY_C */
