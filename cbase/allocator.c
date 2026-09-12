// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(ALLOCATOR_C)
#define ALLOCATOR_C

#include "cbase.h"

static void *
allocator_default_malloc(int64 size, void *context) {
    (void)context;
    return malloc((size_t)size);
}

static void
allocator_default_free(void *pointer, int64 size, void *context) {
    (void)size;
    (void)context;
    free(pointer);
    return;
}

static void *
allocator_default_realloc(void *pointer,
                          int64 old_size, int64 new_size,
                          void *context) {
    (void)old_size;
    (void)context;
    return realloc(pointer, (size_t)new_size);
}

static void *
allocator_default_realloc_flex(void *pointer, int64 struct_size,
                               int64 old_capacity, int64 new_capacity,
                               int64 object_size, void *context) {
    int64 old_size = struct_size + old_capacity*object_size;
    int64 new_size = struct_size + new_capacity*object_size;
    return allocator_default_realloc(pointer, old_size, new_size, context);
}

Allocator
allocator_default(void) {
    Allocator allocator = {
        .malloc = allocator_default_malloc,
        .free = allocator_default_free,
        .realloc = allocator_default_realloc,
        .realloc_flex = allocator_default_realloc_flex,
        .context = NULL,
    };
    return allocator;
}

#endif /* ALLOCATOR_C */
