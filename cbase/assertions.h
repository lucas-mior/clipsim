// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(ASSERTIONS_H)
#define ASSERTIONS_H

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_assert 1
#elif !defined(TESTING_assert)
#define TESTING_assert 0
#endif

#include "libc.h"
#include "primitives.h"
#include "base_macros.h"
#include "platform_detection.h"

#define ASSERT_FP_KIND_NONE    0
#define ASSERT_FP_KIND_FLOAT   1
#define ASSERT_FP_KIND_DOUBLE  2

#define ASSERT_FP_KIND_EXPR(VAR) \
_Generic((VAR), \
    float:  ASSERT_FP_KIND_FLOAT,  \
    double: ASSERT_FP_KIND_DOUBLE, \
    default: ASSERT_FP_KIND_NONE   \
)

#if !defined(ASSERT_FLOAT_MAX_ULPS)
#define ASSERT_FLOAT_MAX_ULPS  16ull
#endif
#if !defined(ASSERT_DOUBLE_MAX_ULPS)
#define ASSERT_DOUBLE_MAX_ULPS 16ull
#endif

_Static_assert(sizeof(float)*CHAR_BIT == 32,
               "assertions.c ULP comparison requires 32-bit float");
_Static_assert(sizeof(double)*CHAR_BIT == 64,
               "assertions.c ULP comparison requires 64-bit double");

CBASE_API_DECL void assert_error(char *, int32, char *, char *, ...)
    ATTR_PRINTF(4, 5);
CBASE_API_DECL void assert_file_contains(char *, int32, char *,
                                        char *, char *);
CBASE_API_DECL void assert_contains(char *, int32, char *,
                                    char *, int32, char *);
CBASE_API_DECL void assert_not_contains(char *, int32, char *,
                                        char *, int32, char *);

#define ASSERT_DECLARE_STRINGS(MODE) \
CBASE_API_DECL void a_strings_##MODE(char *, int32, char *, \
                                    char *, char *, char *, char *);
ASSERT_DECLARE_STRINGS(less)
ASSERT_DECLARE_STRINGS(less_equal)
ASSERT_DECLARE_STRINGS(equal)
ASSERT_DECLARE_STRINGS(not_equal)
ASSERT_DECLARE_STRINGS(more)
ASSERT_DECLARE_STRINGS(more_equal)
#undef ASSERT_DECLARE_STRINGS

#define ASSERT_DECLARE_POINTERS(MODE) \
CBASE_API_DECL void a_pointers_##MODE(char *, int32, char *, \
                                     char *, char *, void *, void *);
ASSERT_DECLARE_POINTERS(less)
ASSERT_DECLARE_POINTERS(less_equal)
ASSERT_DECLARE_POINTERS(equal)
ASSERT_DECLARE_POINTERS(not_equal)
ASSERT_DECLARE_POINTERS(more)
ASSERT_DECLARE_POINTERS(more_equal)
#undef ASSERT_DECLARE_POINTERS

#define ASSERT_DECLARE_INTEGERS(SIGN, MODE) \
CBASE_API_DECL void a_both_##SIGN##_##MODE(char *, int32, char *, \
                                         char *, char *, char *, char *, \
                                         llong, llong, SIGN long long, \
                                         SIGN long long);
ASSERT_DECLARE_INTEGERS(signed, less)
ASSERT_DECLARE_INTEGERS(signed, less_equal)
ASSERT_DECLARE_INTEGERS(signed, equal)
ASSERT_DECLARE_INTEGERS(signed, not_equal)
ASSERT_DECLARE_INTEGERS(signed, more)
ASSERT_DECLARE_INTEGERS(signed, more_equal)
ASSERT_DECLARE_INTEGERS(unsigned, less)
ASSERT_DECLARE_INTEGERS(unsigned, less_equal)
ASSERT_DECLARE_INTEGERS(unsigned, equal)
ASSERT_DECLARE_INTEGERS(unsigned, not_equal)
ASSERT_DECLARE_INTEGERS(unsigned, more)
ASSERT_DECLARE_INTEGERS(unsigned, more_equal)
#undef ASSERT_DECLARE_INTEGERS

