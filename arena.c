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

#if !defined(ARENA_C)
#define ARENA_C

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
#include <unistd.h>
#endif

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_arena 1
#elif !defined(TESTING_arena)
#define TESTING_arena 0
#endif

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>

#if !defined(SIZEKB)
#define SIZEKB(X) ((int64)(X)*1024l)
#define SIZEMB(X) ((int64)(X)*1024l*1024l)
#define SIZEGB(X) ((int64)(X)*1024l*1024l*1024l)
#endif

#define ARENA_ALIGN(S, A) (((S) + ((A) - 1)) & ~((A) - 1))
#if !defined(ALIGNMENT)
#define ALIGNMENT 16ul
#endif
#if !defined(ALIGN)
#define ALIGN(x) ARENA_ALIGN(x, ALIGNMENT)
#endif

#if OS_LINUX && defined(MAP_HUGE_2MB)
#define FLAGS_HUGE_PAGES MAP_HUGETLB | MAP_HUGE_2MB
#else
#define FLAGS_HUGE_PAGES 0
#endif

#if !defined(DEBUGGING)
#define DEBUGGING 0
#endif

#if !defined(INLINE)
#if defined(__GNUC__)
#define INLINE static inline __attribute__((always_inline))
#else
#define INLINE static inline
#endif
#endif

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long ullong;

typedef long long llong;
typedef uintptr_t uintptr;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef struct Arena {
    char *name;
    char *begin;
    void *pos;
    int64 size;
    int64 npushed;
    struct Arena *next;
} Arena;

static void *arena_allocate(int64 *);
static bool arena_free(Arena *);
static bool arena_pop(Arena *arena, void *p);
static int64 arena_narenas(Arena *arena);

static int64 arena_page_size = 0;

#if !defined(error2)
#define error2(...) fprintf(stderr, __VA_ARGS__)
#endif

void memset64(void *buffer, int value, int64 size);

static void
arena_print(Arena *arena) {
    while (arena) {
        error2("Arena %p {\n", (void *)arena);
        error2("  name: %s\n", arena->name);
        error2("  begin: %p\n", arena->begin);
        error2("  pos: %p\n", arena->pos);
        error2("  size: %lld\n", (llong)arena->size);
        error2("  npushed: %lld\n", (llong)arena->npushed);
        error2("  next:    %p\n", (void *)arena->next);
        error2("}");
        if (arena->next) {
            error2(" -> ");
        } else {
            error2("\n");
        }
        arena = arena->next;
    }
}

enum ArenaErrors {
    EARENA_INVALID = 2000000,
    EARENA_INVALID_OBJECT,
    EARENA_OBJECT_SIZE,
    EARENA_SIZE,
};

static char *
arena_strerror(int arena_errno) {
    switch (arena_errno) {
    case EARENA_INVALID:
        return "Invalid arena pointer";
    case EARENA_INVALID_OBJECT:
        return "Object is not from arena";
    case EARENA_OBJECT_SIZE:
        return "Object is too big for arena";
    case EARENA_SIZE:
        return "Invalid size";
    default:
        return strerror(arena_errno);
    }
}

static Arena *
arena_create(int64 size) {
    void *p;
    Arena *arena;

    if (size <= 0) {
        errno = EARENA_SIZE;
        return NULL;
    }
    if ((p = arena_allocate(&size)) == NULL) {
        return NULL;
    }

    arena = p;
    arena->name = "arena";
    arena->begin = (char *)arena + ALIGN(sizeof(*arena));
    arena->size = size;
    arena->pos = arena->begin;
    arena->next = NULL;
    arena->npushed = 0;

    return arena;
}

static void
arena_destroy(Arena *arena) {
    Arena *next;

    do {
        next = arena->next;
        arena_free(arena);
    } while ((arena = next));

    return;
}

