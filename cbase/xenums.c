// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#define CBASE_INCLUDE_ONLY 1

#include "base_macros.h"

#if CC_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wfixed-enum-extension"
#endif

#if !defined(TESTING_xenums)
#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_xenums 1
#else
#define TESTING_xenums 0
#endif
#endif

#if !defined(CBASE_H)
  #if defined(ENUM_NAME) || defined(ENUM_PREFIX_)                              \
      || defined(ENUM_FIELDS) || defined(ENUM_BITFLAGS)                        \
      || defined(ENUM_UNDERLYING_TYPE) || defined(ENUM_CHAR_REPR)
    #error "include cbase.h before configuring xenums.c"
  #endif
#include "cbase.h"
#endif

#if !defined(ENUM_UNDERLYING_TYPE)
  #define ENUM_UNDERLYING_TYPE uint32
#endif

#if !defined(ENUM_CHAR_REPR)
  #define ENUM_CHAR_REPR 0
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
    #define XX_1(e) CAT(e, _BIT_INDEX),
  #if ENUM_CHAR_REPR
    #define XX_2(e, v) CAT(e, _BIT_INDEX),
  #else
    #define XX_2(e, v)
  #endif
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

#if ENUM_BITFLAGS == 0
enum CAT(ENUM_NAME, _XenumIndices) {
    #define XX_1(e)        CAT(e, _XENUM_INDEX),
    #define XX_2(e, alias) CAT(e, _XENUM_INDEX),
    #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

    ENUM_FIELDS

    #undef XX
    #undef XX_1
    #undef XX_2
};
#endif

// For bit flag enums, the optional second X macro parameter is a value unless
// ENUM_CHAR_REPR is 1. Only use compositions of previous enum values there, not
// numeric values.
// For non-bit flag enums, the optional second X macro parameter is a parse
// alias token. Custom numeric values are not supported for non-bit flag enums.
// If ENUM_CHAR_REPR is 1, enum aliases are char literals instead.
//
// Passing multiple ENUM names for the same value will break compilation.
enum ENUM_NAME ENUM_UNDERLYING_TYPE_SPEC {
#if ENUM_BITFLAGS
    CAT(ENUM_PREFIX_, NONE) = 0,
#endif

#if ENUM_BITFLAGS == 0
    #define XX_1(e)        e = CAT(e, _XENUM_INDEX) + 1,
    #define XX_2(e, alias) e = CAT(e, _XENUM_INDEX) + 1,
#elif ENUM_CHAR_REPR
    #define XX_1(e)        e = (ENUM_UNDERLYING_TYPE)1 << CAT(e, _BIT_INDEX),
    #define XX_2(e, alias) e = (ENUM_UNDERLYING_TYPE)1 << CAT(e, _BIT_INDEX),
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
typedef enum ENUM_NAME ENUM_PREFIX_;

#endif

#if ENUM_BITFLAGS
XENUMS_LINKAGE void CAT(ENUM_PREFIX_, alias_free)(char *);
XENUMS_LINKAGE void CAT(ENUM_PREFIX_, str_free)(char *);
#endif

XENUMS_LINKAGE int32 CAT(ENUM_PREFIX_, str_len)(enum ENUM_NAME, char **);
XENUMS_LINKAGE char *CAT(ENUM_PREFIX_, str)(enum ENUM_NAME);
XENUMS_LINKAGE int32 CAT(ENUM_PREFIX_, alias_len)(enum ENUM_NAME, char **);
XENUMS_LINKAGE char *CAT(ENUM_PREFIX_, alias)(enum ENUM_NAME);
XENUMS_LINKAGE enum ENUM_NAME CAT(ENUM_PREFIX_, parse)(char *, int32);
XENUMS_LINKAGE enum ENUM_NAME CAT(ENUM_PREFIX_, parse_strict)(char *, int32);

#if ENUM_CHAR_REPR && ENUM_BITFLAGS
XENUMS_LINKAGE enum ENUM_NAME CAT(ENUM_PREFIX_, parse_chars)(char **);
#endif

#if XENUMS_DECLARE_ONLY == 0
#if ENUM_BITFLAGS
XENUMS_LINKAGE void
CAT(ENUM_PREFIX_, str_free)(char *str) {
    (void)str;
#if ENUM_BITFLAGS
    free2(str, opt_strlen32(str) + 1);
#endif
    return;
}

XENUMS_LINKAGE void
CAT(ENUM_PREFIX_, alias_free)(char *str) {
    CAT(ENUM_PREFIX_, str_free)(str);
    return;
}
#endif

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
        default: {
            char *invalid = "Invalid enum value";

            *out = invalid;
            return strlen32(invalid);
        }
    }