#define ASSERT_DECLARE_SIGNED_UNSIGNED(MODE) \
CBASE_API_DECL void a_signed_unsigned##MODE(char *, int32, char *, \
                                           char *, char *, char *, char *, \
                                           llong, llong, llong, ullong);
ASSERT_DECLARE_SIGNED_UNSIGNED(less)
ASSERT_DECLARE_SIGNED_UNSIGNED(less_equal)
ASSERT_DECLARE_SIGNED_UNSIGNED(equal)
ASSERT_DECLARE_SIGNED_UNSIGNED(not_equal)
ASSERT_DECLARE_SIGNED_UNSIGNED(more)
ASSERT_DECLARE_SIGNED_UNSIGNED(more_equal)
#undef ASSERT_DECLARE_SIGNED_UNSIGNED

#define ASSERT_DECLARE_UNSIGNED_SIGNED(MODE) \
CBASE_API_DECL void a_unsigned_signed_##MODE(char *, int32, char *, \
                                           char *, char *, char *, char *, \
                                           llong, llong, ullong, llong);
ASSERT_DECLARE_UNSIGNED_SIGNED(less)
ASSERT_DECLARE_UNSIGNED_SIGNED(less_equal)
ASSERT_DECLARE_UNSIGNED_SIGNED(equal)
ASSERT_DECLARE_UNSIGNED_SIGNED(not_equal)
ASSERT_DECLARE_UNSIGNED_SIGNED(more)
ASSERT_DECLARE_UNSIGNED_SIGNED(more_equal)
#undef ASSERT_DECLARE_UNSIGNED_SIGNED

#define ASSERT_DECLARE_DOUBLE(MODE) \
CBASE_API_DECL void a_double_##MODE(char *, int32, char *, \
                                  char *, char *, char *, char *, \
                                  llong, llong, double, double);
ASSERT_DECLARE_DOUBLE(less)
ASSERT_DECLARE_DOUBLE(less_equal)
ASSERT_DECLARE_DOUBLE(equal)
ASSERT_DECLARE_DOUBLE(not_equal)
ASSERT_DECLARE_DOUBLE(more)
ASSERT_DECLARE_DOUBLE(more_equal)
#undef ASSERT_DECLARE_DOUBLE

#define ASSERT_DECLARE_DOUBLE_CLOSE(MODE) \
CBASE_API_DECL void a_double_##MODE(char *, int32, char *, \
                                  char *, char *, char *, char *, \
                                  llong, llong, int, int, double, double);
ASSERT_DECLARE_DOUBLE_CLOSE(close)
ASSERT_DECLARE_DOUBLE_CLOSE(not_close)
#undef ASSERT_DECLARE_DOUBLE_CLOSE

#define ASSERT_DECLARE_DOUBLE_CLOSE_TOL(MODE) \
CBASE_API_DECL void a_double_##MODE(char *, int32, char *, \
                                  char *, char *, char *, char *, \
                                  llong, llong, double, double, double);
ASSERT_DECLARE_DOUBLE_CLOSE_TOL(close_tol)
ASSERT_DECLARE_DOUBLE_CLOSE_TOL(not_close_tol)
#undef ASSERT_DECLARE_DOUBLE_CLOSE_TOL

#define ASSERT_DECLARE_BOOL(MODE) \
CBASE_API_DECL void a_bool_##MODE(char *, int32, char *, \
                                char *, char *, char *, char *, \
                                llong, llong, bool, bool);
ASSERT_DECLARE_BOOL(equal)
ASSERT_DECLARE_BOOL(not_equal)
#undef ASSERT_DECLARE_BOOL
CBASE_API_DECL noreturn void a_bool_more(void *, ...);
CBASE_API_DECL noreturn void a_bool_less(void *, ...);
CBASE_API_DECL noreturn void a_bool_more_equal(void *, ...);
CBASE_API_DECL noreturn void a_bool_less_equal(void *, ...);

