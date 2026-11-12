// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(MEMORY_H)
#define MEMORY_H

#include "platform_detection.h"
#include "warnings.h"
#include "primitives.h"
#include "base_macros.h"

#define MEM_FREED 0xDC
#define MEM_MALLOCED_UNINITIALIZED 0xCD
#define MEM_DONT_READ 0xBD

#define MEMORY_PADDING ((int32)32)

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

void memcpy64(void *dest, void *source, int64 n);
void memmove64(void *dest, void *source, int64 n);
void memset64(void *buffer, int value, int64 size);

void memory_check(void);
void free2_(void *pointer, int64 size);
void free_debug(char *file, int32 line, char *func,
                void *pointer, int64 size);
void *malloc_debug(char *file, int32 line, char *func,
                   int64 size, bool zero);
void *realloc4(void *old,
               int64 old_capacity, int64 new_capacity, int64 obj_size);
void *realloc_debug(char *file, int32 line, char *func,
                    void *old, int64 old_capacity, int64 new_capacity,
                    int64 obj_size);
void *realloc_flex_debug(char *file, int32 line, char *func,
                         void *old, int64 struct_size,
                         int64 old_capacity, int64 new_capacity,
                         int64 obj_size);

void *xmalloc(int64 size, bool zero);
void *xmemdup(void *source, int64 size);
void *xmmap_commit(int64 *size);
void xmunmap(void *p, int64 size);
void *xrealloc(void *old, int64 new_size);
char *xstrdup(char *string);
char *xstrndup(char *s, int64 n);

#if DEBUGGING_MEMORY
  #define malloc2_zero(SIZE)                                                   \
      malloc_debug(__FILE__, __LINE__, FUNC, SIZE, true)
  #define malloc2(SIZE)                                                        \
      malloc_debug(__FILE__, __LINE__, FUNC, SIZE, false)
  #define realloc2(OLD, OLD_CAPACITY, NEW_CAPACITY, OBJECT_SIZE)               \
      realloc_debug(__FILE__, __LINE__, FUNC, OLD, OLD_CAPACITY,               \
                    NEW_CAPACITY, OBJECT_SIZE)
  #define realloc_flex(OLD, OLD_CAPACITY, NEW_CAPACITY, OBJECT_SIZE)           \
      realloc_flex_debug(__FILE__, __LINE__, FUNC, OLD, SIZEOF(*(OLD)),        \
                         OLD_CAPACITY, NEW_CAPACITY, OBJECT_SIZE)
  #define free2(POINTER, SIZE)                                                 \
      free_debug(__FILE__, __LINE__, FUNC, POINTER, SIZE)
#else
  #define malloc2_zero(SIZE) xmalloc(SIZE, true)
  #define malloc2(SIZE) xmalloc(SIZE, false)
  #define realloc2(OLD, OLD_CAPACITY, NEW_CAPACITY, OBJECT_SIZE)               \
      realloc4(OLD, OLD_CAPACITY, NEW_CAPACITY, OBJECT_SIZE)
  #define realloc_flex(OLD, OLD_CAPACITY, NEW_CAPACITY, OBJECT_SIZE)           \
      xrealloc(OLD, SIZEOF(*(OLD)) + (OBJECT_SIZE)*(NEW_CAPACITY))
  #define free2(POINTER, SIZE) free2_(POINTER, SIZE)
#endif

#endif /* MEMORY_H */
