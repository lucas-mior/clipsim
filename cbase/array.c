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

CBASE_API_DEF void *
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

CBASE_API_DEF void *
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
        error("Array with %d items of size %lld is too much.\n",
              new_cap, item_size);
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

static int32
generic_array_next_capacity(int32 old_cap, int32 needed_count) {
    int32 new_cap;

    if (old_cap <= 0) {
        return needed_count;
    }

    new_cap = old_cap;
    while (needed_count > new_cap) {
        if (new_cap > (MAXOF(new_cap)/2)) {
            error("Array is too large.\n");
            fatal(EXIT_FAILURE);
        }
        new_cap *= 2;
    }

    return new_cap;
}

CBASE_API_DEF bool
generic_array_reserve(void **array, int32 needed_count, int64 item_size) {
    GenericArrayHeader *header;
    int32 old_cap;
    int32 old_count;
    int32 new_cap;
    int64 old_size;
    int64 new_size;

    if (array == NULL) {
        return false;
    }
    if (needed_count < 0) {
        return false;
    }
    if (item_size <= 0) {
        return false;
    }
    if (needed_count == 0) {
        return true;
    }
    if (*array == NULL) {
        *array = generic_array_init(needed_count, item_size);
        return true;
    }

    header = ARRAY_HEADER(*array);
    if (needed_count <= header->cap) {
        return true;
    }

    old_cap = header->cap;
    old_count = header->count;
    new_cap = generic_array_next_capacity(old_cap, needed_count);
    if (((MAXOF(new_size) - SIZEOF(*header))/item_size) < new_cap) {
        error("Array with %d items of size %lld is too much.\n",
              new_cap, item_size);
        fatal(EXIT_FAILURE);
    }

    old_size = SIZEOF(*header) + old_cap*item_size;
    new_size = SIZEOF(*header) + new_cap*item_size;
    header = realloc2(header, old_size, new_size, 1);
    header->count = old_count;
    header->cap = new_cap;
    *array = header + 1;

    return true;
}

CBASE_API_DEF int32
generic_array_capacity(void *array) {
    if (array == NULL) {
        return 0;
    }

    return ARRAY_HEADER(array)->cap;
}

CBASE_API_DEF void
generic_array_set_count(void *array, int32 count) {
    if (array == NULL) {
        return;
    }

    ARRAY_HEADER(array)->count = count;

    return;
}

static inline void
array_sink(void) {
    (void)array_sink;
    (void)generic_array_init;
    (void)generic_array_grow;
    (void)generic_array_reserve;
    (void)generic_array_capacity;
    (void)generic_array_set_count;
    return;
}

#if TESTING_array
#define CBASE_IMPLEMENT
#include "cbase.h"

static void
array_test_reserve_and_counts(void) {
    int32 *items = NULL;
    int32 *other = NULL;
    int32 old_cap;

    ASSERT(ARRAY_RESERVE(items, 3));
    ASSERT(items);
    ASSERT(ARRAY_LEN(items) == 0);
    ASSERT(ARRAY_CAPACITY(items) == 3);

    ARRAY_PUSH(items, 10);
    ARRAY_PUSH(items, 11);
    ARRAY_PUSH(items, 12);
    old_cap = ARRAY_CAPACITY(items);

    ASSERT(ARRAY_RESERVE(items, old_cap + 1));
    ASSERT(ARRAY_LEN(items) == 3);
    ASSERT(ARRAY_CAPACITY(items) >= (old_cap + 1));
    ASSERT(items[0] == 10);
    ASSERT(items[1] == 11);
    ASSERT(items[2] == 12);

    ARRAY_SET_COUNT(items, 2);
    ASSERT(ARRAY_LEN(items) == 2);

    ARRAY_INIT_COUNT(other, 4);
    ASSERT(other);
    ASSERT(ARRAY_LEN(other) == 4);
    ASSERT(ARRAY_CAPACITY(other) == 4);

    ARRAY_FREE(items);
    ARRAY_FREE(other);

    return;
}

int
main(void) {
    array_sink();
    array_test_reserve_and_counts();
    exit(EXIT_SUCCESS);
}
#endif

#endif /* ARRAY_C */
