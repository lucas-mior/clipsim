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

#if CC_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif

#if !defined(ENUM_UNDERLYING_TYPE)
  #define ENUM_UNDERLYING_TYPE uint32
#endif

#if CC_CLANG
  #define ENUM_UNDERLYING_TYPE_SPEC : ENUM_UNDERLYING_TYPE
#else
  #define ENUM_UNDERLYING_TYPE_SPEC
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
    X(TEST_FLAGS_READ) \
    X(TEST_FLAGS_WRITE) \
    X(TEST_FLAGS_EXEC)
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
enum CAT(ENUM_NAME, _BitIndices) ENUM_UNDERLYING_TYPE_SPEC {
    #define X_IDX_1(e)    CAT(e, _BIT_IDX),
    #define X_IDX_2(e, v)
    #define X(...)        SELECT_ON_NUM_ARGS(X_IDX_, __VA_ARGS__)

    ENUM_FIELDS

    #undef X
    #undef X_IDX_1
    #undef X_IDX_2
    CAT(ENUM_PREFIX_, BIT_COUNT)
};
_Static_assert(CAT(ENUM_PREFIX_, BIT_COUNT)
               <= (sizeof(ENUM_UNDERLYING_TYPE)*CHAR_BIT),
               "bit flag enum does not fit in the underlying integer");
#endif

_Static_assert((ENUM_UNDERLYING_TYPE)-1 > 0,
               "enum underlying type must be unsigned");

