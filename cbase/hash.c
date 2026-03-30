/*
 * Copyright (C) 2025 Mior, Lucas;
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the*License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(HASH_H)
#define HASH_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "rapidhash.h"
#include "util.c"
#include "assert.c"

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_hash 1
#elif !defined(TESTING_hash)
#define TESTING_hash 0
#endif

#if 1 == TESTING_hash
#define HASH_VALUE_TYPE int32
#define HASH_VALUE_FORMATTER "%d"
#define HASH_TYPE map
#define HASH_AUTO_RESIZE 1
#endif

#define HASH_SLOT_FREE   0
#define HASH_SLOT_DELETED -1

#if !defined(GREEN)
#define GREEN "\x1b[32m"
#define RESET "\x1b[0m"
#endif

#if !defined(ALIGNMENT)
#define ALIGNMENT 16
#endif

uint64 hash_function(char *key, int32 key_length);
uint32 hash_normal(void *map, uint64 hash);
uint32 hash_capacity(void *map);
uint32 hash_length(void *map);
uint32 hash_expected_collisions(void *map);

#if !defined(INTEGERS)
#define INTEGERS
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long ullong;

typedef long long llong;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
#endif

#if !defined(QUOTE)
#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)
#endif
#define HASH_PRINT_SUMMARY_map(MAP) hash_print_summary_map(MAP, QUOTE(MAP))
#define HASH_PRINT_SUMMARY_set(MAP) hash_print_summary_set(MAP, QUOTE(MAP))

#if !defined(CAT)
#define CAT_(a, b) a##b
#define CAT(a, b) CAT_(a, b)
#endif

struct CommonBucket {
    uint64 hash;
    char *key;
    int32 key_len;
    int32 value;
};

struct CommonMap {
    int64 size;
    uint32 capacity;
    uint32 bitmask;
    uint32 length;
    uint32 occupied;
    struct CommonBucket *array;
};

#endif /* HASH_H */

#if !defined(HASH_TYPE)
#error HASH_TYPE is undefined
#endif

#if !defined(HASH_AUTO_RESIZE)
#error HASH_AUTO_RESIZE is undefined
#endif

#if !defined(HASH_DUPLICATE_KEYS)
#define HASH_DUPLICATE_KEYS 0
#endif

#define Bucket CAT(Bucket_, HASH_TYPE)
#define Map CAT(Hash_, HASH_TYPE)

typedef struct Bucket {
    uint64 hash;
    char *key;
    int32 key_len;
#if defined(HASH_VALUE_TYPE)
    HASH_VALUE_TYPE value;
#endif
#if defined(HASH_PADDING_TYPE)
    HASH_PADDING_TYPE padding;
#endif
} Bucket;

struct Map {
    int64 size;
    uint32 capacity;
    uint32 bitmask;
    uint32 length;
    uint32 occupied;
    Bucket *array;
};

static void
CAT(hash_zero_, HASH_TYPE)(struct Map *map) {
    map->length = 0;
    map->occupied = 0;
    memset64(map->array, 0, map->capacity*sizeof(Bucket));
    return;
}

static struct Map *
CAT(hash_create_, HASH_TYPE)(uint32 length) {
    struct Map *map;
    int64 array_size;
    uint32 capacity = 1;
    uint32 power = 0;

    if (length > (UINT32_MAX / 4)) {
        length = UINT32_MAX / 4;
    }

    while (capacity < length) {
        capacity *= 2;
        power += 1;
    }
    capacity *= 2;
    power += 1;

    array_size = capacity*sizeof(Bucket);

    map = xmalloc(sizeof(*map));
    map->array = xmmap_commit(&array_size);
    map->capacity = capacity;
    map->bitmask = (1 << power) - 1;
    map->size = array_size;
    map->length = 0;
    map->occupied = 0;
    return map;
}

static void
CAT(hash_destroy_, HASH_TYPE)(struct Map *map) {
    if (map == NULL) {
        return;
    }
#if HASH_DUPLICATE_KEYS
    for (uint32 i = 0; i < map->capacity; i += 1) {
        switch ((int64)map->array[i].key) {
        case HASH_SLOT_DELETED:
        case HASH_SLOT_FREE:
            break;
        default:
            free(map->array[i].key, map->array[i].key_len);
            break;
        }
    }
#endif
    xmunmap(map->array, map->size);
    free(map, sizeof(*map));
    return;
}

