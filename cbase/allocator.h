// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(ALLOCATOR_H)
#define ALLOCATOR_H

#include "primitives.h"

typedef void *AllocatorMalloc(int64 size, void *context);
typedef void AllocatorFree(void *pointer, int64 size, void *context);
typedef void *AllocatorRealloc(void *pointer,
                               int64 old_size, int64 new_size,
                               void *context);
typedef void *AllocatorReallocFlex(void *pointer, int64 struct_size,
                                   int64 old_capacity, int64 new_capacity,
                                   int64 object_size, void *context);

typedef struct Allocator {
    AllocatorMalloc *malloc;
    AllocatorFree *free;
    AllocatorRealloc *realloc;
    AllocatorReallocFlex *realloc_flex;
    void *context;
} Allocator;

Allocator allocator_default(void);

#endif /* ALLOCATOR_H */