#if OS_UNIX
void *
arena_allocate(int64 *size) {
    void *p;

    if (arena_page_size == 0) {
        long aux;
        if ((aux = sysconf(_SC_PAGESIZE)) <= 0) {
            error2("Error getting page size: %s.\n", strerror(errno));
            return NULL;
        }
        arena_page_size = aux;
    }

    do {
        if ((*size >= SIZEMB(2)) && FLAGS_HUGE_PAGES) {
            p = mmap(NULL, (size_t)*size, PROT_READ | PROT_WRITE,
                     MAP_ANON | MAP_PRIVATE | FLAGS_HUGE_PAGES, -1, 0);
            if (p != MAP_FAILED) {
                *size = ARENA_ALIGN(*size, SIZEMB(2));
                break;
            }
        }
        p = mmap(NULL, (size_t)*size, PROT_READ | PROT_WRITE,
                 MAP_ANON | MAP_PRIVATE, -1, 0);
        *size = ARENA_ALIGN(*size, arena_page_size);
    } while (0);

    if (p == MAP_FAILED) {
        error2("Error in mmap(%lld): %s.\n", (llong)*size, strerror(errno));
        return NULL;
    }
    return p;
}
bool
arena_free(Arena *arena) {
    if (munmap(arena, (size_t)arena->size) < 0) {
        error2("Error in munmap(%p, %lld): %s.\n", (void *)arena,
               (llong)arena->size, strerror(errno));
        return false;
    }
    return true;
}
#else
void *
arena_allocate(int64 *size) {
    void *p;

    if (arena_page_size == 0) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        arena_page_size = si.dwPageSize;
        if (arena_page_size <= 0) {
            error2("Error getting page size.\n");
            return NULL;
        }
    }

    if ((p
         = VirtualAlloc(NULL, *size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE))
        == NULL) {
        error2("Error in VirtualAlloc(%lld): %lu.\n", (llong)*size,
               GetLastError());
        return NULL;
    }
    *size = ARENA_ALIGN(*size, arena_page_size);
    return p;
}
bool
arena_free(Arena *arena) {
    if (!VirtualFree(arena, 0, MEM_RELEASE)) {
        error2("Error in VirtualFree(%p): %lu.\n", arena, GetLastError());
        return false;
    }
    return true;
}
#endif

static int64
arena_data_size(Arena *arena) {
    int64 size = arena->size - (arena->begin - (char *)arena);
    return size;
}

static Arena *
arena_with_space(Arena *arena, int64 size) {
    if (arena == NULL) {
        errno = EARENA_INVALID;
        return NULL;
    }
    if (size > (arena_data_size(arena))) {
        errno = EARENA_OBJECT_SIZE;
        return NULL;
    }

    if (arena->npushed == 0) {
        return arena;
    }

    while (arena) {
        if (((char *)arena->pos + size)
            <= (arena->begin + arena_data_size(arena))) {
            break;
        }
        if (arena->next == NULL) {
            arena->next = arena_create(arena->size);
        }

        arena = arena->next;
    }
    return arena;
}

static void *__attribute__((malloc))
arena_push(Arena *arena, int64 size) {
    void *before;

    if ((arena = arena_with_space(arena, size)) == NULL) {
        return NULL;
    }

    before = arena->pos;
    if (DEBUGGING) {
        memset64(before, 0xCD, size);
    }
    arena->pos = (char *)arena->pos + size;
    arena->npushed += 1;
    return before;
}

static void *
arenas_push(Arena **arenas, int64 number, int64 size) {
    for (uint32 i = 0; i < number; i += 1) {
        void *p = arena_push(arenas[i], size);
        if (p) {
            return p;
        }
    }
    return NULL;
}

static uint32
arena_push_index32(Arena *arena, uint32 size) {
    void *before;

    if ((arena = arena_with_space(arena, size)) == NULL) {
        return UINT32_MAX;
    }

    before = arena->pos;
    arena->pos = (char *)arena->pos + size;
    if (arena->size >= UINT32_MAX) {
        return UINT32_MAX;
    }
    arena->npushed += 1;

    return (uint32)((char *)before - (char *)arena->begin);
}

static Arena *
arena_of(Arena *arena, void *p) {
    uintptr pointer_num = (uintptr)p;

    while (arena) {
        uintptr begin = (uintptr)arena->begin;
        uintptr end = (uintptr)((char *)arena + arena->size);
        if ((begin <= pointer_num) && (pointer_num < end)) {
            return arena;
        }

        if (!arena->next) {
            break;
        }
        arena = arena->next;
    }
    errno = EARENA_INVALID_OBJECT;
    return NULL;
}

static bool
arenas_pop(Arena **arenas, uint32 number, void *p) {
    for (uint32 i = 0; i < number; i += 1) {
        if (arena_pop(arenas[i], p)) {
            return true;
        }
    }
    return false;
}

static bool
arena_pop(Arena *arena, void *p) {
    if ((arena = arena_of(arena, p)) == NULL) {
        return false;
    }

    arena->npushed -= 1;
    if (arena->npushed < 0) {
        error2("Warning: inconsistent arena state (npushed = %lld)\n",
               (llong)arena->npushed);
    }
    if (arena->npushed <= 0) {
        arena->pos = arena->begin;
    }
    return true;
}

static int64
arena_narenas(Arena *arena) {
    int64 n = 0;
    while (arena) {
        n += 1;
        arena = arena->next;
    }
    return n;
}

static void *
arena_reset(Arena *arena) {
    Arena *first = arena;

    if (first == NULL) {
        return NULL;
    }

    do {
        arena->pos = arena->begin;
        arena->npushed = 0;
    } while ((arena = arena->next));

    return first->begin;
}

static void *
arenas_reset(Arena **arenas, int64 number) {
    for (uint32 i = 0; i < number; i += 1) {
        arena_reset(arenas[i]);
    }
    return NULL;
}