static bool
CAT(hash_insert_pre_calc_, HASH_TYPE)(struct Map *map,
                                      char *key, int32 key_length,
                                      uint64 hash, uint32 base_index
#if defined(HASH_VALUE_TYPE)
                                      , HASH_VALUE_TYPE value
#endif
) {
    uint32 capacity;
    uint32 i;
    uint32 probe;
    int32 first_tombstone;

    if (HASH_AUTO_RESIZE && (map->occupied*100 >= map->capacity*75)) {
        uint32 new_capacity = map->capacity*2;
        uint32 new_bitmask = (new_capacity - 1);
        int64 new_size = new_capacity*sizeof(Bucket);
        Bucket *new_array = xmmap_commit(&new_size);
        Bucket *old_array = map->array;
        uint32 old_capacity = map->capacity;

        for (uint32 j = 0; j < old_capacity; j += 1) {
            Bucket *iterator = &old_array[j];
            uint32 rehash_base = iterator->hash & new_bitmask;
            uint32 rehash_probe = rehash_base;
            uint32 rehash_step = 0;

            if ((int64)iterator->key == HASH_SLOT_FREE) {
                continue;
            }

            if ((int64)iterator->key == HASH_SLOT_DELETED) {
                continue;
            }

            while (rehash_step < new_capacity) {
                Bucket *target = &new_array[rehash_probe];

                if (target->key == (char *)HASH_SLOT_FREE) {
                    target->key = iterator->key;
                    target->hash = iterator->hash;
#if defined(HASH_VALUE_TYPE)
                    target->value = iterator->value;
#endif
                    break;
                }

                rehash_step += 1;
                rehash_probe = (uint32)(rehash_base
                                + ((uint64)rehash_step
                                   + (uint64)rehash_step*rehash_step) / 2) & new_bitmask;
            }
        }

        xmunmap(old_array, map->size);
        map->array = new_array;
        map->capacity = new_capacity;
        map->bitmask = new_bitmask;
        map->size = new_size;
        map->occupied = map->length;

        base_index = hash & map->bitmask;
    }

    capacity = map->capacity;
    i = 0;
    probe = base_index;
    first_tombstone = -1;

    while (i < capacity) {
        Bucket *iterator = &map->array[probe];

        switch ((int64)iterator->key) {
        case HASH_SLOT_FREE: {
            Bucket *target;
            if (first_tombstone >= 0) {
                target = &map->array[first_tombstone];
            } else {
                target = iterator;
                map->occupied += 1;
            }

#if HASH_DUPLICATE_KEYS
            target->key = xmemdup(key, key_length + 1);
#else
            target->key = key;
#endif
            target->key_len = key_length;
            target->hash = hash;
#if defined(HASH_VALUE_TYPE)
            target->value = value;
#endif
            map->length += 1;
            return true;
        }
        case HASH_SLOT_DELETED:
            if (first_tombstone < 0) {
                first_tombstone = (int32)probe;
            }
            break;
        default:
            if ((iterator->hash == hash) && (strcmp(iterator->key, key) == 0)) {
                return false;
            }
            break;
        }

        i += 1;
        probe = (uint32)(base_index + ((uint64)i + (uint64)i*i) / 2) & map->bitmask;
    }

    if (first_tombstone >= 0) {
        Bucket *target = &map->array[first_tombstone];
#if HASH_DUPLICATE_KEYS
        target->key = xmemdup(key, key_length + 1);
#else
        target->key = key;
#endif
        target->key_len = key_length;
        target->hash = hash;
#if defined(HASH_VALUE_TYPE)
        target->value = value;
#endif
        map->length += 1;
        return true;
    }

    return false;
}

static bool
CAT(hash_insert_, HASH_TYPE)(struct Map *map, char *key,
                             int32 key_length
#if defined(HASH_VALUE_TYPE)
                             , HASH_VALUE_TYPE value
#endif
) {
    uint64 hash = hash_function(key, key_length);
    uint32 index = hash_normal(map, hash);
    return CAT(hash_insert_pre_calc_, HASH_TYPE)(map, key, key_length,
                                                 hash, index
#if defined(HASH_VALUE_TYPE)
                                                 , value
#endif
    );
}

static bool
CAT(hash_insert2_, HASH_TYPE)(struct Map *map, char *key
#if defined(HASH_VALUE_TYPE)
                             , HASH_VALUE_TYPE value
#endif
) {
    int32 key_length = strlen32(key);
    return CAT(hash_insert_, HASH_TYPE)(map, key, key_length
#if defined(HASH_VALUE_TYPE)
                                        , value
#endif
    );
}

#if defined(HASH_VALUE_TYPE)
static HASH_VALUE_TYPE *
#else
static void *
#endif
CAT(hash_lookup_pre_calc_, HASH_TYPE)(struct Map *map,
                                      char *key, uint64 hash, uint32 base_index) {
    uint32 capacity = map->capacity;
    uint32 i = 0;
    uint32 probe = base_index;

    while (i < capacity) {
        Bucket *iterator = &map->array[probe];

        switch ((int64)iterator->key) {
        case HASH_SLOT_FREE:
            return NULL;
        case HASH_SLOT_DELETED:
            break;
        default:
            if ((iterator->hash == hash) && (strcmp(iterator->key, key) == 0)) {
#if defined(HASH_VALUE_TYPE)
                return &(iterator->value);
#else
                return iterator->key;
#endif
            }
        }

        i += 1;
        probe = (uint32)(base_index + ((uint64)i + (uint64)i*i) / 2) & map->bitmask;
    }

    return NULL;
}