void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_SIGNED(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_UNSIGNED(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_DOUBLE(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_BOOL(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE_CHARP(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE_VOIDP(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_CLOSE_FIRST(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_CLOSE_SECOND(void);

#define ASSERT(...) do {                                                       \
    if (!(__VA_ARGS__)) {                                                      \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        } else {                                                               \
            assert_error(__FILE__, __LINE__, FUNC__, "%s\n", #__VA_ARGS__);    \
            TRAP();                                                            \
        }                                                                      \
    }                                                                          \
} while (0)

#define ASSERT_NULL(VAR1) do {                                                 \
    void *ASSERT_NULL = VAR1;                                                  \
    if (ASSERT_NULL != NULL) {                                                 \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
        assert_error(__FILE__, __LINE__, FUNC__,                               \
                     "%s = %p == NULL\n", #VAR1, ASSERT_NULL);                 \
        TRAP();                                                                \
    }                                                                          \
} while (0)

#define ASSERT_ZERO(VAR1) do {                                                 \
    llong ASSERT_ZERO = VAR1;                                                  \
    if (ASSERT_ZERO != 0) {                                                    \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
        assert_error(__FILE__, __LINE__, FUNC__,                               \
                     "%s = %lld == 0\n", #VAR1, ASSERT_ZERO);                  \
        TRAP();                                                                \
    }                                                                          \
} while (0)

#define ASSERT_POSITIVE(VAR1) do {                                             \
    llong ASSERT_POSITIVE = VAR1;                                              \
    if (ASSERT_POSITIVE <= 0) {                                                \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
        assert_error(__FILE__, __LINE__, FUNC__,                               \
                     "%s = %lld > 0\n", #VAR1, ASSERT_POSITIVE);               \
        TRAP();                                                                \
    }                                                                          \
} while (0)

#define ASSERT_NEGATIVE(VAR1) do {                                             \
    llong ASSERT_NEGATIVE = VAR1;                                              \
    if (ASSERT_NEGATIVE >= 0) {                                                \
        assert_error(__FILE__, __LINE__, FUNC__,                               \
                     "%s = %lld < 0\n", #VAR1, ASSERT_NEGATIVE);               \
        TRAP();                                                                \
    }                                                                          \
} while (0)

#define ASSERT_NON_POSITIVE(VAR1) do {                                         \
    llong ASSERT_NON_POSITIVE = VAR1;                                          \
    if (ASSERT_NON_POSITIVE > 0) {                                             \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
        assert_error(__FILE__, __LINE__, FUNC__,                               \
                     "%s = %lld <= 0\n", #VAR1, ASSERT_NON_POSITIVE);          \
        TRAP();                                                                \
    }                                                                          \
} while (0)

#define ASSERT_NON_NEGATIVE(VAR1) do {                                         \
    llong ASSERT_NON_NEGATIVE = VAR1;                                          \
    if (ASSERT_NON_NEGATIVE < 0) {                                             \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
        assert_error(__FILE__, __LINE__, FUNC__,                               \
                     "%s = %lld >= 0\n", #VAR1, ASSERT_NON_NEGATIVE);          \
        TRAP();                                                                \
    }                                                                          \
} while (0)

#define ASSERT_FILE_CONTAINS(PATH, NEEDLE)           \
    assert_file_contains(__FILE__, __LINE__, FUNC__, \
                         PATH, NEEDLE)

#define ASSERT_CONTAINS(HAYSTACK, HAYSTACK_LEN, NEEDLE) \
    assert_contains(__FILE__, __LINE__, FUNC__,         \
                    HAYSTACK, HAYSTACK_LEN, NEEDLE)

#define ASSERT_NOT_CONTAINS(HAYSTACK, HAYSTACK_LEN, NEEDLE) \
    assert_not_contains(__FILE__, __LINE__, FUNC__,         \
                        HAYSTACK, HAYSTACK_LEN, NEEDLE)

#define A_BOTH_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2)             \
    a_both_signed_##MODE(__FILE__, __LINE__, FUNC__,              \
                         #VAR1, #VAR2,                            \
                         typename(TYPE1), typename(TYPE2),        \
                         typebits(TYPE1), typebits(TYPE2),        \
                         (llong)(VAR1), (llong)(VAR2))

#define A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2)         \
    a_signed_unsigned##MODE(__FILE__, __LINE__, FUNC__,           \
                            #VAR1, #VAR2,                         \
                            typename(TYPE1), typename(TYPE2),     \
                            typebits(TYPE1), typebits(TYPE2),     \
                            (llong)(VAR1), (ullong)(VAR2))

#if CHAR_MIN < 0
#define A_CHAR_FOR_SIGNED(MODE, VAR1, VAR2, TYPE1) \
    A_BOTH_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_CHAR)

#define A_CHAR_FOR_UNSIGNED(MODE, VAR1, VAR2, TYPE1) \
    A_UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_CHAR)

#define A_FIRST_CHAR(MODE, VAR1, VAR2) \
    A_FIRST_SIGNED(MODE, VAR1, VAR2, TYPE_CHAR)
#else
#define A_CHAR_FOR_SIGNED(MODE, VAR1, VAR2, TYPE1) \
    A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_CHAR)

#define A_CHAR_FOR_UNSIGNED(MODE, VAR1, VAR2, TYPE1) \
    A_BOTH_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_CHAR)

