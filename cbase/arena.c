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

#include "util.c"

#define BYTE_POPED 0xDC
#define BYTE_PUSHED_UNINITIALIZED 0xCD

#if OS_WINDOWS
#include <windows.h>
#endif

#if OS_UNIX
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
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

static Arena *global_arena = NULL;

static void *arena_allocate(int64 *);
static bool arena_free(Arena *);
static bool arena_decr(Arena *arena, void *p);

static int64 arena_page_size = 0;

#if !defined(error2)
#define error2(...) fprintf(stderr, __VA_ARGS__)
#endif

#if !defined(UTIL_C)
static void memset64(void *buffer, int value, int64 size);
#endif

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
    return;
}

enum ArenaErrors {
    EARENA_INVALID = 2000000,
    EARENA_INVALID_OBJECT,
    EARENA_OBJECT_SIZE,
    EARENA_MORE_THAN_4GB,
    EARENA_LINKED,
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
    case EARENA_MORE_THAN_4GB:
        return "Tried to get 32 bit index on arena larger than 4GB of space";
    case EARENA_LINKED:
        return "Tried to get 32 bit index but arena has links";
    default:
        return strerror(arena_errno);
    }
}

static Arena *
arena_create(int64 size, char *name) {
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
    if (name) {
        arena->name = xstrdup(name);
    }
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
        arena_page_size = (int64)aux;
    }

    do {
        if ((*size >= SIZEMB(2)) && FLAGS_HUGE_PAGES) {
            p = mmap(NULL, (size_t)*size, PROT_READ | PROT_WRITE,
                     MAP_ANON | MAP_PRIVATE | FLAGS_HUGE_PAGES, -1, 0);
            if (p != MAP_FAILED) {
                *size = ALIGN_POWER_OF_2(*size, SIZEMB(2));
                break;
            }
        }
        p = mmap(NULL, (size_t)*size, PROT_READ | PROT_WRITE,
                 MAP_ANON | MAP_PRIVATE, -1, 0);
        *size = ALIGN_POWER_OF_2(*size, arena_page_size);
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
    *size = ALIGN_POWER_OF_2(*size, arena_page_size);
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
            arena->next = arena_create(arena->size, NULL);
        }

        arena = arena->next;
    }
    return arena;
}

static void *
arena_push(Arena *arena, int64 size) {
    void *before;
    size = ALIGN(size);

    if ((arena = arena_with_space(arena, size)) == NULL) {
        return NULL;
    }

    before = arena->pos;
    if (DEBUGGING) {
        memset64(before, BYTE_PUSHED_UNINITIALIZED, size);
    }
    arena->pos = (char *)arena->pos + size;
    arena->npushed += 1;
    ASSUME_ALIGNED(before);
    return before;
}

static void *
arenas_push(Arena **arenas, int32 number, int64 size) {
    for (int32 i = 0; i < number; i += 1) {
        void *p;
        if ((p = arena_push(arenas[i], size))) {
            return p;
        }
    }
    return NULL;
}

static void *
xarena_push(Arena *arena, int64 size) {
    void *p;

    if (arena == NULL) {
        if (global_arena == NULL) {
            global_arena = arena_create(SIZEMB(2), "global_arena");
            arena = global_arena;
        } else {
            error2("Error in %s: arena is NULL.\n", __func__);
            fatal(EXIT_FAILURE);
        }
    }

    if ((p = arena_push(arena, size)) == NULL) {
        error2("Error allocating %lld bytes: %s.\n", (llong)size, arena_strerror(errno));
        exit(EXIT_FAILURE);
    }
    return p;
}

static void *
xarenas_push(Arena **arenas, int32 narenas, int64 size) {
    void *p;

    if ((p = arenas_push(arenas, narenas, size)) == NULL) {
        error2("Error pushing %lld bytes into arenas %p: %s.\n", (llong)size,
               (void *)arenas, arena_strerror(errno));
        exit(EXIT_FAILURE);
    }
    return p;
}

static uint32
arena_push_index32(Arena *arena, uint32 size) {
    void *before;
    Arena *arena_save = arena;

    if ((arena = arena_with_space(arena, size)) == NULL) {
        return UINT32_MAX;
    }

    if (arena != arena_save) {
        return EARENA_LINKED;
    }

    if (arena->size >= UINT32_MAX) {
        errno = EARENA_MORE_THAN_4GB;
        return UINT32_MAX;
    }
    before = arena->pos;
    arena->pos = (char *)arena->pos + size;
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
arenas_pop(Arena **arenas, int32 narenas, void *p) {
    for (int32 i = 0; i < narenas; i += 1) {
        if (arena_decr(arenas[i], p)) {
            return true;
        }
    }
    return false;
}

static bool
arena_decr(Arena *arena, void *p) {
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
        if (DEBUGGING) {
            memset64(arena->pos, BYTE_POPED, arena_data_size(arena));
        }
    }
    return true;
}

static int32
arena_nlinked(Arena *arena) {
    int32 n = 0;
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
        if (DEBUGGING) {
            memset64(arena->begin, MEM_FREED, arena_data_size(arena));
        }
    } while ((arena = arena->next));

    return first->begin;
}

static void *
arenas_reset(Arena **arenas, int32 number) {
    for (int32 i = 0; i < number; i += 1) {
        arena_reset(arenas[i]);
    }
    return NULL;
}

static void
arenas_destroy(Arena **arenas, int32 number) {
    for (int32 i = 0; i < number; i += 1) {
        arena_destroy(arenas[i]);
    }
    return;
}

#if 0 == TESTING_arena
static inline void
arena_functions_sink(void) {
    (void)arena_print;
    (void)xarenas_push;
    (void)xarena_push;
    (void)arena_push_index32;
    (void)arenas_pop;
    (void)arena_nlinked;
    (void)arenas_reset;
    (void)arenas_destroy;
    return;
}
#endif