// Note: passing numbers to the X macro second parameter is not allowed for the
// BITFLAGS case. It will break the API. You can only passing composition of
// previous enum values.
//
// Passing multiple ENUM names for the same value will break compilation.
enum ENUM_NAME ENUM_UNDERLYING_TYPE_SPEC {
#if ENUM_BITFLAGS == 0
    #define XENUM_DEF_1(e)    e,
    #define XENUM_DEF_2(e, v) e = v,
#else
    #define XENUM_DEF_1(e)    e = (ENUM_UNDERLYING_TYPE)1 << CAT(e, _BIT_IDX),
    #define XENUM_DEF_2(e, v) e = v,
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

static void
CAT(ENUM_PREFIX_, str_free)(char *str) {
    (void)str;
#if ENUM_BITFLAGS
    free2(str, optional_strlen32(str) + 1);
#endif
    return;
}

static char *
CAT(ENUM_PREFIX_, str)(enum ENUM_NAME val) {
#if ENUM_BITFLAGS == 0
    switch (val) {
        #define XENUM_ST_1(e)    case e: \
                                     return #e;
        #define XENUM_ST_2(e, v) case e: \
                                     return #e;
        #define X(...)           SELECT_ON_NUM_ARGS(XENUM_ST_, __VA_ARGS__)

        ENUM_FIELDS

        #undef X
        #undef XENUM_ST_1
        #undef XENUM_ST_2
        case CAT(ENUM_PREFIX_, LAST):
            return QUOTE(ENUM_PREFIX_) "LAST";
        default:
            return "Invalid enum value";
    }
#else
    char *buffer = NULL;
    int32 buffer_len = 0;
    int32 buffer_cap = 0;
    int32 is_first = 1;

    if (val == 0) {
        return xstrdup("NONE");
    }

    #define XENUM_EXACT(e) \
        if (val == e) { \
            return xstrdup(#e); \
        }
    #define XENUM_EXACT_1(e)    XENUM_EXACT(e)
    #define XENUM_EXACT_2(e, v) XENUM_EXACT(e)
    #define X(...)              SELECT_ON_NUM_ARGS(XENUM_EXACT_, __VA_ARGS__)

    ENUM_FIELDS

    #undef X
    #undef XENUM_EXACT_1
    #undef XENUM_EXACT_2
    #undef XENUM_EXACT

    #define XENUM(e) \
        if (val && ((val & e) == e)) { \
            char *name = #e; \
            int32 len = strlen32(name); \
            int32 new_cap; \
            new_cap = buffer_len + len + 1; \
            if (is_first == 0) { \
                new_cap += 1; \
            } \
            buffer = realloc2(buffer, buffer_cap, new_cap, SIZEOF(*buffer)); \
            buffer_cap = new_cap; \
            if (is_first == 0) { \
                buffer[buffer_len] = '|'; \
                buffer_len += 1; \
            } \
            memcpy64(buffer + buffer_len, name, len); \
            buffer_len += len; \
            is_first = 0; \
            val &= ~e; \
        }

    #define XENUM_FL_1(e)    XENUM(e)
    #define XENUM_FL_2(e, v) XENUM(e)
    #define X(...)           SELECT_ON_NUM_ARGS(XENUM_FL_, __VA_ARGS__)

    ENUM_FIELDS

    #undef X
    #undef XENUM_FL_1
    #undef XENUM_FL_2
    #undef XENUM

    if (val) {
        error2("Error: bit flags enum contains invalid bit set.\n");
        TRAP();
    }

    buffer[buffer_len] = '\0';
    return buffer;
#endif
}

static bool
CAT(ENUM_PREFIX_, token_equals)(char *token, int32 token_len, char *name) {
    int32 name_len = strlen32(name);
    if (token_len != name_len) {
        return false;
    }
    return !strncmp32(token, name, token_len);
}

static bool
CAT(ENUM_PREFIX_, token_equals_enum_name)(char *token, int32 token_len,
                                          char *name) {
    char *prefix = QUOTE(ENUM_PREFIX_);
    int32 prefix_len = strlen32(prefix);

    if (CAT(ENUM_PREFIX_, token_equals)(token, token_len, name)) {
        return true;
    }
    if (strncmp32(name, prefix, prefix_len)) {
        return false;
    }
    return CAT(ENUM_PREFIX_, token_equals)(token, token_len, name + prefix_len);
}

static enum ENUM_NAME
CAT(ENUM_PREFIX_, parse)(char *string) {
    ENUM_UNDERLYING_TYPE result = 0;
    char *p = string;

    if (p == NULL) {
        return (enum ENUM_NAME)0;
    }

    while (*p != '\0') {
        char *token;
        int32 token_len;
        int32 matched = 0;

        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'
               || *p == '|' || *p == '(' || *p == ')') {
            p += 1;
        }
        if (*p == '\0') {
            break;
        }

        token = p;
        while (is_ident_char(*p)) {
            p += 1;
        }
        token_len = (int32)(p - token);
        if (token_len <= 0) {
            error2("Error: invalid enum parse character '%c' in %s.\n", *p,
                   string);
            TRAP();
        }

#if ENUM_BITFLAGS
        if (CAT(ENUM_PREFIX_, token_equals)(token, token_len,
                                            QUOTE(ENUM_PREFIX_) "NONE")
            || CAT(ENUM_PREFIX_, token_equals)(token, token_len, "NONE")) {
            matched = 1;
        }
#endif

#if ENUM_BITFLAGS == 0
        if (CAT(ENUM_PREFIX_, token_equals)(token, token_len,
                                            QUOTE(ENUM_PREFIX_) "LAST")
            || CAT(ENUM_PREFIX_, token_equals)(token, token_len, "LAST")) {
            result |= (ENUM_UNDERLYING_TYPE)CAT(ENUM_PREFIX_, LAST);
            matched = 1;
        }
#endif

        #define XENUM_PARSE_ONE(e) \
            if (!matched \
                && CAT(ENUM_PREFIX_, token_equals_enum_name)(token, token_len, \
                                                            #e)) { \
                result |= (ENUM_UNDERLYING_TYPE)e; \
                matched = 1; \
            }
        #define XENUM_PARSE_1(e)    XENUM_PARSE_ONE(e)
        #define XENUM_PARSE_2(e, v) XENUM_PARSE_ONE(e)
        #define X(...)              SELECT_ON_NUM_ARGS(XENUM_PARSE_, __VA_ARGS__)

        ENUM_FIELDS

        #undef X
        #undef XENUM_PARSE_1
        #undef XENUM_PARSE_2
        #undef XENUM_PARSE_ONE

        if (!matched) {
            error2("Error: unknown enum token '%.*s' while parsing %s.\n",
                   token_len, token, string);
            TRAP();
        }
    }

    return (enum ENUM_NAME)result;
}

#if 0 == TESTING_xenums
static inline void
CAT(ENUM_PREFIX_, functions_sink)(void) {
    (void)CAT(ENUM_PREFIX_, str);
    (void)CAT(ENUM_PREFIX_, str_free);
    (void)CAT(ENUM_PREFIX_, parse);
    return;
}
#endif

#undef ENUM_NAME
#undef ENUM_PREFIX_
#undef ENUM_FIELDS
#undef ENUM_BITFLAGS
#undef ENUM_UNDERLYING_TYPE
#undef ENUM_UNDERLYING_TYPE_SPEC

#if TESTING_xenums && !defined(TESTING_xenums_started)
#define TESTING_xenums_started

#include "assert.c"

#define ENUM_NAME TestNormal
#define ENUM_PREFIX_ TEST_NORMAL_
#define ENUM_BITFLAGS 0
#define ENUM_FIELDS \
    X(TEST_NORMAL_APPLE) \
    X(TEST_NORMAL_BANANA) \
    X(TEST_NORMAL_CHERRY, 10)
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
    TEST_FLAGS_str_free(s);

    s = TEST_FLAGS_str(TEST_FLAGS_READ | TEST_FLAGS_EXEC);
    ASSERT_EQUAL(s, "TEST_FLAGS_READ|TEST_FLAGS_EXEC");
    TEST_FLAGS_str_free(s);

    s = TEST_FLAGS_str(TEST_FLAGS_READ | TEST_FLAGS_WRITE | TEST_FLAGS_EXEC);
    ASSERT_EQUAL(s, "TEST_FLAGS_READ|TEST_FLAGS_WRITE|TEST_FLAGS_EXEC");
    TEST_FLAGS_str_free(s);

    s = TEST_FLAGS_str(0);
    ASSERT_EQUAL(s, "NONE");
    TEST_FLAGS_str_free(s);

    ASSERT_EQUAL((uint32)TEST_FLAGS_parse("TEST_FLAGS_READ"), TEST_FLAGS_READ);
    ASSERT_EQUAL((uint32)TEST_FLAGS_parse("TEST_FLAGS_READ | TEST_FLAGS_EXEC"),
                 TEST_FLAGS_READ | TEST_FLAGS_EXEC);
    ASSERT_EQUAL((uint32)TEST_FLAGS_parse("READ|WRITE"),
                 TEST_FLAGS_READ | TEST_FLAGS_WRITE);
    ASSERT_EQUAL((uint32)TEST_FLAGS_parse("NONE"), TEST_FLAGS_NONE);

    s = TEST_NORMAL_str(TEST_NORMAL_APPLE);
    ASSERT_EQUAL(s, "TEST_NORMAL_APPLE");
    TEST_NORMAL_str_free(s);

    s = TEST_NORMAL_str(TEST_NORMAL_BANANA);
    ASSERT_EQUAL(s, "TEST_NORMAL_BANANA");
    TEST_NORMAL_str_free(s);

    s = TEST_NORMAL_str(TEST_NORMAL_CHERRY);
    ASSERT_EQUAL(s, "TEST_NORMAL_CHERRY");
    TEST_NORMAL_str_free(s);

    ASSERT_EQUAL((uint32)TEST_NORMAL_parse("TEST_NORMAL_APPLE"), TEST_NORMAL_APPLE);
    ASSERT_EQUAL((uint32)TEST_NORMAL_parse("BANANA"), TEST_NORMAL_BANANA);
    ASSERT_EQUAL((uint32)TEST_NORMAL_parse("TEST_NORMAL_CHERRY"), TEST_NORMAL_CHERRY);

    s = TEST_NORMAL_str(999);
    ASSERT_EQUAL(s, "Invalid enum value");
    TEST_NORMAL_str_free(s);

    printf("xenums.c: All tests passed successfully.\n");
    return EXIT_SUCCESS;
}

#endif /* TESTING_xenums && !defined(TESTING_xenums_started) */

#if CC_CLANG
#pragma clang diagnostic pop
#endif