#define A_FIRST_CHAR(MODE, VAR1, VAR2) \
    A_FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_CHAR)
#endif

#define A_FIRST_SIGNED(MODE, VAR1, VAR2, TYPE1)                              \
_Generic((VAR2),                                                             \
    char:    A_CHAR_FOR_SIGNED(MODE, VAR1, VAR2, TYPE1),                     \
    schar:   A_BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_SCHAR  ),       \
    short:   A_BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_SHORT  ),       \
    int:     A_BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_INT    ),       \
    long:    A_BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_LONG   ),       \
    llong:   A_BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_LLONG  ),       \
    uchar:   A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_UCHAR  ),       \
    ushort:  A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_USHORT ),       \
    uint:    A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_UINT   ),       \
    ulong:   A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_ULONG  ),       \
    ullong:  A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_ULLONG ),       \
    float:   A_BOTH_DOUBLE(MODE,     VAR1, VAR2, TYPE1, TYPE_FLOAT  ),       \
    double:  A_BOTH_DOUBLE(MODE,     VAR1, VAR2, TYPE1, TYPE_DOUBLE ),       \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_SIGNED()                   \
)
#define A_BOTH_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2)             \
    a_both_unsigned_##MODE(__FILE__, __LINE__, FUNC__,              \
                           #VAR1, #VAR2,                            \
                           typename(TYPE1), typename(TYPE2),        \
                           typebits(TYPE1), typebits(TYPE2),        \
                           (ullong)(VAR1), (ullong)(VAR2))

#define A_UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2)           \
    a_unsigned_signed_##MODE(__FILE__, __LINE__, FUNC__,            \
                             #VAR1, #VAR2,                          \
                             typename(TYPE1), typename(TYPE2),      \
                             typebits(TYPE1), typebits(TYPE2),      \
                             (ullong)(VAR1), (llong)(VAR2))

#define A_FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE1)                        \
_Generic((VAR2),                                                         \
    char:    A_CHAR_FOR_UNSIGNED(MODE, VAR1, VAR2, TYPE1),               \
    schar:   A_UNSIGNED_SIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_SCHAR  ), \
    short:   A_UNSIGNED_SIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_SHORT  ), \
    int:     A_UNSIGNED_SIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_INT    ), \
    long:    A_UNSIGNED_SIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_LONG   ), \
    llong:   A_UNSIGNED_SIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_LLONG  ), \
    uchar:   A_BOTH_UNSIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_UCHAR  ), \
    ushort:  A_BOTH_UNSIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_USHORT ), \
    uint:    A_BOTH_UNSIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_UINT   ), \
    ulong:   A_BOTH_UNSIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_ULONG  ), \
    ullong:  A_BOTH_UNSIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_ULLONG ), \
    float:   A_BOTH_DOUBLE(MODE,       VAR1, VAR2, TYPE1, TYPE_FLOAT  ), \
    double:  A_BOTH_DOUBLE(MODE,       VAR1, VAR2, TYPE1, TYPE_DOUBLE ), \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_UNSIGNED()             \
)
#define A_BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE2)                      \
    a_double_##MODE(__FILE__, __LINE__, FUNC__,                            \
                    #VAR1, #VAR2,                                          \
                    typename(TYPE1), typename(TYPE2),                      \
                    typebits(TYPE1), typebits(TYPE2),                      \
                    DOUBLE_GET2(VAR1, TYPE1), DOUBLE_GET2(VAR2, TYPE2))

