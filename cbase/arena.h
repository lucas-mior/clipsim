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

Arena *arena_create(int64 size, char *name);
int64 arena_data_size(Arena *arena);
bool arena_decr(Arena *arena, void *p);
void arena_destroy(Arena *arena);
int32 arena_nlinked(Arena *arena);
Arena *arena_of(Arena *arena, void *p);
void arena_print(Arena *arena);
void *arena_push(Arena *arena, int64 size);
uint32 arena_push_index32(Arena *arena, uint32 size);
void *arena_reset(Arena *arena);
char *arena_strerror(int arena_errno);
Arena *arena_with_space(Arena *arena, int64 size);
void arenas_destroy(Arena **arenas, int32 number);
bool arenas_pop(Arena **arenas, int32 narenas, void *p);
void *arenas_push(Arena **arenas, int32 number, int64 size);
void *arenas_reset(Arena **arenas, int32 number);
void *xarena_push(Arena *arena, int64 size);
void *xarenas_push(Arena **arenas, int32 narenas, int64 size);

#endif /* ARENA_H */
