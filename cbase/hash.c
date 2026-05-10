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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "rapidhash.h"
#include "util.h"
#include "assert.c"
#include "arena.c"

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_hash 1
#elif !defined(TESTING_hash)
#define TESTING_hash 0
#endif

#if 1 == TESTING_hash
#define HASH_KEY_TYPE char
#define HASH_KEY_FORMATTER "%s"
#define HASH_VALUE_TYPE int32
#define HASH_VALUE_FORMATTER "%d"
#define HASH_TYPE map
#define HASH_DUPLICATE_KEYS 1
#endif

#define HASH_SLOT_USED     1
#define HASH_SLOT_FREE     0
#define HASH_SLOT_DELETED -1

INLINE uint64 hash_function(void *key, int32 key_length);
INLINE uint32 hash_normal(void *map, uint64 hash);
INLINE uint32 hash_capacity(void *map);
INLINE uint32 hash_length(void *map);
uint32 hash_expected_collisions(void *map);

#define HASH_PRINT_SUMMARY_map(MAP) hash_print_summary_map(MAP, QUOTE(MAP))
#define HASH_PRINT_SUMMARY_set(MAP) hash_print_summary_set(MAP, QUOTE(MAP))

struct CommonBucket;

typedef struct CommonMap {
    char *name;
    int64 size;
    uint32 capacity;
    uint32 bitmask;
    uint32 length;
    uint32 occupied;
    struct CommonBucket *array;
} CommonMap;

#endif /* HASH_H */

#if !defined(HASH_TYPE)
#error HASH_TYPE is undefined
#endif

#if !defined(HASH_KEY_TYPE)
#error HASH_KEY_TYPE is undefined
#endif

#if !defined(HASH_KEY_FIXED_LEN)
#define HASH_KEY_FIXED_LEN 0
#endif

#if !defined(HASH_DUPLICATE_KEYS)
#define HASH_DUPLICATE_KEYS 0
#endif

#define Bucket CAT(Bucket_, HASH_TYPE)
#define Map CAT(Hash_, HASH_TYPE)

typedef struct Bucket {
    uint64 hash;
#if HASH_KEY_FIXED_LEN
    HASH_KEY_TYPE key;
    int8 slot_state;
    uint8 padding2[3];
#else
    HASH_KEY_TYPE *key;
    int32 key_len;
#endif
#if defined(HASH_PADDING_TYPE2)
    HASH_PADDING_TYPE2 padding3;
#endif
#if defined(HASH_VALUE_TYPE)
    HASH_VALUE_TYPE value;
#endif
#if defined(HASH_PADDING_TYPE)
    HASH_PADDING_TYPE padding;
#endif
} Bucket;

// TODO: Struct `Map` is not typedef'd. Per your codebase rules ("do typedef
// structs"), define it as `typedef struct Map { ... } Map;`.
struct Map {
    char *name;
    int64 size;
    uint32 capacity;
    uint32 bitmask;
    uint32 length;
    uint32 occupied;
#if HASH_DUPLICATE_KEYS
    Arena *arena_keys;
#endif
    Bucket *array;
};