static void *
CAT(hash_lookup_, HASH_TYPE)(struct Map *map, char *key, int32 key_length) {
    uint64 hash = hash_function(key, key_length);
    uint32 index = hash_normal(map, hash);
    return CAT(hash_lookup_pre_calc_, HASH_TYPE)(map, key, hash, index);
}

static void *
CAT(hash_lookup2_, HASH_TYPE)(struct Map *map, char *key) {
    int32 key_length = strlen32(key);
    return CAT(hash_lookup_, HASH_TYPE)(map, key, key_length);
}

static bool
CAT(hash_remove_pre_calc_, HASH_TYPE)(struct Map *map,
                                      char *key, uint64 hash, uint32 base_index) {
    uint32 i = 0;
    uint32 probe = base_index;

    if (map == NULL) {
        return false;
    }

    while (i < map->capacity) {
        Bucket *iterator = &map->array[probe];

        switch ((int64)iterator->key) {
        case HASH_SLOT_FREE:
            return false;
        case HASH_SLOT_DELETED:
            break;
        default:
            if ((iterator->hash == hash) && (strcmp(iterator->key, key) == 0)) {
#if HASH_DUPLICATE_KEYS
                free(iterator->key, iterator->key_len);
#endif
                iterator->key = (char *)HASH_SLOT_DELETED;
                map->length -= 1;
                return true;
            }
            break;
        }

        i += 1;
        probe = (uint32)(base_index + ((uint64)i + (uint64)i*i) / 2) & map->bitmask;
    }

    return false;
}

static bool
CAT(hash_remove_, HASH_TYPE)(struct Map *map, char *key, int32 key_length) {
    uint64 hash = hash_function(key, key_length);
    uint32 index = hash_normal(map, hash);
    return CAT(hash_remove_pre_calc_, HASH_TYPE)(map, key, hash, index);
}

static void
CAT(hash_print_summary_, HASH_TYPE)(struct Map *map, char *name) {
    printf("struct Hash%s %s {\n", QUOTE(HASH_TYPE), name);
    printf("  capacity: %u\n", map->capacity);
    printf("  length: %u\n", map->length);
    printf("  expected collisions: %u\n", hash_expected_collisions(map));
    printf("}\n");
    return;
}

static void
CAT(hash_print_, HASH_TYPE)(struct Map *map, bool verbose) {
    if (map == NULL) {
        return;
    }

    for (uint32 i = 0; i < map->capacity; i += 1) {
        Bucket *iterator = &map->array[i];

        if (!verbose) {
            if (iterator->key == (char *)HASH_SLOT_FREE) {
                continue;
            }
            if (iterator->key == (char *)HASH_SLOT_DELETED) {
                continue;
            }
        }

        printf("\n%03u: ", i);

        switch ((int64)iterator->key) {
        case HASH_SLOT_FREE:
            printf("[empty]");
            break;
        case HASH_SLOT_DELETED:
            printf("[deleted]");
            break;
        default:
            printf("'%s'", iterator->key);
#if defined(HASH_VALUE_TYPE) && defined(HASH_VALUE_FORMATTER)
            printf("="HASH_VALUE_FORMATTER, iterator->value);
#endif
        }
    }

    printf("\n");
    return;
}

static uint32
CAT(hash_ndeleted_, HASH_TYPE)(struct Map *map) {
    uint32 ndeleted = 0;
    for (uint32 i = 0; i < map->capacity; i += 1) {
        Bucket *iterator = &map->array[i];
        if (iterator->key == (char *)HASH_SLOT_DELETED) {
            ndeleted += 1;
        }
    }
    return ndeleted;
}

static inline void
CAT(hash_functions_sink_, HASH_TYPE)(void) {
    (void)CAT(hash_zero_, HASH_TYPE);
    (void)CAT(hash_create_, HASH_TYPE);
    (void)CAT(hash_destroy_, HASH_TYPE);
    (void)CAT(hash_insert_pre_calc_, HASH_TYPE);
    (void)CAT(hash_insert_, HASH_TYPE);
    (void)CAT(hash_insert2_, HASH_TYPE);
    (void)CAT(hash_lookup_pre_calc_, HASH_TYPE);
    (void)CAT(hash_lookup_, HASH_TYPE);
    (void)CAT(hash_lookup2_, HASH_TYPE);
    (void)CAT(hash_remove_pre_calc_, HASH_TYPE);
    (void)CAT(hash_remove_, HASH_TYPE);
    (void)CAT(hash_print_summary_, HASH_TYPE);
    (void)CAT(hash_print_, HASH_TYPE);
    (void)CAT(hash_ndeleted_, HASH_TYPE);
}

