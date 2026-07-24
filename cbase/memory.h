// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(MEMORY_H)
#define MEMORY_H

#include "primitives.h"
#include "base_macros.h"

#if !defined(ALIGNMENT)
#define ALIGNMENT 16
#endif

#define MEMORY_PADDING ((int32)ALIGNMENT)

#if !defined(TESTING_memory)
#define TESTING_memory 0
#endif

#if TESTING_memory
#define DEBUGGING_MEMORY 1
#define MEMORY_CHECK_USE_AFTER_FREE 1
#define MEMORY_CHECK_DOUBLE_FREE 1
#endif

#if !defined(MEMORY_CHECK_USE_AFTER_FREE)
#define MEMORY_CHECK_USE_AFTER_FREE 0
#endif

#if MEMORY_CHECK_USE_AFTER_FREE
#define MEMORY_CHECK_DOUBLE_FREE 1
#endif

#if !defined(MEMORY_CHECK_DOUBLE_FREE)
#define MEMORY_CHECK_DOUBLE_FREE 1
#endif

#if !defined(DEBUGGING_MEMORY)
#define DEBUGGING_MEMORY DEBUGGING
#endif

static void free2_(void *, int64);
static void free_debug(char *, int32, char *, void *, int64);
static void *malloc_debug(char *, int32, char *, int64, bool);
static void memcpy64(void *, void *, int64);
static void memmove64(void *, void *, int64);
static void memory_check(void);
static void memory_functions_sink(void);
static void memset64(void *, int, int64);
static void *realloc4(void *, int64, int64, int64);
static void *realloc_debug(char *, int32, char *, void *, int64, int64, int64);
static void *realloc_flex_debug(
    char *,
    int32,
    char *,
    void *,
    int64,
    int64,
    int64,
    int64
);
static void *xmalloc(int64, bool);
static void *xmemdup(void *, int64);
static void *xmmap_commit(int64 *);
static void xmunmap(void *, int64);
static void *xrealloc(void *, int64);
static char *xstrdup(char *);
static char *xstrndup(char *, int64);

#if DEBUGGING_MEMORY
#define malloc2_zero(SIZE) \
    malloc_debug(__FILE__, __LINE__, FUNC, SIZE, true)
#define malloc2(SIZE) \
    malloc_debug(__FILE__, __LINE__, FUNC, SIZE, false)
#define realloc2(OLD, OLD_CAPACITY, NEW_CAPACITY, OBJECT_SIZE) \
    realloc_debug(__FILE__, __LINE__, FUNC, OLD, OLD_CAPACITY, \
                  NEW_CAPACITY, OBJECT_SIZE)
#define realloc_flex(OLD, OLD_CAPACITY, NEW_CAPACITY, OBJECT_SIZE) \
    realloc_flex_debug(__FILE__, __LINE__, FUNC, OLD, SIZEOF(*(OLD)), \
                       OLD_CAPACITY, NEW_CAPACITY, OBJECT_SIZE)
#define free2(POINTER, SIZE) \
    free_debug(__FILE__, __LINE__, FUNC, POINTER, SIZE)
#else
#define malloc2_zero(SIZE) xmalloc(SIZE, true)
#define malloc2(SIZE) xmalloc(SIZE, false)
#define realloc2(OLD, OLD_CAPACITY, NEW_CAPACITY, OBJECT_SIZE) \
    realloc4(OLD, OLD_CAPACITY, NEW_CAPACITY, OBJECT_SIZE)
#define realloc_flex(OLD, OLD_CAPACITY, NEW_CAPACITY, OBJECT_SIZE) \
    xrealloc(OLD, SIZEOF(*(OLD)) + (OBJECT_SIZE)*(NEW_CAPACITY))
#define free2(POINTER, SIZE) free2_(POINTER, SIZE)
#endif

#endif /* MEMORY_H */
