// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#include "base_macros.h"

#if CC_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wfixed-enum-extension"
#endif

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_xenums 1
#elif !defined(TESTING_xenums)
#define TESTING_xenums 0
#endif

#if !defined(CBASE_H)
  #if defined(ENUM_NAME) || defined(ENUM_PREFIX_)                              \
      || defined(ENUM_FIELDS) || defined(ENUM_BITFLAGS)                        \
      || defined(ENUM_UNDERLYING_TYPE)
    #error "include cbase.h before configuring xenums.c"
  #endif
#include "cbase.h"
#endif

#if !defined(ENUM_UNDERLYING_TYPE)
  #define ENUM_UNDERLYING_TYPE uint32
#endif

#if CC_CLANG
  #define ENUM_UNDERLYING_TYPE_SPEC : ENUM_UNDERLYING_TYPE
#else
  #define ENUM_UNDERLYING_TYPE_SPEC
#endif

#if TESTING_xenums && !defined(ENUM_NAME)
#define ENUM_NAME TestFlags
#define ENUM_PREFIX_ TEST_FLAGS_
#define ENUM_BITFLAGS 1
#define ENUM_FIELDS                                                            \
    XX(TEST_FLAGS_READ)                                                        \
    XX(TEST_FLAGS_WRITE)                                                       \
    XX(TEST_FLAGS_EXEC)                                                        \
    XX(TEST_FLAGS_READ_WRITE, TEST_FLAGS_READ|TEST_FLAGS_WRITE)
#endif

#if !defined(__INCLUDE_LEVEL__) || (__INCLUDE_LEVEL__ >= 1)                    \
    || (TESTING_xenums == 0)
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

#if !defined(XENUMS_DECLARE_ONLY)
#define XENUMS_DECLARE_ONLY 0
#endif

#if !defined(XENUMS_FUNCTIONS_ONLY)
#define XENUMS_FUNCTIONS_ONLY 0
#endif

#if XENUMS_DECLARE_ONLY || XENUMS_FUNCTIONS_ONLY
#define XENUMS_LINKAGE
#else
#define XENUMS_LINKAGE static inline
#endif

#if XENUMS_FUNCTIONS_ONLY == 0

#if ENUM_BITFLAGS
enum CAT(ENUM_NAME, _BitIndices) ENUM_UNDERLYING_TYPE_SPEC {
    #define XX_1(e)    CAT(e, _BIT_INDEX),
    #define XX_2(e, v)
    #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

    ENUM_FIELDS

    #undef XX
    #undef XX_1
    #undef XX_2
    CAT(ENUM_PREFIX_, BIT_COUNT)
};
_Static_assert(CAT(ENUM_PREFIX_, BIT_COUNT)
               <= (sizeof(ENUM_UNDERLYING_TYPE)*CHAR_BIT),
               "bit flag enum does not fit in the underlying integer");
#endif

_Static_assert((ENUM_UNDERLYING_TYPE)-1 > 0,
               "enum underlying type must be unsigned");

#if ENUM_BITFLAGS && CC_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wduplicate-enum"
#endif

// For bit flag enums, the optional second X macro parameter is a value.
// Only use compositions of previous enum values there, not numeric values.
// For non-bit flag enums, the optional second X macro parameter is a parse
// alias token. Custom numeric values are not supported for non-bit flag enums.
//
// Passing multiple ENUM names for the same value will break compilation.
enum ENUM_NAME ENUM_UNDERLYING_TYPE_SPEC {
#if ENUM_BITFLAGS
    CAT(ENUM_PREFIX_, NONE) = 0,
#endif

#if ENUM_BITFLAGS == 0
    #define XX_1(e)        e,
    #define XX_2(e, alias) e,
#else
    #define XX_1(e)        e = (ENUM_UNDERLYING_TYPE)1 << CAT(e, _BIT_INDEX),
    #define XX_2(e, v)     e = v,
#endif
    #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

    ENUM_FIELDS

    #undef XX
    #undef XX_1
    #undef XX_2

#if ENUM_BITFLAGS
    CAT(ENUM_PREFIX_, LAST)
#else
    CAT(ENUM_PREFIX_, COUNT)
#endif
};

#endif