#define A_FIRST_DOUBLE(MODE, VAR1, VAR2, TYPE1) \
_Generic((VAR2), \
    char:    A_BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_CHAR   ),     \
    schar:   A_BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_SCHAR  ),     \
    short:   A_BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_SHORT  ),     \
    int:     A_BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_INT    ),     \
    long:    A_BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_LONG   ),     \
    llong:   A_BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_LLONG  ),     \
    uchar:   A_BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_UCHAR  ),     \
    ushort:  A_BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_USHORT ),     \
    uint:    A_BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_UINT   ),     \
    ulong:   A_BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_ULONG  ),     \
    ullong:  A_BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_ULLONG ),     \
    float:   A_BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_FLOAT  ),     \
    double:  A_BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_DOUBLE ),     \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_DOUBLE()             \
)
#define A_FIRST_BOOL(MODE, VAR1, VAR2, TYPE1)                  \
_Generic((VAR2),                                               \
    bool: a_bool_##MODE(__FILE__, __LINE__, FUNC__,            \
                        #VAR1, #VAR2,                          \
                        typename(TYPE1), typename(TYPE_BOOL),  \
                        typebits(TYPE1), typebits(TYPE_BOOL),  \
                        (VAR1), (VAR2)),                       \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_BOOL()       \
)
#define A_POINTERS(MODE, VAR1, VAR2)                           \
    a_pointers_##MODE(__FILE__, __LINE__, FUNC__,              \
                      #VAR1, #VAR2,                            \
                      (void *)(uintptr)(VAR1),                 \
                      (void *)(uintptr)(VAR2))

#define ASSERT_COMPARE(MODE, VAR1, VAR2)                                \
_Generic((VAR1),                                                        \
    void *: _Generic((VAR2),                                            \
        char *: A_POINTERS(MODE, VAR1, VAR2),                           \
        void *: A_POINTERS(MODE, VAR1, VAR2),                           \
        default: UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE_VOIDP()    \
    ),                                                                  \
    char *: _Generic((VAR2),                                            \
        char *: a_strings_##MODE(__FILE__, __LINE__, FUNC__,            \
                                 #VAR1, #VAR2,                          \
                                 (char *)(uintptr)(VAR1),               \
                                 (char *)(uintptr)(VAR2)),              \
        void *: A_POINTERS(MODE, VAR1, VAR2),                           \
        default: UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE_CHARP()    \
    ),                                                                  \
    char:    A_FIRST_CHAR(MODE,     VAR1, VAR2),                        \
    schar:   A_FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_SCHAR  ),          \
    short:   A_FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_SHORT  ),          \
    int:     A_FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_INT    ),          \
    long:    A_FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_LONG   ),          \
    llong:   A_FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_LLONG  ),          \
    uchar:   A_FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_UCHAR  ),          \
    ushort:  A_FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_USHORT ),          \
    uint:    A_FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_UINT   ),          \
    ulong:   A_FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_ULONG  ),          \
    ullong:  A_FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_ULLONG ),          \
    float:   A_FIRST_DOUBLE(MODE,   VAR1, VAR2, TYPE_FLOAT  ),          \
    double:  A_FIRST_DOUBLE(MODE,   VAR1, VAR2, TYPE_DOUBLE ),          \
    bool:    A_FIRST_BOOL(MODE,     VAR1, VAR2, TYPE_BOOL),             \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE()              \
)

