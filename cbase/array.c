// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(ARRAY_C)
#define ARRAY_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_array 1
#elif !defined(TESTING_array)
#define TESTING_array 0
#endif

#include "cbase.h"

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
    if (array == NULL) {
        header->count = 0;
    }
    header->cap = new_cap;

    return header + 1;
}

static void
array_sink(void) {
    (void)generic_array_init;
    (void)generic_array_grow;
    return;
}

#if TESTING_array
#define CBASE_IMPLEMENT
#include "cbase.h"

int
main(void) {
    array_sink();
    exit(EXIT_SUCCESS);
}
#endif

#endif /* ARRAY_C */