XENUMS_LINKAGE void CAT(ENUM_PREFIX_, str_free)(char *);
XENUMS_LINKAGE int32 CAT(ENUM_PREFIX_, str_len)(enum ENUM_NAME, char **);
XENUMS_LINKAGE char *CAT(ENUM_PREFIX_, str)(enum ENUM_NAME);
XENUMS_LINKAGE void CAT(ENUM_PREFIX_, alias_free)(char *);
XENUMS_LINKAGE int32 CAT(ENUM_PREFIX_, alias_len)(enum ENUM_NAME, char **);
XENUMS_LINKAGE char *CAT(ENUM_PREFIX_, alias)(enum ENUM_NAME);
XENUMS_LINKAGE enum ENUM_NAME CAT(ENUM_PREFIX_, parse)(char *);

#if XENUMS_DECLARE_ONLY == 0
XENUMS_LINKAGE void
CAT(ENUM_PREFIX_, str_free)(char *str) {
    (void)str;
#if ENUM_BITFLAGS
    free2(str, optional_strlen32(str) + 1);
#endif
    return;
}

XENUMS_LINKAGE void
CAT(ENUM_PREFIX_, alias_free)(char *str) {
    CAT(ENUM_PREFIX_, str_free)(str);
    return;
}

XENUMS_LINKAGE int32
CAT(ENUM_PREFIX_, str_len)(enum ENUM_NAME val, char **out) {
#if ENUM_BITFLAGS == 0
    switch (val) {
        #define XX_1(e)    case e:                                             \
                               *out = #e;                                      \
                               return STRLIT_LEN(#e);
        #define XX_2(e, v) case e:                                             \
                               *out = #e;                                      \
                               return STRLIT_LEN(#e);
        #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

        ENUM_FIELDS

        #undef XX
        #undef XX_1
        #undef XX_2

        case CAT(ENUM_PREFIX_, COUNT):
            *out = QUOTE(ENUM_PREFIX_) "COUNT";
            return STRLIT_LEN(QUOTE(ENUM_PREFIX_) "COUNT");
        default:
            *out = "Invalid enum value";
            return STRLIT_LEN("Invalid enum value");
    }
#else
    char *buffer = NULL;
    int32 buffer_len = 0;
    int32 buffer_cap = 0;
    bool32 is_first = true;

    if (val == 0) {
        *out = xstrndup(STRLIT("NONE"));
        return STRLIT_LEN("NONE");
    }

    #define XX_EXACT(e)                                                        \
        if (val == e) {                                                        \
            *out = xstrndup(#e, STRLIT_LEN(#e));                               \
            return STRLIT_LEN(#e);                                             \
        }
    #define XX_1(e)    XX_EXACT(e)
    #define XX_2(e, v) XX_EXACT(e)
    #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

    ENUM_FIELDS

    #undef XX
    #undef XX_1
    #undef XX_2
    #undef XX_EXACT

    #define XX_BITCHECK(e)                                                     \
        if (val && ((val & e) == e)) {                                         \
            char *name = #e;                                                   \
            int32 len = STRLIT_LEN(#e);                                        \
            int32 new_cap = buffer_len + len + 1;                              \
                                                                               \
            if (!is_first) {                                                   \
                new_cap += 1;                                                  \
            }                                                                  \
                                                                               \
            buffer = realloc2(buffer, buffer_cap, new_cap, SIZEOF(*buffer));   \
            buffer_cap = new_cap;                                              \
            if (!is_first) {                                                   \
                buffer[buffer_len] = '|';                                      \
                buffer_len += 1;                                               \
            }                                                                  \
            memcpy64(buffer + buffer_len, name, len);                          \
            buffer_len += len;                                                 \
                                                                               \
            is_first = false;                                                  \
            val &= (ENUM_UNDERLYING_TYPE)~e;                                   \
        }

    #define XX_1(e)    XX_BITCHECK(e)
    #define XX_2(e, v) XX_BITCHECK(e)
    #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

    ENUM_FIELDS

    #undef XX
    #undef XX_1
    #undef XX_2
    #undef XX_BITCHECK

    if (val) {
        error2("Error: bit flags enum contains invalid bit set.\n");
        TRAP();
    }

    buffer[buffer_len] = '\0';
    *out = buffer;
    return buffer_len;
#endif
}

XENUMS_LINKAGE char *
CAT(ENUM_PREFIX_, str)(enum ENUM_NAME val) {
    char *str;

    CAT(ENUM_PREFIX_, str_len)(val, &str);
    return str;
}

XENUMS_LINKAGE int32
CAT(ENUM_PREFIX_, alias_len)(enum ENUM_NAME val, char **out) {
#if ENUM_BITFLAGS == 0
    switch (val) {
        #define XX_1(e)        case e:                                         \
                                   *out = #e;                                  \
                                   return STRLIT_LEN(#e);
        #define XX_2(e, alias) case e:                                         \
                                   *out = #alias;                              \
                                   return STRLIT_LEN(#alias);
        #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

        ENUM_FIELDS

        #undef XX
        #undef XX_1
        #undef XX_2

        case CAT(ENUM_PREFIX_, COUNT):
            *out = QUOTE(ENUM_PREFIX_) "COUNT";
            return STRLIT_LEN(QUOTE(ENUM_PREFIX_) "COUNT");
        default:
            *out = "Invalid enum value";
            return STRLIT_LEN("Invalid enum value");
    }
#else
    return CAT(ENUM_PREFIX_, str_len)(val, out);
#endif
}

XENUMS_LINKAGE char *
CAT(ENUM_PREFIX_, alias)(enum ENUM_NAME val) {
    char *str;

    CAT(ENUM_PREFIX_, alias_len)(val, &str);
    return str;
}

#define XENUM_TOKEN_EQUALS(token, token_len, name)                             \
    ((token_len) == strlen32(name)                                             \
     && BEGINS_WITH_4(token, token_len, name, token_len))

#define XENUM_TOKEN_EQUALS_ENUM_NAME(token, token_len, name)                   \
    (XENUM_TOKEN_EQUALS(token, token_len, name)                                \
     || (BEGINS_WITH_4(name, strlen32(name), QUOTE(ENUM_PREFIX_),              \
                       strlen32(QUOTE(ENUM_PREFIX_)))                          \
         && XENUM_TOKEN_EQUALS(token, token_len,                               \
                               &(name)[strlen32(QUOTE(ENUM_PREFIX_))])))

XENUMS_LINKAGE enum ENUM_NAME
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
            error2("Error: invalid enum parse character '%c' in %s.\n",
                   *p, string);
            TRAP();
        }

#if ENUM_BITFLAGS
        if (XENUM_TOKEN_EQUALS(token, token_len, QUOTE(ENUM_PREFIX_) "NONE")
            || XENUM_TOKEN_EQUALS(token, token_len, "NONE")) {
            matched = 1;
        }
#endif

#if ENUM_BITFLAGS == 0
        if (XENUM_TOKEN_EQUALS(token, token_len, QUOTE(ENUM_PREFIX_) "COUNT")
            || XENUM_TOKEN_EQUALS(token, token_len, "COUNT")) {
            result = (ENUM_UNDERLYING_TYPE)CAT(ENUM_PREFIX_, COUNT);
            matched = 1;
        }
#endif

#if ENUM_BITFLAGS
        #define XENUM_PARSE_ONE(e)                                             \
            if (!matched                                                       \
                && XENUM_TOKEN_EQUALS_ENUM_NAME(token, token_len, #e)) {       \
                result |= (ENUM_UNDERLYING_TYPE)e;                             \
                matched = 1;                                                   \
            }
        #define XX_1(e)    XENUM_PARSE_ONE(e)
        #define XX_2(e, v) XENUM_PARSE_ONE(e)
#else
        #define XENUM_PARSE_ONE(e)                                             \
            if (!matched                                                       \
                && XENUM_TOKEN_EQUALS_ENUM_NAME(token, token_len, #e)) {       \
                result = (ENUM_UNDERLYING_TYPE)e;                              \
                matched = 1;                                                   \
            }
        #define XENUM_PARSE_ALIAS(e, alias)                                    \
            XENUM_PARSE_ONE(e)                                                 \
            if (!matched                                                       \
                && XENUM_TOKEN_EQUALS(token, token_len, #alias)) {             \
                result = (ENUM_UNDERLYING_TYPE)e;                              \
                matched = 1;                                                   \
            }
        #define XX_1(e)        XENUM_PARSE_ONE(e)
        #define XX_2(e, alias) XENUM_PARSE_ALIAS(e, alias)
#endif
        #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

        ENUM_FIELDS

        #undef XX
        #undef XX_1
        #undef XX_2
#if ENUM_BITFLAGS == 0
        #undef XENUM_PARSE_ALIAS
#endif
        #undef XENUM_PARSE_ONE

        if (!matched) {
            error2("Error: unknown enum token '%.*s' while parsing %s.\n",
                   token_len, token, string);
            TRAP();
        }
    }

    return (enum ENUM_NAME)result;
}

#undef XENUM_TOKEN_EQUALS
#undef XENUM_TOKEN_EQUALS_ENUM_NAME

#if 0 == TESTING_xenums
static inline void
CAT(ENUM_PREFIX_, functions_sink)(void) {
    (void)CAT(ENUM_PREFIX_, functions_sink);
    (void)CAT(ENUM_PREFIX_, str);
    (void)CAT(ENUM_PREFIX_, str_free);
    (void)CAT(ENUM_PREFIX_, alias);
    (void)CAT(ENUM_PREFIX_, alias_free);
    (void)CAT(ENUM_PREFIX_, parse);
    return;
}
#endif
#endif

#undef XENUMS_LINKAGE
#undef XENUMS_DECLARE_ONLY
#undef XENUMS_FUNCTIONS_ONLY

#undef ENUM_NAME
#undef ENUM_PREFIX_
#undef ENUM_FIELDS
#undef ENUM_BITFLAGS
#undef ENUM_UNDERLYING_TYPE
#undef ENUM_UNDERLYING_TYPE_SPEC

#if TESTING_xenums                                                             \
    && !defined(TESTING_xenums_started)                                        \
    && !defined(XENUMS_NO_TESTS)
#define TESTING_xenums_started

#define ENUM_NAME TestNormal
#define ENUM_PREFIX_ TEST_NORMAL_
#define ENUM_BITFLAGS 0
#define ENUM_FIELDS                                                            \
    XX(TEST_NORMAL_APPLE)                                                      \
    XX(TEST_NORMAL_BANANA, banana)                                             \
    XX(TEST_NORMAL_CHERRY, cherry)
#include "xenums.c"

int
main(void) {
    char *s;
    ASSERT_ZERO(TEST_FLAGS_READ_BIT_INDEX);
    ASSERT(TEST_FLAGS_BIT_COUNT == 3);
    ASSERT(TEST_FLAGS_READ == (1 << 0));
    ASSERT(TEST_FLAGS_WRITE == (1 << 1));
    ASSERT(TEST_FLAGS_EXEC == (1 << 2));

    s = TEST_FLAGS_str(TEST_FLAGS_READ);
    ASSERT_EQUAL(s, "TEST_FLAGS_READ");
    TEST_FLAGS_str_free(s);

    s = TEST_FLAGS_alias(TEST_FLAGS_READ);
    ASSERT_EQUAL(s, "TEST_FLAGS_READ");
    TEST_FLAGS_alias_free(s);

    s = TEST_FLAGS_str(TEST_FLAGS_READ | TEST_FLAGS_EXEC);
    ASSERT_EQUAL(s, "TEST_FLAGS_READ|TEST_FLAGS_EXEC");
    TEST_FLAGS_str_free(s);

    s = TEST_FLAGS_alias(TEST_FLAGS_READ | TEST_FLAGS_EXEC);
    ASSERT_EQUAL(s, "TEST_FLAGS_READ|TEST_FLAGS_EXEC");
    TEST_FLAGS_alias_free(s);

    s = TEST_FLAGS_str(TEST_FLAGS_READ_WRITE);
    ASSERT_EQUAL(s, "TEST_FLAGS_READ_WRITE");
    TEST_FLAGS_str_free(s);

    s = TEST_FLAGS_str(TEST_FLAGS_READ | TEST_FLAGS_WRITE | TEST_FLAGS_EXEC);
    ASSERT_EQUAL(s, "TEST_FLAGS_READ|TEST_FLAGS_WRITE|TEST_FLAGS_EXEC");
    TEST_FLAGS_str_free(s);

    s = TEST_FLAGS_str(0);
    ASSERT_EQUAL(s, "NONE");
    TEST_FLAGS_str_free(s);

    ASSERT(TEST_FLAGS_parse("TEST_FLAGS_READ") == TEST_FLAGS_READ);
    ASSERT(TEST_FLAGS_parse("TEST_FLAGS_READ | TEST_FLAGS_EXEC")
           == (TEST_FLAGS_READ | TEST_FLAGS_EXEC));
    ASSERT(TEST_FLAGS_parse("READ|WRITE")
            == (TEST_FLAGS_READ | TEST_FLAGS_WRITE));
    ASSERT(TEST_FLAGS_parse("READ_WRITE") == TEST_FLAGS_READ_WRITE);
    ASSERT(TEST_FLAGS_parse("NONE") == TEST_FLAGS_NONE);

    ASSERT_ZERO(TEST_NORMAL_APPLE);
    ASSERT(TEST_NORMAL_BANANA == 1);
    ASSERT(TEST_NORMAL_CHERRY == 2);
    ASSERT(TEST_NORMAL_COUNT == 3);

    s = TEST_NORMAL_str(TEST_NORMAL_APPLE);
    ASSERT_EQUAL(s, "TEST_NORMAL_APPLE");
    TEST_NORMAL_str_free(s);

    s = TEST_NORMAL_alias(TEST_NORMAL_APPLE);
    ASSERT_EQUAL(s, "TEST_NORMAL_APPLE");
    TEST_NORMAL_alias_free(s);

    s = TEST_NORMAL_str(TEST_NORMAL_BANANA);
    ASSERT_EQUAL(s, "TEST_NORMAL_BANANA");
    TEST_NORMAL_str_free(s);

    s = TEST_NORMAL_alias(TEST_NORMAL_BANANA);
    ASSERT_EQUAL(s, "banana");
    TEST_NORMAL_alias_free(s);

    s = TEST_NORMAL_str(TEST_NORMAL_CHERRY);
    ASSERT_EQUAL(s, "TEST_NORMAL_CHERRY");
    TEST_NORMAL_str_free(s);

    s = TEST_NORMAL_alias(TEST_NORMAL_CHERRY);
    ASSERT_EQUAL(s, "cherry");
    TEST_NORMAL_alias_free(s);

    ASSERT(TEST_NORMAL_parse("TEST_NORMAL_APPLE") == TEST_NORMAL_APPLE);
    ASSERT(TEST_NORMAL_parse("BANANA") == TEST_NORMAL_BANANA);
    ASSERT(TEST_NORMAL_parse("banana") == TEST_NORMAL_BANANA);
    ASSERT(TEST_NORMAL_parse("TEST_NORMAL_CHERRY") == TEST_NORMAL_CHERRY);
    ASSERT(TEST_NORMAL_parse("cherry") == TEST_NORMAL_CHERRY);
    ASSERT(TEST_NORMAL_parse("TEST_NORMAL_COUNT") == TEST_NORMAL_COUNT);
    ASSERT(TEST_NORMAL_parse("COUNT") == TEST_NORMAL_COUNT);

    s = TEST_NORMAL_str(TEST_NORMAL_COUNT);
    ASSERT_EQUAL(s, "TEST_NORMAL_COUNT");
    TEST_NORMAL_str_free(s);

    s = TEST_NORMAL_alias(TEST_NORMAL_COUNT);
    ASSERT_EQUAL(s, "TEST_NORMAL_COUNT");
    TEST_NORMAL_alias_free(s);

    s = TEST_NORMAL_str(999);
    ASSERT_EQUAL(s, "Invalid enum value");
    TEST_NORMAL_str_free(s);

    printf("xenums.c: All tests passed successfully.\n");
    return EXIT_SUCCESS;
}

#define CBASE_IMPLEMENT
#include "cbase.h"

#endif /* TESTING_xenums && !defined(TESTING_xenums_started)
          && !defined(XENUMS_NO_TESTS) */

#if CC_CLANG
#pragma clang diagnostic pop
#endif
