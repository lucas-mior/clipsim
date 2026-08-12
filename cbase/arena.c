// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(ARENA_C)
#define ARENA_C

#define BYTE_POPED 0xDC
#define BYTE_PUSHED_UNINITIALIZED 0xCD

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_arena 1
#elif !defined(TESTING_arena)
#define TESTING_arena 0
#endif

#include "cbase.h"

static Arena *global_arena = NULL;

void
arena_print(Arena *arena) {
    while (arena) {
        error2("Arena %p {\n", (void *)arena);
        error2("  name: %s\n", arena->name);
        error2("  begin: %p\n", arena->begin);
        error2("  pos: %p\n", arena->pos);
        error2("  size: %lld\n", arena->size);
        error2("  npushed: %lld\n", arena->npushed);
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

char *
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

Arena *
arena_create(int64 size, char *name) {
    Arena *arena;

    if (size <= 0) {
        errno = EARENA_SIZE;
        return NULL;
    }

    arena = xmmap_commit(&size);
    arena->name = NULL;
    if (name) {
        int64 len = strlen32(name);
        arena->name = xmalloc(len + 1, false);
        memcpy64(arena->name, name, len + 1);
    }
    arena->begin = (char *)arena + ALIGN(sizeof(*arena));
    arena->size = size;
    arena->pos = arena->begin;
    arena->next = NULL;
    arena->npushed = 0;

    return arena;
}

void
arena_destroy(Arena *arena) {
    Arena *next;

    do {
        next = arena->next;
        free(arena->name);
        xmunmap(arena, arena->size);
    } while ((arena = next));

    return;
}

int64
arena_data_size(Arena *arena) {
    int64 size = arena->size - (arena->begin - (char *)arena);
    return size;
}

Arena *
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

void *
arena_push(Arena *arena, int64 size) {
    void *before;
    size = ALIGN(size);

    if ((arena = arena_with_space(arena, size)) == NULL) {
        return NULL;
    }

    before = arena->pos;
    if (DEBUGGING) {
        assert(size >= 0);
        memset64(before, BYTE_PUSHED_UNINITIALIZED, size);
    }
    arena->pos = (char *)arena->pos + size;
    arena->npushed += 1;
    ASSUME_ALIGNED(before);
    return before;
}

void *
arenas_push(Arena **arenas, int32 number, int64 size) {
    for (int32 i = 0; i < number; i += 1) {
        void *p;
        if ((p = arena_push(arenas[i], size))) {
            return p;
        }
    }
    return NULL;
}

void *
xarena_push(Arena *arena, int64 size) {
    void *p;

    if (arena == NULL) {
        if (global_arena == NULL) {
            global_arena = arena_create(SIZEMB(2), "global_arena");
            arena = global_arena;
        } else {
            error("arena is NULL.\n");
            fatal(EXIT_FAILURE);
        }
    }

    if ((p = arena_push(arena, size)) == NULL) {
        error2("Error allocating %lld bytes: %s.\n",
               size, arena_strerror(errno));
        exit(EXIT_FAILURE);
    }
    return p;
}

void *
xarenas_push(Arena **arenas, int32 narenas, int64 size) {
    void *p;

    if ((p = arenas_push(arenas, narenas, size)) == NULL) {
        error2("Error pushing %lld bytes into arenas %p: %s.\n", size,
               (void *)arenas, arena_strerror(errno));
        exit(EXIT_FAILURE);
    }
    return p;
}

uint32
arena_push_index32(Arena *arena, uint32 size) {
    void *before;
    Arena *arena_save = arena;

    if ((arena = arena_with_space(arena, size)) == NULL) {
        return UINT32_MAX;
    }

    if (arena != arena_save) {
        errno = EARENA_LINKED;
        return UINT32_MAX;
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

Arena *
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

bool
arenas_pop(Arena **arenas, int32 narenas, void *p) {
    for (int32 i = 0; i < narenas; i += 1) {
        if (arena_decr(arenas[i], p)) {
            return true;
        }
    }
    return false;
}

bool
arena_decr(Arena *arena, void *p) {
    if ((arena = arena_of(arena, p)) == NULL) {
        return false;
    }

    arena->npushed -= 1;
    if (arena->npushed < 0) {
        error2("Warning: inconsistent arena state (npushed = %lld)\n",
               arena->npushed);
    }
    if (arena->npushed <= 0) {
        arena->pos = arena->begin;
        if (DEBUGGING) {
            memset64(arena->pos, BYTE_POPED, arena_data_size(arena));
        }
    }
    return true;
}

int32
arena_nlinked(Arena *arena) {
    int32 n = 0;
    while (arena) {
        n += 1;
        arena = arena->next;
    }
    return n;
}

void *
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

void *
arenas_reset(Arena **arenas, int32 number) {
    for (int32 i = 0; i < number; i += 1) {
        arena_reset(arenas[i]);
    }
    return NULL;
}

void
arenas_destroy(Arena **arenas, int32 number) {
    for (int32 i = 0; i < number; i += 1) {
        arena_destroy(arenas[i]);
    }
    return;
}

#if 0 == TESTING_arena
static inline void
arena_functions_sink(void) {
    (void)arena_functions_sink;
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
#define CBASE_IMPLEMENT
#include "cbase.h"
// flags: -lm

int
main(void) {
    Arena *arena;
    char *objs[1000];
    uint32 arena_size;

    {
        Arena *small_arena;

        ASSERT((small_arena = arena_create(1, "small_arena")));
        ASSERT_MORE(small_arena->size, ALIGN(sizeof(*small_arena)));
        arena_destroy(small_arena);
    }

    ASSERT((arena = arena_create(SIZEMB(3), "arena")));
    ASSERT(arena->pos == arena->begin);
    arena_size = (uint32)arena_data_size(arena);

    ASSERT_EQUAL(ALIGN_POWER_OF_2(1, 16), 16);
    ASSERT_EQUAL(ALIGN_POWER_OF_2(2, 16), 16);
    ASSERT_EQUAL(ALIGN_POWER_OF_2(10, 16), 16);
    ASSERT_EQUAL(ALIGN_POWER_OF_2(16, 16), 16);
    ASSERT_EQUAL(ALIGN_POWER_OF_2(17, 16), 32);
    ASSERT_EQUAL(ALIGN_POWER_OF_2(18, 16), 32);

    ASSERT_EQUAL(arena_nlinked(arena), 1);

    {
        int64 total_size = 0;
        int64 total_pushed = 0;

        for (int32 i = 0; i < LENGTH(objs); i += 1) {
            int64 size = ALIGN(1 + (rand_int() % 10000u));
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
            ASSERT_POSITIVE(a->npushed);
            total_pushed += a->npushed;
        }
        ASSERT_EQUAL(total_pushed, LENGTH(objs));
    }

    {
        int aux;
        int32 nallocated = LENGTH(objs);

        while (nallocated > 0) {
            int32 j = (int32)(rand_int() % (uint32)LENGTH(objs));
            int32 k = (int32)(rand_int() % (uint32)LENGTH(objs));
            if (objs[j]) {
                ASSERT(arena_decr(arena, objs[j]));
                objs[j] = NULL;
                nallocated -= 1;
            }
            if ((k + 1) < (nallocated / 2)) {
                ASSERT((objs[j] = arena_push(arena, ALIGNMENT)));
                nallocated += 1;
            }
        }
        for (Arena *a = arena; a; a = a->next) {
            ASSERT_ZERO(a->npushed);
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
        ASSERT_ZERO(arena->npushed);
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
        ASSERT_ZERO(arena->npushed);
    }

    arena_reset(arena);
    ASSERT(arena->pos == arena->begin);
    ASSERT_ZERO(arena->npushed);

    ASSERT(arena->next->pos == arena->next->begin);
    ASSERT_ZERO(arena->next->npushed);
    ASSERT(arena->pos == arena->begin);
    ASSERT_ZERO(arena->npushed);

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
        ASSERT_ZERO(arenas[0]->npushed);
        ASSERT_ZERO(arenas[1]->npushed);

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