#undef HASH_VALUE_TYPE
#undef HASH_PADDING_TYPE
#undef HASH_TYPE
#undef HASH_DUPLICATE_KEYS
#undef HASH_VALUE_FORMATTER

#if !defined(HASH_H2)
#define HASH_H2

uint64
hash_function(char *key, int32 key_length) {
    uint64 hash;
    hash = rapidhash(key, (size_t)key_length);
    return hash;
}

uint32
hash_normal(void *map, uint64 hash) {
    struct CommonMap *map2 = map;
    uint32 normal = hash & map2->bitmask;
    return normal;
}

uint32
hash_capacity(void *map) {
    struct CommonMap *map2 = map;
    return map2->capacity;
}

uint32
hash_length(void *map) {
    struct CommonMap *map2 = map;
    return map2->length;
}

uint32
hash_expected_collisions(void *map) {
    struct CommonMap *map2 = map;
    long double n = map2->length;
    long double m = map2->capacity;
    long double result = n - m*(1 - powl((m - 1) / m, n));
    return (uint32)(roundl(result));
}

#endif /* HASH_H2 */

#if TESTING_hash

#if !OS_UNIX
#error "hash.c tests only work on unix systems"
#endif

#include <assert.h>
#include "arena.c"

#define NSTRINGS 500
#define NBYTES ALIGNMENT

typedef struct String {
    char *s;
    int32 len;
    int32 value;
} String;

static String
random_string(Arena *arena, uint32 nbytes) {
    char characters[] = "abcdefghijklmnopqrstuvwxyz1234567890";
    String string;
    int32 size;
    int32 len;

    len = (int32)(nbytes + (uint)rand() % 16u);
    size = len + 1;
    string.s = arena_push(arena, size);

    for (int32 i = 0; i < len; i += 1) {
        int32 c = (int32)((size_t)rand() % (sizeof(characters) - 1));
        string.s[i] = characters[c];
    }
    string.s[len] = '\0';
    string.len = len;
    string.value = rand();

    return string;
}

// flags: -lm
int
main(void) {
    struct timespec t0;
    struct timespec t1;
    struct Hash_map *map;
    Arena *arena;
    String str1 = {.s = "aaaaaaaaaaaaaaaa", .value = 0};
    String str2 = {.s = "bbbbbbbbbbbbbbb", .value = 1};
    String *strings;
    uint32 initial_capacity;

    map = hash_create_map(100);
    arena = arena_create(NBYTES*NSTRINGS);
    strings = xmalloc(NSTRINGS*sizeof(*strings));

    ASSERT(map);
    initial_capacity = map->capacity;

    str1.len = strlen32(str1.s);
    str2.len = strlen32(str2.s);

    ASSERT(hash_insert_map(map, str1.s, str1.len, str1.value));
    ASSERT(!hash_insert_map(map, str1.s, str1.len, 1));
    ASSERT(hash_insert_map(map, str2.s, str2.len, str2.value));

    ASSERT_EQUAL(hash_length(map), 2u);

    ASSERT_NULL(hash_lookup_map(map, "does_not_exist", 14));

    srand(42);
    for (uint32 i = 0; i < NSTRINGS; i += 1) {
        strings[i] = random_string(arena, NBYTES);
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    for (uint32 i = 0; i < NSTRINGS; i += 1) {
        ASSERT(hash_insert_map(map, strings[i].s, strings[i].len,
                               strings[i].value));
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    PRINT_TIMINGS(NSTRINGS, t0, t1, "insertion with resizes");

    ASSERT(map->capacity > initial_capacity);

    for (uint32 i = 0; i < NSTRINGS; i += 1) {
        int32 *stored;
        stored = hash_lookup_map(map, strings[i].s, strings[i].len);
        ASSERT(stored);
        ASSERT_EQUAL(*stored, strings[i].value);
    }

    ASSERT(hash_remove_map(map, strings[0].s, strings[0].len));
    ASSERT_EQUAL(hash_ndeleted_map(map), 1);

    hash_zero_map(map);
    ASSERT_EQUAL(hash_length(map), 0);
    ASSERT_EQUAL(hash_ndeleted_map(map), 0);
    ASSERT_EQUAL(map->occupied, 0);

    for (uint32 i = 0; i < 10; i += 1) {
        ASSERT(hash_insert_map(map, strings[i].s, strings[i].len, strings[i].value));
    }
    ASSERT_EQUAL(hash_length(map), 10);

    hash_destroy_map(map);
    free(strings, NSTRINGS*sizeof(*strings));

    exit(EXIT_SUCCESS);
}

#endif /* TESTING_hash */