#define CHECK_COMMON_MAP(FIELD) \
    _Static_assert(OFFSET_OF(struct Map, FIELD) == OFFSET_OF(CommonMap, FIELD), \
                   "CommonMap and new Map must have the same offset for " #FIELD)

CHECK_COMMON_MAP(size);
CHECK_COMMON_MAP(capacity);
CHECK_COMMON_MAP(bitmask);
CHECK_COMMON_MAP(length);
CHECK_COMMON_MAP(occupied);

#undef CHECK_COMMON_MAP

static void
CAT(hash_print_summary_, HASH_TYPE)(struct Map *map) {
    printf("struct Hash%s {\n", QUOTE(HASH_TYPE));
    printf("  name: %s\n", map->name);
    printf("  size: %lldB\n", (llong)map->size);
    printf("  capacity: %u\n", map->capacity);
    printf("  bitmask: %u\n", map->bitmask);
    printf("  length: %u\n", map->length);
#if HASH_DUPLICATE_KEYS
    printf("  arena:\n");
    arena_print(map->arena_keys);
#endif
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
#if HASH_KEY_FIXED_LEN
            if (iterator->slot_state == HASH_SLOT_FREE) {
                continue;
            }
            if (iterator->slot_state == HASH_SLOT_DELETED) {
                continue;
            }
#else
            if ((int64)iterator->key == HASH_SLOT_FREE) {
                continue;
            }
            if ((int64)iterator->key == HASH_SLOT_DELETED) {
                continue;
            }
#endif
        }

        printf("\n%03u: ", i);

#if HASH_KEY_FIXED_LEN
        switch (iterator->slot_state)
#else
        switch ((int64)iterator->key)
#endif
        {
        case HASH_SLOT_FREE:
            printf("[empty]");
            break;
        case HASH_SLOT_DELETED:
            printf("[deleted]");
            break;
        default:
#if defined(HASH_KEY_FORMATTER)
            printf("'" HASH_KEY_FORMATTER "'", iterator->key);
#else
            printf("key");
#endif
#if defined(HASH_VALUE_TYPE) && defined(HASH_VALUE_FORMATTER)
            printf("=" HASH_VALUE_FORMATTER, iterator->value);
#endif
            break;
        }
    }

    printf("\n");
    return;
}

static void
CAT(hash_zero_, HASH_TYPE)(struct Map *map) {
    map->length = 0;
    map->occupied = 0;
    memset64(map->array, 0, map->capacity*sizeof(Bucket));
#if HASH_DUPLICATE_KEYS
    arena_reset(map->arena_keys);
#endif
    return;
}

static struct Map *
CAT(hash_create_, HASH_TYPE)(uint32 length, char *name) {
    struct Map *map;
    int64 array_size;
    uint32 capacity = 1;
    uint32 power = 0;
    int64 name_len;

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
    name_len = strlen32(name);
    map->name = xmalloc(name_len + 1);
    memcpy64(map->name, name, name_len + 1);
    map->array = xmmap_commit(&array_size);
    map->capacity = capacity;
    map->bitmask = (1 << power) - 1;
    map->size = array_size;
    map->length = 0;
    map->occupied = 0;
#if HASH_DUPLICATE_KEYS
    {
        char buffer[256];
        SNPRINTF(buffer, "%s->arena_keys", map->name);
        map->arena_keys = arena_create(SIZEMB(2), buffer);
    }
#endif
    if (DEBUGGING) {
        CAT(hash_print_summary_, HASH_TYPE)(map);
    }
    return map;
}

static void
CAT(hash_destroy_, HASH_TYPE)(struct Map *map) {
    if (map == NULL) {
        return;
    }
#if !HASH_KEY_FIXED_LEN && HASH_DUPLICATE_KEYS
    arena_destroy(map->arena_keys);
#endif
    xmunmap(map->array, map->size);
    free(map);
    return;
}