static void
arenas_destroy(Arena **arenas, int64 number) {
    for (uint32 i = 0; i < number; i += 1) {
        arena_destroy(arenas[i]);
    }
    return;
}

#if TESTING_arena
#include "assert.c"
#include <stdio.h>

void
memset64(void *buffer, int value, int64 size) {
    if (size == 0) {
        return;
    }
    ASSERT(size >= 0);
    ASSERT_LESS(size, SIZE_MAX);
    memset(buffer, value, (size_t)size);
    return;
}

#define LENGTH(X) ((int64)(sizeof(X) / sizeof(*X)))

int
main(void) {
    Arena *arena;
    char *objs[1000];
    uint32 arena_size;

    ASSERT((arena = arena_create(SIZEMB(3))));
    ASSERT(arena->pos == arena->begin);
    arena_size = (uint32)arena_data_size(arena);

    srand((uint32)time(NULL));

    ASSERT_EQUAL(arena_narenas(arena), 1);

    {
        int64 total_size = 0;
        int64 total_pushed = 0;

        for (uint32 i = 0; i < LENGTH(objs); i += 1) {
            int64 size = 10 + (rand() % 10000);
            ASSERT((objs[i] = arena_push(arena, size)));

            total_size += size;
            memset64(objs[i], 0xCD, size);

            if (total_size < arena_data_size(arena)) {
                ASSERT_EQUAL(arena_narenas(arena), 1);
                ASSERT((char *)objs[i] >= arena->begin);
                ASSERT((char *)arena->pos >= (char *)objs[i]);
            }
        }

        for (Arena *a = arena; a; a = a->next) {
            ASSERT_MORE(a->npushed, 0);
            total_pushed += a->npushed;
        }
        ASSERT_EQUAL(total_pushed, LENGTH(objs));
    }

    {
        int aux;
        uint32 nallocated = LENGTH(objs);

        while (nallocated > 0) {
            uint32 j = (uint32)rand() % LENGTH(objs);
            uint32 k = (uint32)rand() % LENGTH(objs);
            if (objs[j]) {
                ASSERT(arena_pop(arena, objs[j]));
                objs[j] = NULL;
                nallocated -= 1;
            }
            if ((k + 1) < (nallocated / 2)) {
                ASSERT((objs[j] = arena_push(arena, ALIGNMENT)));
                nallocated += 1;
            }
        }
        for (Arena *a = arena; a; a = a->next) {
            ASSERT_EQUAL(a->npushed, 0);
        }

        ASSERT(!arena_pop(arena, &aux));
    }

    arena_reset(arena);
    {
        void *p1;
        void *p2;

        ASSERT((p1 = arena_push(arena, arena_size)));
        ASSERT_EQUAL(arena->npushed, 1);
        ASSERT((p2 = arena_push(arena, arena_size)));
        ASSERT_EQUAL(arena->npushed, 1);
        ASSERT_EQUAL(arena_narenas(arena), 2);
        ASSERT(arena->next);
        ASSERT(arena_of(arena, p1) != arena_of(arena, p2));

        ASSERT(arena_pop(arena, p1));
        ASSERT(arena_pop(arena, p2));
        ASSERT_EQUAL(arena->npushed, 0);
    }

    arena_reset(arena);

    ASSERT(arena_push(arena, arena_size + 1) == NULL);
    error2("Expected error in arena_push: %s.\n", arena_strerror(errno));

    {
        void *p3;
        void *p4;

        ASSERT((p3 = arena_push(arena, arena_size / 2)));
        ASSERT_EQUAL(arena->npushed, 1);
        ASSERT((p4 = arena_push(arena, arena_size / 2)));
        ASSERT_EQUAL(arena->npushed, 2);
        ASSERT(arena_of(arena, p3) == arena_of(arena, p4));

        ASSERT(arena_pop(arena, p3));
        ASSERT_EQUAL(arena->npushed, 1);
        ASSERT(arena_pop(arena, p4));
        ASSERT_EQUAL(arena->npushed, 0);
    }

    arena_reset(arena);
    ASSERT(arena->pos == arena->begin);
    ASSERT_EQUAL(arena->npushed, 0);

    ASSERT(arena->next->pos == arena->next->begin);
    ASSERT_EQUAL(arena->next->npushed, 0);
    ASSERT(arena->pos == arena->begin);
    ASSERT_EQUAL(arena->npushed, 0);

    arena_reset(arena);
    {
        uint32 index = arena_push_index32(arena, 32);
        ASSERT_NOT_EQUAL(index, UINT32_MAX);
        ASSERT(arena->begin + index == arena->begin);

        index = arena_push_index32(arena, 32);
        ASSERT_NOT_EQUAL(index, UINT32_MAX);
        ASSERT(arena->begin + index == arena->begin + 32);
    }

    arena_print(arena);

    arena_destroy(arena);
    return 0;
}
#endif

#endif
