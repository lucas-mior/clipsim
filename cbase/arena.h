// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(ARENA_H)
#define ARENA_H

#include "primitives.h"
#include "base_macros.h"

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

CBASE_API_DECL Arena *arena_create(int64, char *);
CBASE_API_DECL int64 arena_data_size(Arena *);
CBASE_API_DECL bool arena_decr(Arena *, void *);
CBASE_API_DECL void arena_destroy(Arena *);
CBASE_API_DECL int32 arena_nlinked(Arena *);
CBASE_API_DECL Arena *arena_of(Arena *, void *);
CBASE_API_DECL void arena_print(Arena *);
CBASE_API_DECL void *arena_push(Arena *, int64);
CBASE_API_DECL uint32 arena_push_index32(Arena *, uint32);
CBASE_API_DECL void *arena_reset(Arena *);
CBASE_API_DECL char *arena_strerror(int);
CBASE_API_DECL Arena *arena_with_space(Arena *, int64);
CBASE_API_DECL void arenas_destroy(Arena **, int32);
CBASE_API_DECL bool arenas_pop(Arena **, int32, void *);
CBASE_API_DECL void *arenas_push(Arena **, int32, int64);
CBASE_API_DECL void *arenas_reset(Arena **, int32);
CBASE_API_DECL void *xarena_push(Arena *, int64);
CBASE_API_DECL void *xarenas_push(Arena **, int32, int64);

#endif /* ARENA_H */