#else
    char *buffer = NULL;
    int32 buffer_len = 0;
    int32 buffer_cap = 0;
    bool32 is_first = true;

    if (val == 0) {
        char *none = "NONE";
        int32 none_len = strlen32(none);

        *out = xstrndup(none, none_len);
        return none_len;
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
#if ENUM_CHAR_REPR
    #define XX_1(e)
    #define XX_2(e, alias) static char CAT(e, _alias)[] = {alias, '\0'};
    #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

    ENUM_FIELDS

    #undef XX
    #undef XX_1
    #undef XX_2
#endif

    switch (val) {
#if ENUM_CHAR_REPR
        #define XX_1(e)        case e:                                         \
                                   *out = #e;                                  \
                                   return STRLIT_LEN(#e);
        #define XX_2(e, alias) case e:                                         \
                                   *out = CAT(e, _alias);                      \
                                   return 1;
#else
        #define XX_1(e)        case e:                                         \
                                   *out = #e;                                  \
                                   return STRLIT_LEN(#e);
        #define XX_2(e, alias) case e:                                         \
                                   *out = #alias;                              \
                                   return STRLIT_LEN(#alias);
#endif
        #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

        ENUM_FIELDS

        #undef XX
        #undef XX_1
        #undef XX_2

        case CAT(ENUM_PREFIX_, COUNT):
            *out = QUOTE(ENUM_PREFIX_) "COUNT";
            return STRLIT_LEN(QUOTE(ENUM_PREFIX_) "COUNT");
        default: {
            char *invalid = "Invalid enum value";

            *out = invalid;
            return strlen32(invalid);
        }
    }
#elif ENUM_CHAR_REPR
    char *buffer = NULL;
    int32 buffer_len = 0;
    int32 buffer_cap = 0;

    if (val == 0) {
        *out = xstrndup(STRLIT(""));
        return 0;
    }

    #define XX_BITCHECK(e, alias)                                             \
        if (val && ((val & e) == e)) {                                         \
            buffer = realloc2(buffer, buffer_cap, buffer_len + 2,              \
                              SIZEOF(*buffer));                               \
            buffer_cap = buffer_len + 2;                                       \
            buffer[buffer_len] = alias;                                        \
            buffer_len += 1;                                                   \
            val &= (ENUM_UNDERLYING_TYPE)~e;                                   \
        }
    #define XX_1(e)
    #define XX_2(e, alias) XX_BITCHECK(e, alias)
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

#if XENUMS_DECLARE_ONLY == 0
#if ENUM_CHAR_REPR == 0
static inline bool32
CAT(ENUM_PREFIX_, parse_name_equals)(char *string, int32 string_len,
                                     char *name, int32 name_len) {
    if (string_len != name_len) {
        return 0;
    }

    for (int32 i = 0; i < string_len; i += 1) {
        char left = string[i];
        char right = name[i];

        if ((left == ' ') || (left == '-')) {
            left = '_';
        }
        if ((right == ' ') || (right == '-')) {
            right = '_';
        }
        if ((left >= 'A') && (left <= 'Z')) {
            left = (char)(left - 'A' + 'a');
        }
        if ((right >= 'A') && (right <= 'Z')) {
            right = (char)(right - 'A' + 'a');
        }
        if (left != right) {
            return 0;
        }
    }

    return 1;
}
#endif
#endif

#if ENUM_CHAR_REPR == 0
#define XENUM_TOKEN_EQUALS_N(token, token_len, name, name_len)             \
    CAT(ENUM_PREFIX_, parse_name_equals)(token, token_len, name, name_len)

#define XENUM_TOKEN_EQUALS(token, token_len, name)                         \
    XENUM_TOKEN_EQUALS_N(token, token_len, name, STRLIT_LEN(name))

#define XENUM_TOKEN_EQUALS_ENUM_NAME(token, token_len, name)               \
    (XENUM_TOKEN_EQUALS(token, token_len, name)                            \
     || (BEGINS_WITH_4(name, STRLIT_LEN(name), QUOTE(ENUM_PREFIX_),        \
                       STRLIT_LEN(QUOTE(ENUM_PREFIX_)))                    \
         && XENUM_TOKEN_EQUALS_N(token, token_len,                         \
                                 &(name)[STRLIT_LEN(QUOTE(ENUM_PREFIX_))], \
                                 STRLIT_LEN(name)                          \
                                 - STRLIT_LEN(QUOTE(ENUM_PREFIX_)))))
#endif

#define XENUM_INVALID_PARSE_RESULT ((enum ENUM_NAME)0)

#if ENUM_CHAR_REPR
#if ENUM_BITFLAGS
XENUMS_LINKAGE enum ENUM_NAME
CAT(ENUM_PREFIX_, parse_chars)(char **cursor) {
    ENUM_UNDERLYING_TYPE result = 0;

    if ((cursor == NULL) || (*cursor == NULL)) {
        return XENUM_INVALID_PARSE_RESULT;
    }

    while (true) {
        switch (**cursor) {
            #define XX_1(e)
            #define XX_2(e, alias) case alias:                            \
                                       result |= (ENUM_UNDERLYING_TYPE)e; \
                                       break;
            #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

            ENUM_FIELDS

            #undef XX
            #undef XX_1
            #undef XX_2
        default:
            return (enum ENUM_NAME)result;
        }

        *cursor += 1;
    }
}
#endif

XENUMS_LINKAGE enum ENUM_NAME
CAT(ENUM_PREFIX_, parse)(char *string, int32 string_len) {
#if ENUM_BITFLAGS
    ENUM_UNDERLYING_TYPE result = 0;
    char *end;

    if ((string == NULL) || (string_len <= 0)) {
        return XENUM_INVALID_PARSE_RESULT;
    }

    end = string + string_len;
    for (char *p = string; p < end; p += 1) {
        switch (*p) {
            #define XX_1(e)
            #define XX_2(e, alias) case alias:                               \
                                       result |= (ENUM_UNDERLYING_TYPE)e;      \
                                       break;
            #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

            ENUM_FIELDS

            #undef XX
            #undef XX_1
            #undef XX_2
        default:
            return XENUM_INVALID_PARSE_RESULT;
        }
    }

    return (enum ENUM_NAME)result;
#else
    if ((string == NULL) || (string_len != 1)) {
        return XENUM_INVALID_PARSE_RESULT;
    }

    switch (string[0]) {
        #define XX_1(e)
        #define XX_2(e, alias) case alias: return e;
        #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

        ENUM_FIELDS

        #undef XX
        #undef XX_1
        #undef XX_2
    default:
        return XENUM_INVALID_PARSE_RESULT;
    }
#endif
}
#else
XENUMS_LINKAGE enum ENUM_NAME
CAT(ENUM_PREFIX_, parse)(char *string, int32 string_len) {
    ENUM_UNDERLYING_TYPE result = 0;
    char *p = string;
    char *end;
    bool matched_any = false;

    if (p == NULL || string_len <= 0) {
        return XENUM_INVALID_PARSE_RESULT;
    }

    end = string + string_len;
    while (p < end) {
        char *token;
        int32 token_len;
        int32 matched = 0;

        while (p < end
               && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'
                   || *p == '|' || *p == '(' || *p == ')')) {
            p += 1;
        }
        if (p >= end) {
            break;
        }

        token = p;
        while (p < end && *p != '|' && *p != '(' && *p != ')') {
            p += 1;
        }
        while ((p > token)
               && (p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\n'
                   || p[-1] == '\r')) {
            p -= 1;
        }
        token_len = (int32)(p - token);
        if (token_len <= 0) {
            return XENUM_INVALID_PARSE_RESULT;
        }

#if ENUM_BITFLAGS
        if (XENUM_TOKEN_EQUALS(token, token_len, QUOTE(ENUM_PREFIX_) "NONE")
            || XENUM_TOKEN_EQUALS(token, token_len, "NONE")) {
            matched = 1;
        }
#endif

#if ENUM_BITFLAGS
        #define XENUM_PARSE_ONE(e)                                        \
            if (!matched                                                  \
                && XENUM_TOKEN_EQUALS_ENUM_NAME(token, token_len, #e)) {  \
                result |= (ENUM_UNDERLYING_TYPE)e;                        \
                matched = 1;                                              \
            }
        #define XX_1(e)    XENUM_PARSE_ONE(e)
        #define XX_2(e, v) XENUM_PARSE_ONE(e)
#else
        #define XENUM_PARSE_ONE(e)                                        \
            if (!matched                                                  \
                && XENUM_TOKEN_EQUALS_ENUM_NAME(token, token_len, #e)) {  \
                result = (ENUM_UNDERLYING_TYPE)e;                         \
                matched = 1;                                              \
            }
        #define XENUM_PARSE_ALIAS(e, alias)                               \
            XENUM_PARSE_ONE(e)                                            \
            if (!matched                                                  \
                && XENUM_TOKEN_EQUALS(token, token_len, #alias)) {        \
                result = (ENUM_UNDERLYING_TYPE)e;                         \
                matched = 1;                                              \
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
            return XENUM_INVALID_PARSE_RESULT;
        }
        matched_any = true;
    }

    if (!matched_any) {
        return XENUM_INVALID_PARSE_RESULT;
    }

    return (enum ENUM_NAME)result;
}
#endif

XENUMS_LINKAGE enum ENUM_NAME
CAT(ENUM_PREFIX_, parse_strict)(char *string, int32 string_len) {
#if ENUM_BITFLAGS == 0
    if ((string == NULL) || (string_len < 0)) {
        return XENUM_INVALID_PARSE_RESULT;
    }

#if ENUM_CHAR_REPR
    if (string_len != 1) {
        return XENUM_INVALID_PARSE_RESULT;
    }

    switch (string[0]) {
        #define XX_1(e)
        #define XX_2(e, alias) case alias: return e;
        #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

        ENUM_FIELDS

        #undef XX
        #undef XX_1
        #undef XX_2
    default:
        break;
    }
#else
    #define XX_1(e)                                                    \
        if (strequal2(string, string_len, #e, STRLIT_LEN(#e))) {        \
            return e;                                                  \
        }
    #define XX_2(e, alias)                                             \
        if (strequal2(string, string_len, #alias, STRLIT_LEN(#alias))) {\
            return e;                                                  \
        }
    #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)

    ENUM_FIELDS

    #undef XX
    #undef XX_1
    #undef XX_2
#endif

    return XENUM_INVALID_PARSE_RESULT;
#else
    enum ENUM_NAME result;
    char *alias;
    int32 alias_len;
    bool32 matched;

    if ((string == NULL) || (string_len < 0)) {
        return XENUM_INVALID_PARSE_RESULT;
    }

    result = CAT(ENUM_PREFIX_, parse)(string, string_len);
    alias_len = CAT(ENUM_PREFIX_, alias_len)(result, &alias);
    matched = STREQUAL(string, string_len, alias, alias_len);
    CAT(ENUM_PREFIX_, alias_free)(alias);
    if (!matched) {
        return XENUM_INVALID_PARSE_RESULT;
    }
    return result;
#endif
}

#if ENUM_CHAR_REPR == 0
#undef XENUM_TOKEN_EQUALS
#undef XENUM_TOKEN_EQUALS_N
#undef XENUM_TOKEN_EQUALS_ENUM_NAME
#endif
#undef XENUM_INVALID_PARSE_RESULT

#if 0 == TESTING_xenums
static inline void
CAT(ENUM_PREFIX_, functions_sink)(void) {
    (void)CAT(ENUM_PREFIX_, functions_sink);
    (void)CAT(ENUM_PREFIX_, str);
    (void)CAT(ENUM_PREFIX_, alias);
#if ENUM_BITFLAGS
    (void)CAT(ENUM_PREFIX_, str_free);
    (void)CAT(ENUM_PREFIX_, alias_free);
#endif
    (void)CAT(ENUM_PREFIX_, parse);
    (void)CAT(ENUM_PREFIX_, parse_strict);
#if ENUM_CHAR_REPR && ENUM_BITFLAGS
    (void)CAT(ENUM_PREFIX_, parse_chars);
#endif
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
#undef ENUM_CHAR_REPR
#undef ENUM_UNDERLYING_TYPE
#undef ENUM_UNDERLYING_TYPE_SPEC

#if TESTING_xenums                                \
    && !defined(TESTING_xenums_started)           \
    && !defined(XENUMS_NO_TESTS)
#define TESTING_xenums_started

#define ENUM_NAME TestNormal
#define ENUM_PREFIX_ TEST_NORMAL_
#define ENUM_BITFLAGS 0
#define ENUM_FIELDS                               \
    XX(TEST_NORMAL_APPLE)                         \
    XX(TEST_NORMAL_BANANA, banana)                \
    XX(TEST_NORMAL_CHERRY, cherry)                \
    XX(TEST_NORMAL_PEANUT_BUTTER, peanut butter)
#include "xenums.c"

#define ENUM_NAME TestCharRepr
#define ENUM_PREFIX_ TEST_CHAR_REPR_
#define ENUM_BITFLAGS 0
#define ENUM_CHAR_REPR 1
#define ENUM_FIELDS                               \
    XX(TEST_CHAR_REPR_IDENTIFIER)                 \
    XX(TEST_CHAR_REPR_PLUS, '+')                  \
    XX(TEST_CHAR_REPR_MINUS, '-')                 \
    XX(TEST_CHAR_REPR_PIPE, '|')                  \
    XX(TEST_CHAR_REPR_CLOSE_PAREN, ')')
#include "xenums.c"

#define ENUM_NAME TestCharFlags
#define ENUM_PREFIX_ TEST_CHAR_FLAGS_
#define ENUM_BITFLAGS 1
#define ENUM_CHAR_REPR 1
#define ENUM_FIELDS                               \
    XX(TEST_CHAR_FLAGS_READ, 'r')                 \
    XX(TEST_CHAR_FLAGS_WRITE, 'w')                \
    XX(TEST_CHAR_FLAGS_EXEC, 'x')
#include "xenums.c"

int
main(void) {
    char *s;
    TEST_FLAGS_ flags = TEST_FLAGS_READ;
    TEST_NORMAL_ normal = TEST_NORMAL_APPLE;
    char *cursor;

    ASSERT_ZERO(TEST_FLAGS_READ_BIT_INDEX);
    ASSERT(3 == TEST_FLAGS_BIT_COUNT);
    ASSERT(TEST_FLAGS_READ == (1 << 0));
    ASSERT(TEST_FLAGS_WRITE == (1 << 1));
    ASSERT(TEST_FLAGS_EXEC == (1 << 2));
    ASSERT(flags == TEST_FLAGS_READ);
    ASSERT(normal == TEST_NORMAL_APPLE);

    s = TEST_FLAGS_str(TEST_FLAGS_READ);
    ASSERT_EQ(s, "TEST_FLAGS_READ");
    TEST_FLAGS_str_free(s);

    s = TEST_FLAGS_alias(TEST_FLAGS_READ);
    ASSERT_EQ(s, "TEST_FLAGS_READ");
    TEST_FLAGS_alias_free(s);

    s = TEST_FLAGS_str(TEST_FLAGS_READ | TEST_FLAGS_EXEC);
    ASSERT_EQ(s, "TEST_FLAGS_READ|TEST_FLAGS_EXEC");
    TEST_FLAGS_str_free(s);

    s = TEST_FLAGS_alias(TEST_FLAGS_READ | TEST_FLAGS_EXEC);
    ASSERT_EQ(s, "TEST_FLAGS_READ|TEST_FLAGS_EXEC");
    TEST_FLAGS_alias_free(s);

    s = TEST_FLAGS_str(TEST_FLAGS_READ_WRITE);
    ASSERT_EQ(s, "TEST_FLAGS_READ_WRITE");
    TEST_FLAGS_str_free(s);

    s = TEST_FLAGS_str(TEST_FLAGS_READ | TEST_FLAGS_WRITE | TEST_FLAGS_EXEC);
    ASSERT_EQ(s, "TEST_FLAGS_READ|TEST_FLAGS_WRITE|TEST_FLAGS_EXEC");
    TEST_FLAGS_str_free(s);

    s = TEST_FLAGS_str(0);
    ASSERT_EQ(s, "NONE");
    TEST_FLAGS_str_free(s);

    ASSERT(TEST_FLAGS_parse(STRLIT("TEST_FLAGS_READ")) == TEST_FLAGS_READ);
    ASSERT(TEST_FLAGS_parse(STRLIT("TEST_FLAGS_READ | TEST_FLAGS_EXEC"))
           == (TEST_FLAGS_READ | TEST_FLAGS_EXEC));
    ASSERT(TEST_FLAGS_parse(STRLIT("READ|WRITE"))
            == (TEST_FLAGS_READ | TEST_FLAGS_WRITE));
    ASSERT(TEST_FLAGS_parse(STRLIT("READ_WRITE")) == TEST_FLAGS_READ_WRITE);
    ASSERT(TEST_FLAGS_parse(STRLIT("NONE")) == TEST_FLAGS_NONE);
    ASSERT(TEST_FLAGS_parse(STRLIT("")) == TEST_FLAGS_NONE);
    ASSERT(TEST_FLAGS_parse(STRLIT("   ")) == TEST_FLAGS_NONE);
    ASSERT(TEST_FLAGS_parse(STRLIT("unknown")) == TEST_FLAGS_NONE);
    ASSERT(TEST_FLAGS_parse(STRLIT("READ|unknown")) == TEST_FLAGS_NONE);
    ASSERT(TEST_FLAGS_parse(STRLIT("@")) == TEST_FLAGS_NONE);
    ASSERT(TEST_FLAGS_parse_strict(STRLIT("TEST_FLAGS_READ"))
           == TEST_FLAGS_READ);
    ASSERT(TEST_FLAGS_parse_strict(STRLIT("TEST_FLAGS_READ|TEST_FLAGS_EXEC"))
           == (TEST_FLAGS_READ | TEST_FLAGS_EXEC));
    ASSERT(TEST_FLAGS_parse_strict(STRLIT("READ")) == TEST_FLAGS_NONE);
    ASSERT(TEST_FLAGS_parse_strict(STRLIT("TEST_FLAGS_READ | TEST_FLAGS_EXEC"))
           == TEST_FLAGS_NONE);

    {
        char counted[] = {'R', 'E', 'A', 'D'};

        ASSERT(TEST_FLAGS_parse(counted, (int32)SIZEOF(counted))
               == TEST_FLAGS_READ);
    }

    ASSERT_EQ(TEST_NORMAL_APPLE, 1);
    ASSERT_EQ(TEST_NORMAL_BANANA, 2);
    ASSERT_EQ(TEST_NORMAL_CHERRY, 3);
    ASSERT_EQ(TEST_NORMAL_COUNT, 5);

    s = TEST_NORMAL_str(TEST_NORMAL_APPLE);
    ASSERT_EQ(s, "TEST_NORMAL_APPLE");

    s = TEST_NORMAL_alias(TEST_NORMAL_APPLE);
    ASSERT_EQ(s, "TEST_NORMAL_APPLE");

    s = TEST_NORMAL_str(TEST_NORMAL_BANANA);
    ASSERT_EQ(s, "TEST_NORMAL_BANANA");

    s = TEST_NORMAL_alias(TEST_NORMAL_BANANA);
    ASSERT_EQ(s, "banana");

    s = TEST_NORMAL_str(TEST_NORMAL_CHERRY);
    ASSERT_EQ(s, "TEST_NORMAL_CHERRY");

    s = TEST_NORMAL_alias(TEST_NORMAL_CHERRY);
    ASSERT_EQ(s, "cherry");

    ASSERT(TEST_NORMAL_parse(STRLIT("TEST_NORMAL_APPLE")) == TEST_NORMAL_APPLE);
    ASSERT(TEST_NORMAL_parse(STRLIT("BANANA")) == TEST_NORMAL_BANANA);
    ASSERT(TEST_NORMAL_parse(STRLIT("banana")) == TEST_NORMAL_BANANA);
    ASSERT(TEST_NORMAL_parse(STRLIT("TEST_NORMAL_CHERRY"))
           == TEST_NORMAL_CHERRY);
    ASSERT(TEST_NORMAL_parse(STRLIT("cherry")) == TEST_NORMAL_CHERRY);
    ASSERT(TEST_NORMAL_parse(STRLIT("peanut_butter"))
           == TEST_NORMAL_PEANUT_BUTTER);
    ASSERT(TEST_NORMAL_parse(STRLIT("PeAnUt BuTtEr"))
           == TEST_NORMAL_PEANUT_BUTTER);
    ASSERT(TEST_NORMAL_parse(STRLIT("PEANUT-BUTTER"))
           == TEST_NORMAL_PEANUT_BUTTER);
    ASSERT_ZERO(TEST_NORMAL_parse(STRLIT("TEST_NORMAL_COUNT")));
    ASSERT_ZERO(TEST_NORMAL_parse(STRLIT("COUNT")));
    ASSERT_ZERO(TEST_NORMAL_parse(STRLIT("")));
    ASSERT_ZERO(TEST_NORMAL_parse(STRLIT("   ")));
    ASSERT_ZERO(TEST_NORMAL_parse(STRLIT("unknown")));
    ASSERT_ZERO(TEST_NORMAL_parse(STRLIT("banana unknown")));
    ASSERT_ZERO(TEST_NORMAL_parse(STRLIT("@")));
    ASSERT(TEST_NORMAL_parse_strict(STRLIT("banana")) == TEST_NORMAL_BANANA);
    ASSERT_ZERO(TEST_NORMAL_parse_strict(STRLIT("BANANA")));
    ASSERT_ZERO(TEST_NORMAL_parse_strict(STRLIT("TEST_NORMAL_BANANA")));
    ASSERT_ZERO(TEST_NORMAL_parse_strict(STRLIT("PeAnUt BuTtEr")));
    ASSERT(TEST_NORMAL_parse_strict(STRLIT("peanut butter"))
           == TEST_NORMAL_PEANUT_BUTTER);

    {
        char counted[] = {'c', 'h', 'e', 'r', 'r', 'y'};
        char counted_prefix[] = "banana suffix";
        char *prefix = "banana";

        ASSERT(TEST_NORMAL_parse(counted, (int32)SIZEOF(counted))
               == TEST_NORMAL_CHERRY);
        ASSERT(TEST_NORMAL_parse(counted_prefix, strlen32(prefix))
               == TEST_NORMAL_BANANA);
    }

    ASSERT_ZERO(TEST_NORMAL_parse_strict(STRLIT("TEST_NORMAL_COUNT")));

    s = TEST_NORMAL_str(TEST_NORMAL_COUNT);
    ASSERT_EQ(s, "TEST_NORMAL_COUNT");

    s = TEST_NORMAL_alias(TEST_NORMAL_COUNT);
    ASSERT_EQ(s, "TEST_NORMAL_COUNT");

    s = TEST_NORMAL_str(0);
    ASSERT_EQ(s, "Invalid enum value");

    s = TEST_NORMAL_alias(0);
    ASSERT_EQ(s, "Invalid enum value");

    s = TEST_NORMAL_str(999);
    ASSERT_EQ(s, "Invalid enum value");

    ASSERT_EQ(TEST_CHAR_REPR_IDENTIFIER, 1);
    ASSERT_EQ(TEST_CHAR_REPR_PLUS, 2);
    ASSERT_EQ(TEST_CHAR_REPR_COUNT, 6);

    s = TEST_CHAR_REPR_alias(TEST_CHAR_REPR_PLUS);
    ASSERT_EQ(s, "+");

    s = TEST_CHAR_REPR_alias(TEST_CHAR_REPR_IDENTIFIER);
    ASSERT_EQ(s, "TEST_CHAR_REPR_IDENTIFIER");

    ASSERT(TEST_CHAR_REPR_parse(STRLIT("+")) == TEST_CHAR_REPR_PLUS);
    ASSERT(TEST_CHAR_REPR_parse(STRLIT("-")) == TEST_CHAR_REPR_MINUS);
    ASSERT(TEST_CHAR_REPR_parse(STRLIT("|")) == TEST_CHAR_REPR_PIPE);
    ASSERT(TEST_CHAR_REPR_parse(STRLIT(")"))
           == TEST_CHAR_REPR_CLOSE_PAREN);
    ASSERT_ZERO(TEST_CHAR_REPR_parse(STRLIT("TEST_CHAR_REPR_PLUS")));
    ASSERT_ZERO(TEST_CHAR_REPR_parse(STRLIT("")));
    ASSERT_ZERO(TEST_CHAR_REPR_parse(STRLIT(" + ")));
    ASSERT(TEST_CHAR_REPR_parse_strict(STRLIT("+")) == TEST_CHAR_REPR_PLUS);
    ASSERT_ZERO(
        TEST_CHAR_REPR_parse_strict(STRLIT("TEST_CHAR_REPR_PLUS")));

    ASSERT_ZERO(TEST_CHAR_FLAGS_READ_BIT_INDEX);
    ASSERT_EQ(TEST_CHAR_FLAGS_BIT_COUNT, 3);
    ASSERT_EQ(TEST_CHAR_FLAGS_READ, 1 << 0);
    ASSERT_EQ(TEST_CHAR_FLAGS_WRITE, 1 << 1);
    ASSERT_EQ(TEST_CHAR_FLAGS_EXEC, 1 << 2);

    s = TEST_CHAR_FLAGS_alias(TEST_CHAR_FLAGS_READ);
    ASSERT_EQ(s, "r");
    TEST_CHAR_FLAGS_alias_free(s);

    s = TEST_CHAR_FLAGS_alias(TEST_CHAR_FLAGS_READ | TEST_CHAR_FLAGS_EXEC);
    ASSERT_EQ(s, "rx");
    TEST_CHAR_FLAGS_alias_free(s);

    s = TEST_CHAR_FLAGS_alias(TEST_CHAR_FLAGS_NONE);
    ASSERT_EQ(s, "");
    TEST_CHAR_FLAGS_alias_free(s);

    ASSERT(TEST_CHAR_FLAGS_parse(STRLIT("r")) == TEST_CHAR_FLAGS_READ);
    ASSERT(TEST_CHAR_FLAGS_parse(STRLIT("rw"))
           == (TEST_CHAR_FLAGS_READ | TEST_CHAR_FLAGS_WRITE));
    ASSERT(TEST_CHAR_FLAGS_parse(STRLIT(""))
           == TEST_CHAR_FLAGS_NONE);
    ASSERT(TEST_CHAR_FLAGS_parse(STRLIT("TEST_CHAR_FLAGS_READ"))
           == TEST_CHAR_FLAGS_NONE);
    ASSERT(TEST_CHAR_FLAGS_parse(STRLIT("r|x"))
           == TEST_CHAR_FLAGS_NONE);
    ASSERT(TEST_CHAR_FLAGS_parse(STRLIT("z"))
           == TEST_CHAR_FLAGS_NONE);
    ASSERT(TEST_CHAR_FLAGS_parse_strict(STRLIT("rx"))
           == (TEST_CHAR_FLAGS_READ | TEST_CHAR_FLAGS_EXEC));
    ASSERT(TEST_CHAR_FLAGS_parse_strict(STRLIT("r|x"))
           == TEST_CHAR_FLAGS_NONE);

    cursor = "rw-width";
    ASSERT(TEST_CHAR_FLAGS_parse_chars(&cursor)
           == (TEST_CHAR_FLAGS_READ | TEST_CHAR_FLAGS_WRITE));
    ASSERT_EQ(cursor, "-width");

    cursor = "z";
    ASSERT(TEST_CHAR_FLAGS_parse_chars(&cursor) == TEST_CHAR_FLAGS_NONE);
    ASSERT_EQ(cursor, "z");

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

#undef CBASE_INCLUDE_ONLY
