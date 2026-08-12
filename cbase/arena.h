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

extern Arena *arena_create(int64, char *);
extern int64 arena_data_size(Arena *);
extern bool arena_decr(Arena *, void *);
extern void arena_destroy(Arena *);
extern int32 arena_nlinked(Arena *);
extern Arena *arena_of(Arena *, void *);
extern void arena_print(Arena *);
extern void *arena_push(Arena *, int64);
extern uint32 arena_push_index32(Arena *, uint32);
extern void *arena_reset(Arena *);
extern char *arena_strerror(int);
extern Arena *arena_with_space(Arena *, int64);
extern void arenas_destroy(Arena **, int32);
extern bool arenas_pop(Arena **, int32, void *);
extern void *arenas_push(Arena **, int32, int64);
extern void *arenas_reset(Arena **, int32);
extern void *xarena_push(Arena *, int64);
extern void *xarenas_push(Arena **, int32, int64);

#endif /* ARENA_H */
