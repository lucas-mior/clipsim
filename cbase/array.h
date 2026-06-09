#if !defined(ARRAY_H)
#define ARRAY_H

#include "util.c"

typedef struct GenericArrayHeader {
    max_align_t alignment;
    int32 count;
    int32 cap;
} GenericArrayHeader;

#define ARRAY_HEADER(array) ((GenericArrayHeader *)(array) - 1)
#define ARRAY_LEN(array) ((array) ? ARRAY_HEADER(array)->count : 0)
#define ARRAY_PUSH(array, ...) \
    ((array) = generic_array_grow((array), SIZEOF(*(array))), \
     (array)[ARRAY_HEADER(array)->count++] = (__VA_ARGS__))
#define ARRAY_INIT(array, cap) \
    ((array) = generic_array_init((cap), SIZEOF(*(array))))

static void *
generic_array_init(int32 cap, int64 item_size) {
    GenericArrayHeader *header = NULL;

    header = realloc_flex(header, 0, cap, item_size);
    header->count = 0;
    header->cap = cap;

    return header + 1;
}

static void *
generic_array_grow(void *array, int64 item_size) {
    GenericArrayHeader *header;
    int32 old_cap;
    int32 new_cap;

    if (array) {
        header = ARRAY_HEADER(array);
        if (header->count < header->cap) {
            return array;
        }
        old_cap = header->cap;
        new_cap = old_cap*2;
    } else {
        header = NULL;
        old_cap = 0;
        new_cap = 8;
    }

    header = realloc_flex(header, old_cap, new_cap, item_size);
    if (!array) {
        header->count = 0;
    }
    header->cap = new_cap;

    return header + 1;
}

#endif /* ARRAY_H */
