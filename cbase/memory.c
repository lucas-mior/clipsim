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
#endif

#if !defined(MEMORY_CHECK_USE_AFTER_FREE)
#define MEMORY_CHECK_USE_AFTER_FREE 0
#endif

#if !defined(DEBUGGING_MEMORY)
#define DEBUGGING_MEMORY DEBUGGING
#endif

static void __attribute__((format(printf, 3, 4)))
    error_impl(char *file, int32 line, char *format, ...);
#define error(...) error_impl(__FILE__, __LINE__, __VA_ARGS__)
static void fatal(int status);
INLINE int32 strlen32(char *string);

typedef struct DebugAllocInfo {
    int64 size;
    char *file;
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

            if (MEMORY_CHECK_USE_AFTER_FREE
                    && (bucket->value.reallocated == -1)) {
                for (int64 j = 0; j < size; j += 1) {
                    if (p[j] != 0xCD) {
                        error_impl(bucket->value.file, bucket->value.line,
                                   "Use after free detected in pointer %p (size %lld).\n",
                                   (void *)p, (llong)size);
                        fatal(EXIT_FAILURE);
                    }
                }
            } else {
                if (p[size] != 0xDC) {
                    error_impl(bucket->value.file, bucket->value.line,
                               "Memory overflow detected in pointer %p (size %lld).\n",
                               (void *)p, (llong)size);
                    fatal(EXIT_FAILURE);
                }
            }
        }
    }
    pthread_mutex_unlock(&allocations_mutex);
    return;
}

static void *
malloc_debug(char *file, int32 line, int64 size) {
    void *p;
    uchar *ptr;

    if (RUNNING_ON_VALGRIND) {
        return malloc((size_t)size);
    }

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

    p = xmalloc(size + 1);
    ptr = (uchar *)p;
    ptr[size] = 0xDC;

    memset64(p, 0xCD, size);

    {
        DebugAllocInfo info;
        info.size = size;
        info.file = file;
        info.line = line;
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
realloc_debug(char *file, int32 line,
              void *old, int64 old_capacity, int64 new_capacity, int64 obj_size) {
    int64 old_size;
    int64 new_size;
    void *p;
    uchar *ptr;
    (void)old_capacity;

    if (RUNNING_ON_VALGRIND) {
        return realloc(old, (size_t)(new_capacity*obj_size));
    }

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

    old_size = old_capacity*obj_size;
    new_size = new_capacity*obj_size;

    {
        DebugAllocInfo info;
        info.size = new_size;
        info.file = file;
        info.line = line;
        info.reallocated = 0;

        pthread_mutex_lock(&allocations_mutex);

        if ((old != NULL) && (allocations == NULL)) {
            error_impl(file, line, "Reallocating invalid pointer %p.", old);
            fatal(EXIT_FAILURE);
        } else if (allocations == NULL) {
            allocations = hash_create_alloc_map(1024, "DebugAllocations");
        }
        if (old != NULL) {
            DebugAllocInfo old_info;
            if (!hash_lookup_alloc_map(allocations, &old, &old_info)) {
                error_impl(file, line, "Reallocating invalid pointer %p.\n", old);
                fatal(EXIT_FAILURE);
            }
            if (old_info.reallocated == -1) {
                error_impl(file, line, "Reallocating freed pointer %p.\n", old);
                fatal(EXIT_FAILURE);
            }
            if (old_info.size != old_size) {
                error_impl(file, line,
                           "Reallocation old size does not match size"
                           " allocated on %s:%d: %lld != %lld\n",
                           old_info.file, old_info.line,
                           (llong)old_info.size, (llong)old_size);
                fatal(EXIT_FAILURE);
            }
            if (((uchar *)old)[old_size] != 0xDC) {
                error_impl(old_info.file, old_info.line,
                           "Memory overflow detected before realloc in %p.\n", old);
                fatal(EXIT_FAILURE);
            }

            info.reallocated = old_info.reallocated + 1;
            hash_remove_alloc_map(allocations, &old);
        }

        p = xrealloc(old, new_size + 1);
        ptr = (uchar *)p;
        ptr[new_size] = 0xDC;

        hash_insert_alloc_map(allocations, &p, info);

        pthread_mutex_unlock(&allocations_mutex);
    }

    return p;
}

static void
free_debug(char *file, int32 line, void *pointer, int64 size) {
    DebugAllocInfo info;

    if (RUNNING_ON_VALGRIND) {
        free(pointer);
        return;
    }

    if (size < 0) {
        error_impl(file, line,
                   "Error: freeing allocation of negative size = %lld.\n",
                   (llong)size);
        fatal(EXIT_FAILURE);
    }

    if (pointer == NULL) {
        return;
    }

    pthread_mutex_lock(&allocations_mutex);

    if (allocations == NULL) {
        error_impl(file, line,
                   "Called free without having called malloc or realloc.\n");
        fatal(EXIT_FAILURE);
    }
    if (hash_lookup_alloc_map(allocations, &pointer, &info)) {
        uchar *ptr;

        if (info.reallocated == -1) {
            error_impl(file, line, "Error: double free of pointer %p.\n", pointer);
            fatal(EXIT_FAILURE);
        }
        if (info.size != size) {
            error_impl(file, line, 
                       "Error: size mismatch freeing %p. Expected %lld, got %lld.\n",
                       pointer, (llong)info.size, (llong)size);
            error_impl(info.file, info.line, "Memory was allocated here.\n");
            fatal(EXIT_FAILURE);
        }
        
        ptr = (uchar *)pointer;
        if (ptr[size] != 0xDC) {
            error_impl(info.file, info.line,
                       "Memory overflow detected during free of %p.\n", pointer);
            fatal(EXIT_FAILURE);
        }

        info.file = file;
        info.line = line;
        info.reallocated = -1;
        hash_remove_alloc_map(allocations, &pointer);
        hash_insert_alloc_map(allocations, &pointer, info);
        
        if (MEMORY_CHECK_USE_AFTER_FREE) {
            memset64(pointer, 0xCD, size);
        }
    } else {
        error_impl(file, line, "Error: freeing untracked pointer %p.\n", pointer);
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
        void *p = malloc2(size);
        ASSERT_EXPECTED_FATAL({
            free2(p, size + 1); // Incorrect size
        });
        pthread_mutex_unlock(&allocations_mutex);
        free(p); 
    }

    {
        int64 size = 16;
        void *p = malloc2(size);
        free2(p, size);
        ASSERT_EXPECTED_FATAL({
            free2(p, size); // Double free
        });
        pthread_mutex_unlock(&allocations_mutex);
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
            p[size] = 0x00; // Corrupt canary
            memory_check();
        });
        pthread_mutex_unlock(&allocations_mutex);
        free(p); 
    }

    {
        int64 size = 32;
        uchar *p = malloc2(size);
        ASSERT_EXPECTED_FATAL({
            p[size] = 0xFF; // Corrupt canary
            free2(p, size);
        });
        pthread_mutex_unlock(&allocations_mutex);
        free(p);
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
    }
#endif

    printf("\nAll memory tests passed.\n");
    return EXIT_SUCCESS;
}

#endif

#endif /* MEMORY_C */