static void
CAT(hash_resize_, HASH_TYPE)(struct Map *map) {
    uint32 new_capacity = map->capacity*2;
    uint32 new_bitmask = (new_capacity - 1);
    int64 new_size = new_capacity*sizeof(Bucket);
    Bucket *new_array = xmmap_commit(&new_size);
    Bucket *old_array = map->array;
    uint32 old_capacity = map->capacity;

    /* if (DEBUGGING) { */
    /*     error("Resizing hash table \"%s\"... %u -> %u\n", */
    /*           map->name, old_capacity, new_capacity); */
    /* } */

    for (uint32 j = 0; j < old_capacity; j += 1) {
        Bucket *iterator = &old_array[j];
        // TODO: Initialize `rehash_base` and `rehash_probe` at declaration
        // below to reduce uninitialized state branching.
        uint32 rehash_base;
        uint32 rehash_probe;
        uint32 rehash_step = 0;

#if HASH_KEY_FIXED_LEN
        if (iterator->slot_state == HASH_SLOT_FREE) {
            continue;
        }
        if (iterator->slot_state == HASH_SLOT_DELETED) {
            continue;
        }
#else
        if ((int64)iterator->key == HASH_SLOT_FREE) {
            continue;
        }
        if ((int64)iterator->key == HASH_SLOT_DELETED) {
            continue;
        }
#endif

        rehash_base = iterator->hash & new_bitmask;
        rehash_probe = rehash_base;

        while (rehash_step < new_capacity) {
            Bucket *target = &new_array[rehash_probe];

#if HASH_KEY_FIXED_LEN
            if (target->slot_state == HASH_SLOT_FREE) {
                memcpy64(&target->key, &iterator->key, sizeof(HASH_KEY_TYPE));
                target->slot_state = HASH_SLOT_USED;
                target->hash = iterator->hash;
  #if defined(HASH_VALUE_TYPE)
                target->value = iterator->value;
  #endif
                break;
            }
#else
            if ((int64)target->key == HASH_SLOT_FREE) {
                target->key = iterator->key;
                target->key_len = iterator->key_len;
                target->hash = iterator->hash;
  #if defined(HASH_VALUE_TYPE)
                target->value = iterator->value;
  #endif
                break;
            }
#endif

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

    /* if (DEBUGGING) { */
    /*     error("Hash table resized.\n"); */
    /* } */

    return;
}

static bool
CAT(hash_probe_, HASH_TYPE)(struct Map *map, HASH_KEY_TYPE *key
#if !HASH_KEY_FIXED_LEN
                            , int32 key_length
#endif
                            , uint64 hash, uint32 base_index, uint32 *out_idx
) {
    uint32 capacity = map->capacity;
    uint32 i = 0;
    uint32 probe = base_index;
    int32 first_tombstone = -1;

    while (i < capacity) {
        Bucket *iterator = &map->array[probe];
#if HASH_KEY_FIXED_LEN
        int64 state = iterator->slot_state;
#else
        int64 state = (int64)iterator->key;
#endif

        if (state == HASH_SLOT_FREE) {
            if (first_tombstone >= 0) {
                *out_idx = (uint32)first_tombstone;
            } else {
                *out_idx = probe;
            }
            return false;
        } else if (state == HASH_SLOT_DELETED) {
            if (first_tombstone < 0) {
                first_tombstone = (int32)probe;
            }
        } else {
#if HASH_KEY_FIXED_LEN
            (void)hash;
            if (!memcmp64(&iterator->key, key, sizeof(HASH_KEY_TYPE)))
#else
            if ((iterator->hash == hash)
                    && (iterator->key_len == key_length)
                    && !memcmp64(iterator->key, key, key_length))
#endif
            {
                *out_idx = probe;
                return true;
            }
        }

        i += 1;
        probe = (uint32)(base_index + ((uint64)i + (uint64)i*i) / 2) & map->bitmask;
    }

    if (first_tombstone >= 0) {
        *out_idx = (uint32)first_tombstone;
    }

    return false;
}


static bool
CAT(hash_insert_pre_calc_, HASH_TYPE)(struct Map *map,
                                      HASH_KEY_TYPE *key
#if !HASH_KEY_FIXED_LEN
                                      , int32 key_length
#endif
                                      , uint64 hash, uint32 base_index
#if defined(HASH_VALUE_TYPE)
                                      , HASH_VALUE_TYPE value
#endif
) {
    uint32 target_idx = MAXOF(target_idx);
    Bucket *target;

    if (map->occupied*100ll >= map->capacity*75ll) {
        CAT(hash_resize_, HASH_TYPE)(map);
        base_index = hash & map->bitmask;
    }

#if HASH_KEY_FIXED_LEN
    if (CAT(hash_probe_, HASH_TYPE)(map, key, hash, base_index, &target_idx))
#else
    if (CAT(hash_probe_, HASH_TYPE)(map, key, key_length, hash, base_index, &target_idx))
#endif
    {
        return false;
    }

    target = &map->array[target_idx];

#if HASH_KEY_FIXED_LEN
    if (target->slot_state == HASH_SLOT_FREE) {
        map->occupied += 1;
    }
    memcpy64(&target->key, key, sizeof(HASH_KEY_TYPE));
    target->slot_state = HASH_SLOT_USED;
    target->hash = hash;
#else
    if ((int64)target->key == HASH_SLOT_FREE) {
        map->occupied += 1;
    }
  #if HASH_DUPLICATE_KEYS
    target->key = xarena_push(map->arena_keys, key_length + 1);
    memcpy64(target->key, key, key_length + 1);
  #else
    target->key = key;
  #endif
    target->key_len = key_length;
    target->hash = hash;
#endif

#if defined(HASH_VALUE_TYPE)
    target->value = value;
#endif
    map->length += 1;
    return true;
}

static bool
CAT(hash_insert_, HASH_TYPE)(struct Map *map, HASH_KEY_TYPE *key
#if !HASH_KEY_FIXED_LEN
                             , int32 key_length
#endif
#if defined(HASH_VALUE_TYPE)
                             , HASH_VALUE_TYPE value
#endif
) {

#if HASH_KEY_FIXED_LEN
    int32 key_length = sizeof(HASH_KEY_TYPE);
#endif
    uint64 hash = hash_function(key, key_length);
    uint32 index = hash_normal(map, hash);
    return CAT(hash_insert_pre_calc_, HASH_TYPE)(map, key
#if !HASH_KEY_FIXED_LEN
                                                 , key_length
#endif
                                                 , hash, index
#if defined(HASH_VALUE_TYPE)
                                                 , value
#endif
    );
}


#if defined(HASH_VALUE_TYPE)
/* only define overwrite functions for HashMaps, not for HashSets */

static bool
CAT(hash_overwrite_pre_calc_, HASH_TYPE)(struct Map *map, HASH_KEY_TYPE *key
#if !HASH_KEY_FIXED_LEN
                                         , int32 key_length
#endif
                                         , uint64 hash, uint32 base_index, HASH_VALUE_TYPE value
) {
    uint32 target_idx = MAXOF(target_idx);
    Bucket *target;

    if (map->occupied*100ll >= map->capacity*75ll) {
        CAT(hash_resize_, HASH_TYPE)(map);
        base_index = hash & map->bitmask;
    }

#if HASH_KEY_FIXED_LEN
    if (CAT(hash_probe_, HASH_TYPE)(map, key, hash, base_index, &target_idx))
#else
    if (CAT(hash_probe_, HASH_TYPE)(map, key, key_length, hash, base_index, &target_idx))
#endif
    {
        target = &map->array[target_idx];
        target->value = value;
        return true;
    }

    target = &map->array[target_idx];

#if HASH_KEY_FIXED_LEN
    if (target->slot_state == HASH_SLOT_FREE) {
        map->occupied += 1;
    }
    memcpy64(&target->key, key, sizeof(HASH_KEY_TYPE));
    target->slot_state = HASH_SLOT_USED;
    target->hash = hash;
#else
    if ((int64)target->key == HASH_SLOT_FREE) {
        map->occupied += 1;
    }
  #if HASH_DUPLICATE_KEYS
    target->key = xarena_push(map->arena_keys, key_length + 1);
    memcpy64(target->key, key, key_length + 1);
  #else
    target->key = key;
  #endif
    target->key_len = key_length;
    target->hash = hash;
#endif

    target->value = value;
    map->length += 1;
    return true;
}

static bool
CAT(hash_overwrite_, HASH_TYPE)(struct Map *map, HASH_KEY_TYPE *key
#if !HASH_KEY_FIXED_LEN
                                , int32 key_length
#endif
                                , HASH_VALUE_TYPE value
) {
#if HASH_KEY_FIXED_LEN
    int32 key_length = sizeof(HASH_KEY_TYPE);
#endif
    uint64 hash = hash_function(key, key_length);
    uint32 index = hash_normal(map, hash);
    return CAT(hash_overwrite_pre_calc_, HASH_TYPE)(map, key
#if !HASH_KEY_FIXED_LEN
                                                    , key_length
#endif
                                                    , hash, index, value);
}

#endif /* HASH_VALUE_TYPE (only define overwrite functions for HashMaps, not for HashSets) */

static bool
CAT(hash_lookup_pre_calc_, HASH_TYPE)(struct Map *map,
                                      HASH_KEY_TYPE *key
#if !HASH_KEY_FIXED_LEN
                                      , int32 key_length
#endif
                                      , uint64 hash, uint32 base_index
#if defined(HASH_VALUE_TYPE)
                                      , HASH_VALUE_TYPE *value_ptr
#endif
) {
    uint32 target_idx;

#if HASH_KEY_FIXED_LEN
    if (CAT(hash_probe_, HASH_TYPE)(map, key, hash, base_index, &target_idx))
#else
    if (CAT(hash_probe_, HASH_TYPE)(map, key, key_length, hash, base_index, &target_idx))
#endif
    {
#if defined(HASH_VALUE_TYPE)
        *value_ptr = map->array[target_idx].value;
#endif
        return true;
    }

    return false;
}

static bool
CAT(hash_lookup_, HASH_TYPE)(struct Map *map, HASH_KEY_TYPE *key
#if !HASH_KEY_FIXED_LEN
                             , int32 key_length
#endif
#if defined(HASH_VALUE_TYPE)
                             , HASH_VALUE_TYPE *value_ptr
#endif
) {
#if HASH_KEY_FIXED_LEN
    int32 key_length = sizeof(HASH_KEY_TYPE);
#endif
    uint64 hash = hash_function(key, key_length);
    uint32 index = hash_normal(map, hash);
    return CAT(hash_lookup_pre_calc_, HASH_TYPE)(map, key
#if !HASH_KEY_FIXED_LEN
                                                 , key_length
#endif
                                                 , hash, index
#if defined(HASH_VALUE_TYPE)
                                                 , value_ptr
#endif
    );
}


static bool
CAT(hash_remove_pre_calc_, HASH_TYPE)(struct Map *map,
                                      HASH_KEY_TYPE *key
#if !HASH_KEY_FIXED_LEN
                                      , int32 key_length
#endif
                                      , uint64 hash, uint32 base_index)
{
    uint32 target_idx;
    Bucket *target;

    if (map == NULL) {
        return false;
    }

#if HASH_KEY_FIXED_LEN
    if (CAT(hash_probe_, HASH_TYPE)(map, key, hash, base_index, &target_idx)) {
        target = &map->array[target_idx];
        target->slot_state = HASH_SLOT_DELETED;
        map->length -= 1;
        return true;
    }
#else
    if (CAT(hash_probe_, HASH_TYPE)(map, key, key_length, hash, base_index, &target_idx)) {
        target = &map->array[target_idx];
  #if HASH_DUPLICATE_KEYS
        arena_decr(map->arena_keys, target->key);
  #endif
        target->key = (HASH_KEY_TYPE *)(int64)HASH_SLOT_DELETED;
        map->length -= 1;
        return true;
    }
#endif

    return false;
}

static bool
CAT(hash_remove_, HASH_TYPE)(struct Map *map, HASH_KEY_TYPE *key
#if !HASH_KEY_FIXED_LEN
                             , int32 key_length
#endif
) {
#if HASH_KEY_FIXED_LEN
    int32 key_length = sizeof(HASH_KEY_TYPE);
#endif
    uint64 hash = hash_function(key, key_length);
    uint32 index = hash_normal(map, hash);
    return CAT(hash_remove_pre_calc_, HASH_TYPE)(map, key
#if !HASH_KEY_FIXED_LEN
                                                 , key_length
#endif
                                                 , hash, index);
}

static uint32
CAT(hash_ndeleted_, HASH_TYPE)(struct Map *map) {
    uint32 ndeleted = 0;
    for (uint32 i = 0; i < map->capacity; i += 1) {
        Bucket *iterator = &map->array[i];
#if HASH_KEY_FIXED_LEN
        if (iterator->slot_state == HASH_SLOT_DELETED)
#else
        if ((int64)iterator->key == HASH_SLOT_DELETED)
#endif
        {
            ndeleted += 1;
        }
    }
    return ndeleted;
}

static inline void
CAT(hash_functions_sink_, HASH_TYPE)(void) {
    (void)CAT(hash_functions_sink_, HASH_TYPE);
    (void)CAT(hash_zero_, HASH_TYPE);
    (void)CAT(hash_create_, HASH_TYPE);
    (void)CAT(hash_destroy_, HASH_TYPE);
    (void)CAT(hash_resize_, HASH_TYPE);
    (void)CAT(hash_probe_, HASH_TYPE);
    (void)CAT(hash_insert_pre_calc_, HASH_TYPE);
    (void)CAT(hash_insert_, HASH_TYPE);
#if defined(HASH_VALUE_TYPE)
    (void)CAT(hash_overwrite_pre_calc_, HASH_TYPE);
    (void)CAT(hash_overwrite_, HASH_TYPE);
#endif
    (void)CAT(hash_lookup_pre_calc_, HASH_TYPE);
    (void)CAT(hash_lookup_, HASH_TYPE);
    (void)CAT(hash_remove_pre_calc_, HASH_TYPE);
    (void)CAT(hash_remove_, HASH_TYPE);
    (void)CAT(hash_print_summary_, HASH_TYPE);
    (void)CAT(hash_print_, HASH_TYPE);
    (void)CAT(hash_ndeleted_, HASH_TYPE);
    return;
}

#undef HASH_VALUE_TYPE
#undef HASH_PADDING_TYPE
#undef HASH_TYPE
#undef HASH_DUPLICATE_KEYS
#undef HASH_VALUE_FORMATTER
#undef HASH_KEY_TYPE
#undef HASH_KEY_FORMATTER
#undef HASH_KEY_FIXED_LEN

#if !defined(HASH_H2)
#define HASH_H2

INLINE uint64
hash_function(void *key, int32 key_length) {
    uint64 hash;
    if (DEBUGGING) {
        ASSERT_MORE(key_length, 0);
    }
    hash = rapidhash(key, (size_t)key_length);
    return hash;
}

INLINE uint32
hash_normal(void *map, uint64 hash) {
    CommonMap *map2 = map;
    uint32 normal = hash & map2->bitmask;
    return normal;
}

INLINE uint32
hash_capacity(void *map) {
    CommonMap *map2 = map;
    return map2->capacity;
}

INLINE uint32
hash_length(void *map) {
    CommonMap *map2 = map;
    return map2->length;
}

uint32
hash_expected_collisions(void *map) {
    CommonMap *map2 = map;
    double n = map2->length;
    double m = map2->capacity;
    double result = n - m*(1 - pow((m - 1) / m, n));
    return (uint32)(round(result));
}

#endif /* HASH_H2 */

#if TESTING_hash && !defined(TESTING_hash_started)
#define TESTING_hash_started

#if !OS_UNIX
#error "hash.c tests only work on unix systems"
#endif

#include <assert.h>
#include "arena.c"
#include "util.c"

// Have to add these declarations so that clangd does not complain
struct Hash_map_by_value;
static struct Hash_map_by_value *hash_create_map_by_value(uint32, char *);
static void hash_destroy_map_by_value(struct Hash_map_by_value *);
static uint32 hash_ndeleted_map_by_value(struct Hash_map_by_value *);
static bool hash_insert_map_by_value(struct Hash_map_by_value *, int64 *, int32);
static bool hash_overwrite_map_by_value(struct Hash_map_by_value *, int64 *, int32);
static bool hash_lookup_map_by_value(struct Hash_map_by_value *, int64 *, int32 *);
static bool hash_remove_map_by_value(struct Hash_map_by_value *, int64 *);

#define HASH_KEY_TYPE int64
#define HASH_KEY_FIXED_LEN 1
#define HASH_VALUE_TYPE int32
#define HASH_VALUE_FORMATTER "%d"
#define HASH_TYPE map_by_value
#define HASH_DUPLICATE_KEYS 0
#include "hash.c"

#define NSTRINGS 10000
#define NBYTES 100*ALIGNMENT

typedef struct String {
    char *s;
    int32 len;
    int32 value;
} String;

static String
random_string(Arena *arena, uint32 nbytes) {
    char characters[] = "abcdefghijklmnopqrstuvwxyz1234567890";
    String string;
    int32 len = (int32)(nbytes + (uint)rand() % 16u);
    int32 size = len + 1;

    string.s = arena_push(arena, size);

    for (int32 i = 0; i < len; i += 1) {
        int32 ci = (int32)((size_t)rand() % (sizeof(characters) - 1));
        string.s[i] = characters[ci];
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
    struct Hash_map *map = hash_create_map(100, "strings_map");
    Arena *arena = arena_create(NBYTES*NSTRINGS, "strings_arena");
    String *strings = malloc(NSTRINGS*sizeof(*strings));
    String str1 = {.s = "aaaaaaaaaaaaaaaa", .value = 10};
    String str2 = {.s = "bbbbbbbbbbbbbbb", .value = 20};
    uint32 initial_capacity;
    int32 test = 0;

    ASSERT(map);
    initial_capacity = map->capacity;

    str1.len = strlen32(str1.s);
    str2.len = strlen32(str2.s);

    ASSERT(hash_insert_map(map, str1.s, str1.len, str1.value));
    ASSERT(!hash_insert_map(map, str1.s, str1.len, 1));
    ASSERT(hash_insert_map(map, str2.s, str2.len, str2.value));
    ASSERT_EQUAL(hash_length(map), 2u);

    ASSERT(hash_overwrite_map(map, str1.s, str1.len, 555));
    ASSERT_EQUAL(hash_length(map), 2u);
    ASSERT(hash_lookup_map(map, str1.s, str1.len, &test));
    ASSERT_EQUAL(test, 555);

    ASSERT(hash_overwrite_map(map, "new_key", 7, 777));
    ASSERT_EQUAL(hash_length(map), 3u);
    ASSERT(hash_lookup_map(map, "new_key", 7, &test));
    ASSERT_EQUAL(test, 777);
    arena_print(map->arena_keys);
    hash_print_summary_map(map);
    ASSERT_EQUAL(map->arena_keys->npushed, map->length);

    ASSERT(!hash_lookup_map(map, "does_not_exist", 14, &test));

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
        int32 stored = 0;
        ASSERT(hash_lookup_map(map, strings[i].s, strings[i].len, &stored));
        ASSERT_EQUAL(stored, strings[i].value);
    }

    ASSERT(hash_remove_map(map, strings[0].s, strings[0].len));
    ASSERT_EQUAL(hash_ndeleted_map(map), 1);

    hash_zero_map(map);
    ASSERT_EQUAL(hash_length(map), 0);
    ASSERT_EQUAL(hash_ndeleted_map(map), 0);
    ASSERT_EQUAL(map->occupied, 0);

    for (uint32 i = 0; i < 10; i += 1) {
        ASSERT(hash_insert_map(map,
                               strings[i].s, strings[i].len, strings[i].value));
    }
    ASSERT_EQUAL(hash_length(map), 10);

    hash_destroy_map(map);
    free(strings);

    {
        struct Hash_map_by_value *map2;
        int64 key1 = 12345;
        int64 key2 = 67890;
        int64 key3 = 55555;
        int32 value1 = 99;
        int32 value2 = 100;
        int32 test2 = 0;
        int64 missing_key = 999;

        map2 = hash_create_map_by_value(16, "value_map");
        ASSERT(hash_insert_map_by_value(map2, &key1, value1));
        ASSERT(!hash_insert_map_by_value(map2, &key1, 1));
        ASSERT(hash_insert_map_by_value(map2, &key2, value2));

        ASSERT_EQUAL(hash_length(map2), 2u);

        ASSERT(hash_overwrite_map_by_value(map2, &key1, 888));
        ASSERT_EQUAL(hash_length(map2), 2u);
        ASSERT(hash_lookup_map_by_value(map2, &key1, &test2));
        ASSERT_EQUAL(test2, 888);

        ASSERT(hash_overwrite_map_by_value(map2, &key3, 333));
        ASSERT_EQUAL(hash_length(map2), 3u);
        ASSERT(hash_lookup_map_by_value(map2, &key3, &test2));
        ASSERT_EQUAL(test2, 333);

        ASSERT(hash_lookup_map_by_value(map2, &key1, &test2));
        ASSERT_EQUAL(test2, 888);

        ASSERT(!hash_lookup_map_by_value(map2, &missing_key, &test2));

        ASSERT(hash_remove_map_by_value(map2, &key1));
        ASSERT_EQUAL(hash_ndeleted_map_by_value(map2), 1);

        hash_destroy_map_by_value(map2);
    }

    exit(EXIT_SUCCESS);
}

#endif /* TESTING_hash && !defined(TESTING_hash_started) */