#if CC_GCC || CC_CLANG
#define ASSERT_DIAGNOSTIC_PUSH() do {                         \
    _Pragma("GCC diagnostic push")                            \
    _Pragma("GCC diagnostic ignored \"-Waddress\"")           \
    _Pragma("GCC diagnostic ignored \"-Wpedantic\"")          \
} while (0)
#define ASSERT_DIAGNOSTIC_POP() do {                          \
    _Pragma("GCC diagnostic pop")                             \
} while (0)
#define ASSERT_COMPARE_DIAGNOSTIC(MODE, VAR1, VAR2) do {      \
    ASSERT_DIAGNOSTIC_PUSH();                                 \
    ASSERT_COMPARE(MODE, VAR1, VAR2);                         \
    ASSERT_DIAGNOSTIC_POP();                                  \
} while (0)
#define ASSERT_DOUBLE_CLOSE_ULPS_DIAGNOSTIC(MODE, VAR1, VAR2) do { \
    ASSERT_DIAGNOSTIC_PUSH();                                      \
    ASSERT_DOUBLE_CLOSE_ULPS(MODE, VAR1, VAR2);                    \
    ASSERT_DIAGNOSTIC_POP();                                       \
} while (0)
#define ASSERT_DOUBLE_CLOSE_TOL_DIAGNOSTIC(MODE, VAR1, VAR2, TOL) do { \
    ASSERT_DIAGNOSTIC_PUSH();                                          \
    ASSERT_DOUBLE_CLOSE_TOL(MODE, VAR1, VAR2, TOL);                    \
    ASSERT_DIAGNOSTIC_POP();                                           \
} while (0)
#define ASSERT_EQUAL(VAR1, VAR2) \
    ASSERT_COMPARE_DIAGNOSTIC(equal, VAR1, VAR2)
#define ASSERT_NOT_EQUAL(VAR1, VAR2) \
    ASSERT_COMPARE_DIAGNOSTIC(not_equal, VAR1, VAR2)
#define ASSERT_LESS(VAR1, VAR2) \
    ASSERT_COMPARE_DIAGNOSTIC(less, VAR1, VAR2)
#define ASSERT_LESS_EQUAL(VAR1, VAR2) \
    ASSERT_COMPARE_DIAGNOSTIC(less_equal, VAR1, VAR2)
#define ASSERT_MORE(VAR1, VAR2) \
    ASSERT_COMPARE_DIAGNOSTIC(more, VAR1, VAR2)
#define ASSERT_MORE_EQUAL(VAR1, VAR2) \
    ASSERT_COMPARE_DIAGNOSTIC(more_equal, VAR1, VAR2)
#else
#define ASSERT_EQUAL(VAR1, VAR2)      ASSERT_COMPARE(equal,      VAR1, VAR2)
#define ASSERT_NOT_EQUAL(VAR1, VAR2)  ASSERT_COMPARE(not_equal,  VAR1, VAR2)
#define ASSERT_LESS(VAR1, VAR2)       ASSERT_COMPARE(less,       VAR1, VAR2)
#define ASSERT_LESS_EQUAL(VAR1, VAR2) ASSERT_COMPARE(less_equal, VAR1, VAR2)
#define ASSERT_MORE(VAR1, VAR2)       ASSERT_COMPARE(more,       VAR1, VAR2)
#define ASSERT_MORE_EQUAL(VAR1, VAR2) ASSERT_COMPARE(more_equal, VAR1, VAR2)
#endif

#define A_BOTH_DOUBLE_CLOSE(MODE, VAR1, VAR2, TYPE1, TYPE2)                   \
    a_double_##MODE(__FILE__, __LINE__, FUNC__,                               \
                    #VAR1, #VAR2,                                             \
                    typename(TYPE1), typename(TYPE2),                         \
                    typebits(TYPE1), typebits(TYPE2),                         \
                    ASSERT_FP_KIND_EXPR(VAR1), ASSERT_FP_KIND_EXPR(VAR2),     \
                    DOUBLE_GET2(VAR1, TYPE1), DOUBLE_GET2(VAR2, TYPE2))

#define A_BOTH_DOUBLE_CLOSE_TOL(MODE, VAR1, VAR2, TOL, TYPE1, TYPE2)          \
    a_double_##MODE(__FILE__, __LINE__, FUNC__,                               \
                    #VAR1, #VAR2,                                             \
                    typename(TYPE1), typename(TYPE2),                         \
                    typebits(TYPE1), typebits(TYPE2),                         \
                    DOUBLE_GET2(VAR1, TYPE1),                                 \
                    DOUBLE_GET2(VAR2, TYPE2),                                 \
                    (double)(TOL))