#if TESTING_arena
#include "assert.c"
#include <stdio.h>

#if !defined(UTIL_C)
static void
memset64(void *buffer, int value, int64 size) {
    if (size == 0) {
        return;
    }
    ASSERT(size >= 0);
    ASSERT_LESS(size, SIZE_MAX);
    memset(buffer, value, (size_t)size);
    return;
}
#endif

int
main(void) {
    Arena *arena;
    char *objs[1000];
    uint32 arena_size;

    ASSERT((arena = arena_create(SIZEMB(3), "arena")));
    ASSERT(arena->pos == arena->begin);
    arena_size = (uint32)arena_data_size(arena);

    ASSERT_EQUAL(ALIGN_POWER_OF_2(1, 16), 16);
    ASSERT_EQUAL(ALIGN_POWER_OF_2(2, 16), 16);
    ASSERT_EQUAL(ALIGN_POWER_OF_2(10, 16), 16);
    ASSERT_EQUAL(ALIGN_POWER_OF_2(16, 16), 16);
    ASSERT_EQUAL(ALIGN_POWER_OF_2(17, 16), 32);
    ASSERT_EQUAL(ALIGN_POWER_OF_2(18, 16), 32);

    srand((uint32)time(NULL));

    ASSERT_EQUAL(arena_nlinked(arena), 1);

    {
        int64 total_size = 0;
        int64 total_pushed = 0;

        for (int32 i = 0; i < LENGTH(objs); i += 1) {
            int64 size = ALIGN(1ul + (ulong)(rand() % 10000));
            ASSERT((objs[i] = arena_push(arena, size)));

            total_size += size;
            memset64(objs[i], 0xCD, size);

            if (total_size < arena_data_size(arena)) {
                ASSERT_EQUAL(arena_nlinked(arena), 1);
                ASSERT_MORE_EQUAL((void *)objs[i], (void *)arena->begin);
                ASSERT_MORE_EQUAL((void *)arena->pos, (void *)objs[i]);
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
        int32 nallocated = LENGTH(objs);

        while (nallocated > 0) {
            uint32 j = (uint32)rand() % LENGTH(objs);
            uint32 k = (uint32)rand() % LENGTH(objs);
            if (objs[j]) {
                ASSERT(arena_decr(arena, objs[j]));
                objs[j] = NULL;
                nallocated -= 1;
            }
            if ((k + 1) < (uint32)(nallocated / 2)) {
                ASSERT((objs[j] = arena_push(arena, ALIGNMENT)));
                nallocated += 1;
            }
        }
        for (Arena *a = arena; a; a = a->next) {
            ASSERT_EQUAL(a->npushed, 0);
        }

        ASSERT(!arena_decr(arena, &aux));
    }

    arena_reset(arena);
    {
        void *p1;
        void *p2;

        ASSERT((p1 = arena_push(arena, arena_size)));
        ASSERT_EQUAL(arena->npushed, 1);
        ASSERT((p2 = arena_push(arena, arena_size)));
        ASSERT_EQUAL(arena->npushed, 1);
        ASSERT_EQUAL(arena_nlinked(arena), 2);
        ASSERT(arena->next);
        ASSERT(arena_of(arena, p1) != arena_of(arena, p2));

        ASSERT(arena_decr(arena, p1));
        ASSERT(arena_decr(arena, p2));
        ASSERT_EQUAL(arena->npushed, 0);
    }

    arena_reset(arena);

    ASSERT(arena_push(arena, arena_size + 1) == NULL);
    error2("Expected error in arena_push: %s.\n", arena_strerror(errno));

    {
        void *p3;
        void *p4;

        ASSERT((p3 = arena_push(arena, ALIGN(arena_size / 2))));
        ASSERT_EQUAL(arena->npushed, 1);
        ASSERT((p4 = arena_push(arena, ALIGN(arena_size / 3))));
        ASSERT_EQUAL(arena->npushed, 2);
        ASSERT(arena_of(arena, p3) == arena_of(arena, p4));

        ASSERT(arena_decr(arena, p3));
        ASSERT_EQUAL(arena->npushed, 1);
        ASSERT(arena_decr(arena, p4));
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

    {
        Arena *arenas[2];
        void *first_pointer;
        void *second_pointer;
        void *third_pointer;
        int32 arena_count;
        int64 first_arena_capacity;
        char *error_message;

        arena_count = (int64)LENGTH(arenas);
        ASSERT((arenas[0] = arena_create(SIZEMB(1), "arenas[0]")));
        ASSERT((arenas[1] = arena_create(SIZEMB(1), "arenas[1]")));

        first_arena_capacity = arena_data_size(arenas[0]);

        ASSERT((first_pointer = xarena_push(arenas[0], first_arena_capacity)));
        ASSERT((second_pointer = arenas_push(arenas, arena_count, 100)));

        ASSERT((third_pointer = xarenas_push(arenas, (int32)arena_count, 100)));

        ASSERT(arenas_pop(arenas, (int32)arena_count, first_pointer));
        ASSERT(arenas_pop(arenas, (int32)arena_count, second_pointer));
        ASSERT(arenas_pop(arenas, (int32)arena_count, third_pointer));

        arenas_reset(arenas, arena_count);
        ASSERT_EQUAL(arenas[0]->npushed, 0);
        ASSERT_EQUAL(arenas[1]->npushed, 0);

        error_message = arena_strerror(EARENA_INVALID);
        ASSERT_EQUAL(error_message, "Invalid arena pointer");

        arenas_destroy(arenas, arena_count);
    }

    arena_print(arena);

    arena_destroy(arena);
    return 0;
}
#endif

#endif /* ARENA_C */
