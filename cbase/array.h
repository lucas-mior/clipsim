#if !defined(ARRAY_H)
#define ARRAY_H

#include <stddef.h>

#include "base_macros.h"
#include "util.c"

typedef struct GenericArrayHeader {
    ldouble alignment;
    int32 count;
    int32 cap;
} GenericArrayHeader;

#define ARRAY_HEADER(array) ((GenericArrayHeader *)(array) - 1)
#define ARRAY_LEN(array) ((array) ? ARRAY_HEADER(array)->count : 0)
#define ARRAY_CLEAR(array) do {                         \
    if (array) {                                        \
        ARRAY_HEADER(array)->count = 0;                 \
    }                                                   \
} while (0)
#define ARRAY_FREE(array) do {                                           \
    if (array) {                                                         \
        GenericArrayHeader *array_header_ = ARRAY_HEADER(array);         \
        free2(array_header_, SIZEOF(*array_header_)                      \
              + array_header_->cap*SIZEOF(*(array)));                    \
        (array) = NULL;                                                  \
    }                                                                    \
} while (0)
#define ARRAY_PUSH(array, ...) \
    ((array) = generic_array_grow((array), SIZEOF(*(array))), \
     (array)[ARRAY_HEADER(array)->count++] = (__VA_ARGS__))
#define ARRAY_INIT(array, cap) \
    ((array) = generic_array_init((cap), SIZEOF(*(array))))

static void *
generic_array_init(int32 cap, int64 item_size) {
    GenericArrayHeader *header;
    int64 size;

    if (cap < 0) {
        cap = 0;
    }

    size = SIZEOF(*header) + cap*item_size;
    header = malloc2(size);
    header->count = 0;
    header->cap = cap;

    return header + 1;
}

static void *
generic_array_grow(void *array, int64 item_size) {
    GenericArrayHeader *header;
    int32 old_cap;
    int32 new_cap;
    int64 old_size;
    int64 new_size;

    if (array) {
        header = ARRAY_HEADER(array);
        if (header->count < header->cap) {
            return array;
        }
        old_cap = header->cap;
        if (old_cap <= (MAXOF(old_cap)/2)) {
            new_cap = old_cap*2;
        } else {
            error("Array is too large.\n");
            fatal(EXIT_FAILURE);
        }
    } else {
        header = NULL;
        old_cap = 0;
        new_cap = 8;
    }

    if ((MAXOF(new_size)/item_size) < new_cap) {
        error("Array with %lld items of size %lld is too much.\n",
              (llong)new_cap, (llong)item_size);
        fatal(EXIT_FAILURE);
    }

    old_size = SIZEOF(*header) + old_cap*item_size;
    new_size = SIZEOF(*header) + new_cap*item_size;
    header = realloc2(header, old_size, new_size, 1);
    if (!array) {
        header->count = 0;
    }
    header->cap = new_cap;

    return header + 1;
}

static inline void
array_sink(void) {
    (void)generic_array_init;
    (void)generic_array_grow;
    return;
}

#endif /* ARRAY_H */
