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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "generic.c"
#include "util.c"
#include "arena.c"

#include "base_macros.h"

#if !defined(ENUM_UNDERLYING_TYPE)
  #if __clang__
    #define ENUM_UNDERLYING_TYPE : uint32
  #else
    #define ENUM_UNDERLYING_TYPE
  #endif
#endif

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
  #define TESTING_xenums 1
#elif !defined(TESTING_xenums)
  #define TESTING_xenums 0
#endif

#if TESTING_xenums && !defined(ENUM_NAME)
#define ENUM_NAME TestFlags
#define ENUM_PREFIX_ TEST_FLAGS_
#define ENUM_BITFLAGS 1
#define ENUM_FIELDS \
    X(READ) \
    X(WRITE) \
    X(EXEC)
#endif

#if !defined(__INCLUDE_LEVEL__) || (__INCLUDE_LEVEL__ >= 1) || (TESTING_xenums == 0)
  #if !defined(ENUM_NAME)
    #error "ENUM_NAME is not defined"
  #endif
  #if !defined(ENUM_PREFIX_)
    #error "ENUM_PREFIX_ is not defined"
  #endif
  #if !defined(ENUM_FIELDS)
    #error "ENUM_FIELDS is not defined"
  #endif
  #if !defined(ENUM_BITFLAGS)
    #error "ENUM_BITFLAGS is not defined"
  #endif
#endif

#if ENUM_BITFLAGS
enum CAT(ENUM_NAME, _BitIndices) ENUM_UNDERLYING_TYPE {
    #define X_IDX_1(e)    CAT3(ENUM_PREFIX_, e, _BIT_IDX),
    #define X_IDX_2(e, v) CAT3(ENUM_PREFIX_, e, _BIT_IDX),
    #define X(...)        SELECT_ON_NUM_ARGS(X_IDX_, __VA_ARGS__)

    ENUM_FIELDS

    #undef X
    #undef X_IDX_1
    #undef X_IDX_2
    CAT(ENUM_PREFIX_, BIT_COUNT)
};
#endif

enum ENUM_NAME ENUM_UNDERLYING_TYPE {
#if ENUM_BITFLAGS == 0
    #define XENUM_DEF_1(e)    CAT(ENUM_PREFIX_, e),
    #define XENUM_DEF_2(e, v) CAT(ENUM_PREFIX_, e) = v,
#else
    #define XENUM_DEF_1(e)    CAT(ENUM_PREFIX_, e) = 1 << CAT3(ENUM_PREFIX_, e, _BIT_IDX),
    #define XENUM_DEF_2(e, v) CAT(ENUM_PREFIX_, e) = v,
#endif
    #define X(...)            SELECT_ON_NUM_ARGS(XENUM_DEF_, __VA_ARGS__)

#if ENUM_BITFLAGS
	CAT(ENUM_PREFIX_, NONE) = 0,
#endif
    ENUM_FIELDS

    #undef X
    #undef XENUM_DEF_1
    #undef XENUM_DEF_2
    CAT(ENUM_PREFIX_, LAST)
};

// TODO: When ENUM_BITFLAGS == 1, passing bitwise OR'd integers into a strict
// `enum ENUM_NAME` type could trigger compiler warnings or undefined behavior
// in pedantic modes since the result isn't explicitly defined in the enum.
// Consider changing the parameter type to an integer (e.g., uint32) for
// bitflags.