#define A_FIRST_DOUBLE_CLOSE(MODE, VAR1, VAR2, TYPE1) \
_Generic((VAR2), \
    float:  A_BOTH_DOUBLE_CLOSE(MODE, VAR1, VAR2, TYPE1, TYPE_FLOAT),  \
    double: A_BOTH_DOUBLE_CLOSE(MODE, VAR1, VAR2, TYPE1, TYPE_DOUBLE), \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_CLOSE_SECOND()        \
)

#define A_FIRST_DOUBLE_CLOSE_TOL(MODE, VAR1, VAR2, TOL, TYPE1) \
_Generic((VAR2), \
    float:  A_BOTH_DOUBLE_CLOSE_TOL(MODE, VAR1, VAR2, TOL, TYPE1, TYPE_FLOAT), \
    double: A_BOTH_DOUBLE_CLOSE_TOL(MODE, VAR1, VAR2, TOL, TYPE1, TYPE_DOUBLE),\
    default: UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_CLOSE_SECOND()                \
)

#define ASSERT_DOUBLE_CLOSE_ULPS(MODE, VAR1, VAR2) \
_Generic((VAR1), \
    float:  A_FIRST_DOUBLE_CLOSE(MODE, VAR1, VAR2, TYPE_FLOAT),        \
    double: A_FIRST_DOUBLE_CLOSE(MODE, VAR1, VAR2, TYPE_DOUBLE),       \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_CLOSE_FIRST()         \
)

#define ASSERT_DOUBLE_CLOSE_TOL(MODE, VAR1, VAR2, TOL) \
_Generic((VAR1), \
    float:  A_FIRST_DOUBLE_CLOSE_TOL(MODE, VAR1, VAR2, TOL, TYPE_FLOAT),  \
    double: A_FIRST_DOUBLE_CLOSE_TOL(MODE, VAR1, VAR2, TOL, TYPE_DOUBLE), \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_CLOSE_FIRST()            \
)

#if CC_GCC || CC_CLANG
#define ASSERT_CLOSE_2(VAR1, VAR2) \
    ASSERT_DOUBLE_CLOSE_ULPS_DIAGNOSTIC(close, VAR1, VAR2)

#define ASSERT_CLOSE_3(VAR1, VAR2, TOL) \
    ASSERT_DOUBLE_CLOSE_TOL_DIAGNOSTIC(close_tol, VAR1, VAR2, TOL)

#define ASSERT_NOT_CLOSE_2(VAR1, VAR2) \
    ASSERT_DOUBLE_CLOSE_ULPS_DIAGNOSTIC(not_close, VAR1, VAR2)

#define ASSERT_NOT_CLOSE_3(VAR1, VAR2, TOL) \
    ASSERT_DOUBLE_CLOSE_TOL_DIAGNOSTIC(not_close_tol, VAR1, VAR2, TOL)
#else
#define ASSERT_CLOSE_2(VAR1, VAR2) \
    ASSERT_DOUBLE_CLOSE_ULPS(close, VAR1, VAR2)

#define ASSERT_CLOSE_3(VAR1, VAR2, TOL) \
    ASSERT_DOUBLE_CLOSE_TOL(close_tol, VAR1, VAR2, TOL)

#define ASSERT_NOT_CLOSE_2(VAR1, VAR2) \
    ASSERT_DOUBLE_CLOSE_ULPS(not_close, VAR1, VAR2)

#define ASSERT_NOT_CLOSE_3(VAR1, VAR2, TOL) \
    ASSERT_DOUBLE_CLOSE_TOL(not_close_tol, VAR1, VAR2, TOL)
#endif

#define ASSERT_CLOSE(...)     SELECT_ON_NUM_ARGS(ASSERT_CLOSE_, __VA_ARGS__)
#define ASSERT_NOT_CLOSE(...) SELECT_ON_NUM_ARGS(ASSERT_NOT_CLOSE_, __VA_ARGS__)

#endif /* ASSERTIONS_H */
