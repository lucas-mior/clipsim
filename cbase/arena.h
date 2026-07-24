// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(ARENA_H)
#define ARENA_H

#include "primitives.h"

typedef struct Arena {
    char *name;
    char *begin;
    void *pos;
    int64 size;
    int64 npushed;
    struct Arena *next;
} Arena;

enum ArenaErrors {
    EARENA_INVALID = 2000000,
    EARENA_INVALID_OBJECT,
    EARENA_OBJECT_SIZE,
    EARENA_MORE_THAN_4GB,
    EARENA_LINKED,
    EARENA_SIZE,
};

static void *arena_allocate(int64 *);
static bool arena_free(Arena *);
static Arena *arena_create(int64, char *);
static int64 arena_data_size(Arena *);
static bool arena_decr(Arena *, void *);
static void arena_destroy(Arena *);
static void arena_functions_sink(void);
static int32 arena_nlinked(Arena *);
static Arena *arena_of(Arena *, void *);
static void arena_print(Arena *);
static void *arena_push(Arena *, int64);
static uint32 arena_push_index32(Arena *, uint32);
static void *arena_reset(Arena *);
static char *arena_strerror(int);
static Arena *arena_with_space(Arena *, int64);
static void arenas_destroy(Arena **, int32);
static bool arenas_pop(Arena **, int32, void *);
static void *arenas_push(Arena **, int32, int64);
static void *arenas_reset(Arena **, int32);
static void *xarena_push(Arena *, int64);
static void *xarenas_push(Arena **, int32, int64);

#endif /* ARENA_H */