static char *
CAT(ENUM_PREFIX_, str)(enum ENUM_NAME val) {
#if ENUM_BITFLAGS == 0
    switch (val) {
        #define XENUM_ST_1(e)    case CAT(ENUM_PREFIX_, e): \
                                     return QUOTE(ENUM_PREFIX_) #e;
        #define XENUM_ST_2(e, v) case CAT(ENUM_PREFIX_, e): \
                                     return QUOTE(ENUM_PREFIX_) #e;
        #define X(...)           SELECT_ON_NUM_ARGS(XENUM_ST_, __VA_ARGS__)

        ENUM_FIELDS

        #undef X
        #undef XENUM_ST_1
        #undef XENUM_ST_2
#if ENUM_BITFLAGS
        case CAT(ENUM_PREFIX_, NONE):
            return QUOTE(ENUM_PREFIX_) "NONE";
#endif
        case CAT(ENUM_PREFIX_, LAST):
            return QUOTE(ENUM_PREFIX_) "LAST";
        default:
            return "Unknown value";
    }
#else
    char buffer[CAT(ENUM_PREFIX_, BIT_COUNT)*256 + 1];
    char *buffer_ptr = buffer;
    char *buffer_end = buffer + sizeof(buffer);
    int32 is_first = 1;

    #define XENUM(e) \
        if (val & CAT(ENUM_PREFIX_, e)) { \
            char *name = QUOTE(ENUM_PREFIX_) #e; \
            int32 len = (int32)strlen32(name); \
            if (is_first == 0) { \
                if (buffer_ptr < (buffer_end - 1)) { \
                    *buffer_ptr = '|'; \
                    buffer_ptr += 1; \
                } else { \
                    error2("Error: enum name is too long.\n"); \
                    TRAP(); \
                } \
            } \
            if (buffer_ptr + len < (buffer_end - 1)) { \
                memcpy64(buffer_ptr, name, len); \
                buffer_ptr += len; \
            } else { \
                error2("Error: enum name is too long.\n"); \
                TRAP(); \
            } \
            is_first = 0; \
        }

    #define XENUM_FL_1(e)    XENUM(e)
    #define XENUM_FL_2(e, v) XENUM(e)
    #define X(...)           SELECT_ON_NUM_ARGS(XENUM_FL_, __VA_ARGS__)

    ENUM_FIELDS

    #undef X
    #undef XENUM_FL_1
    #undef XENUM_FL_2
    #undef XENUM

    if (buffer_ptr == buffer) {
        return "NONE";
    }

    *buffer_ptr = '\0';

    {
        int64 final_len = (int64)(buffer_ptr - buffer) + 1;
        char *copy;

        if ((copy = xarena_push(global_arena, final_len))) {
            memcpy64(copy, buffer, final_len);
        }

        return copy;
    }
#endif
}

#if 0 == TESTING_xenums
static inline void
CAT(ENUM_PREFIX_, functions_sink)(void) {
    (void)CAT(ENUM_PREFIX_, str);
    return;
}
#endif

#undef ENUM_NAME
#undef ENUM_PREFIX_
#undef ENUM_FIELDS
#undef ENUM_BITFLAGS

#if TESTING_xenums && !defined(TESTING_xenums_started)
#define TESTING_xenums_started

#include "assert.c"

#define ENUM_NAME TestNormal
#define ENUM_PREFIX_ TEST_NORMAL_
#define ENUM_BITFLAGS 0
#define ENUM_FIELDS \
    X(APPLE) \
    X(BANANA) \
    X(CHERRY, 10)
#include "xenums.c"

int
main(void) {
    char *s;
    ASSERT_EQUAL(TEST_FLAGS_READ_BIT_IDX, 0);
    ASSERT_EQUAL(TEST_FLAGS_READ, 1 << 0);
    ASSERT_EQUAL(TEST_FLAGS_WRITE, 1 << 1);
    ASSERT_EQUAL(TEST_FLAGS_EXEC, 1 << 2);

    s = TEST_FLAGS_str(TEST_FLAGS_READ);
    ASSERT_EQUAL(s, "TEST_FLAGS_READ");
    ASSERT(arena_decr(global_arena, s));

    s = TEST_FLAGS_str(TEST_FLAGS_READ | TEST_FLAGS_EXEC);
    ASSERT_EQUAL(s, "TEST_FLAGS_READ|TEST_FLAGS_EXEC");
    ASSERT(arena_decr(global_arena, s));

    s = TEST_FLAGS_str(TEST_FLAGS_READ | TEST_FLAGS_WRITE | TEST_FLAGS_EXEC);
    ASSERT_EQUAL(s, "TEST_FLAGS_READ|TEST_FLAGS_WRITE|TEST_FLAGS_EXEC");
    ASSERT(arena_decr(global_arena, s));

    ASSERT_EQUAL(TEST_FLAGS_str(0), "NONE");

    s = TEST_NORMAL_str(TEST_NORMAL_APPLE);
    ASSERT_EQUAL(s, "TEST_NORMAL_APPLE");

    s = TEST_NORMAL_str(TEST_NORMAL_BANANA);
    ASSERT_EQUAL(s, "TEST_NORMAL_BANANA");

    s = TEST_NORMAL_str(TEST_NORMAL_CHERRY);
    ASSERT_EQUAL(s, "TEST_NORMAL_CHERRY");

    s = TEST_NORMAL_str(999);
    ASSERT_EQUAL(s, "Unknown value");

    printf("xenums.c: All tests passed successfully.\n");
    return EXIT_SUCCESS;
}

#endif /* TESTING_xenums && !defined(TESTING_xenums_started) */
