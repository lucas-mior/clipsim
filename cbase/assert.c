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

#if !defined(ASSERT_C)
#define ASSERT_C

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <assert.h>
#include <signal.h>

#if !defined(error2)
#define error2(...) fprintf(stderr, __VA_ARGS__)
#endif

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_assert 1
#elif !defined(TESTING_assert)
#define TESTING_assert 0
#endif

#if 1 == TESTING_assert
  #define TRAP(...) raise(SIGILL)
#elif !defined(TRAP)
  #if defined(__GNUC__) || defined(__clang__)
  #define TRAP(...) __builtin_trap()
  #elif defined(_MSC_VER)
  #define TRAP(...) __debugbreak()
  #else
  #define TRAP(...) *(int *)0 = 0
  #endif
#endif

#include "generic.c"
#include "primitives.h"

#define ASSERT(C) do {                                 \
    if (!(C)) {                                        \
        error2("%s: Assertion '%s' failed at %s:%d\n", \
               __func__, #C, __FILE__, __LINE__);      \
        TRAP();                                        \
    }                                                  \
} while (0)

#define GENERATE_ASSERT_STRINGS(MODE, SYMBOL)                            \
static void                                                              \
a_strings_##MODE(char *file, uint line, char *func,                      \
                 char *name1, char *name2,                               \
                 char *var1, char *var2) {                               \
    if (var1 == NULL) {                                                  \
        error2("\n%s: Error in assertion at %s:%u\n", func, file, line); \
        error2("%s is NULL\n", name1);                                   \
        TRAP();                                                          \
    }                                                                    \
    if (var2 == NULL) {                                                  \
        error2("\n%s: Error in assertion at %s:%u\n", func, file, line); \
        error2("%s is NULL\n", name2);                                   \
        TRAP();                                                          \
    }                                                                    \
    if (!(strcmp(var1, var2) SYMBOL 0)) {                                \
        error2("\n%s: Assertion failed at %s:%u\n", func, file, line);   \
        error2("%s = %s " #SYMBOL " %s = %s\n",                          \
               name1, var1, var2, name2);                                \
        TRAP();                                                          \
    }                                                                    \
    return;                                                              \
}

GENERATE_ASSERT_STRINGS(less,        <)
GENERATE_ASSERT_STRINGS(less_equal, <=)
GENERATE_ASSERT_STRINGS(equal,      ==)
GENERATE_ASSERT_STRINGS(not_equal,  !=)
GENERATE_ASSERT_STRINGS(more,        >)
GENERATE_ASSERT_STRINGS(more_equal, >=)

#undef GENERATE_ASSERT_STRINGS

#define GENERATE_ASSERT_POINTERS(MODE, SYMBOL)                           \
static void                                                              \
a_pointers_##MODE(char *file, uint line, char *func,                     \
                  char *name1, char *name2,                              \
                  void *var1, void *var2) {                              \
    if (!((uintptr_t)var1 SYMBOL (uintptr_t)var2)) {                     \
        error2("\n%s: Assertion failed at %s:%u\n", func, file, line);   \
        error2("%s = %p " #SYMBOL " %p = %s\n",                          \
               name1, var1, var2, name2);                                \
        TRAP();                                                          \
    }                                                                    \
    return;                                                              \
}

GENERATE_ASSERT_POINTERS(less,        <)
GENERATE_ASSERT_POINTERS(less_equal, <=)
GENERATE_ASSERT_POINTERS(equal,      ==)
GENERATE_ASSERT_POINTERS(not_equal,  !=)
GENERATE_ASSERT_POINTERS(more,        >)
GENERATE_ASSERT_POINTERS(more_equal, >=)

#undef GENERATE_ASSERT_POINTERS

#define GENERATE_ASSERT_INTEGERS_SAME_SIGN(TYPE, FORMAT, SYMBOL, MODE)      \
static void                                                                 \
a_both_##TYPE##_##MODE(char *file, uint line, char *func,                   \
                       char *name1, char *name2,                            \
                       char *type1, char *type2,                            \
                       llong bits1, llong bits2,                            \
                       TYPE long long var1, TYPE long long var2) {          \
    if (!(var1 SYMBOL var2)) {                                              \
        error2("\n%s: Assertion failed at %s:%u\n", func, file, line);      \
        error2("[%s%lld]%s = "FORMAT" " #SYMBOL " "FORMAT" = %s[%s%lld]\n", \
               type1, bits1, name1, var1, var2, name2, type2, bits2);       \
        TRAP();                                                             \
    }                                                                       \
    return;                                                                 \
}

GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed,   "%lld", ==, equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llu", ==, equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed,   "%lld", !=, not_equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llu", !=, not_equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed,   "%lld", <,  less)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llu", <,  less)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed,   "%lld", <=, less_equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llu", <=, less_equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed,   "%lld", >,  more)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llu", >,  more)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed,   "%lld", >=, more_equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llu", >=, more_equal)

#undef GENERATE_ASSERT_INTEGERS_SAME_SIGN

static int
compare_sign_with_unsign(llong s, ullong u) {
    ullong saux;
    if (s < 0) {
        return -1;
    }
    saux = (ullong)s;
    if (saux < u) {
        return -1;
    } else if (saux == u) {
        return 0;
    } else {
        return +1;
    }
}

#define GENERATE_ASSERT_SIGNED_UNSIGNED(MODE, SYMBOL)                   \
static void                                                             \
a_signed_unsigned##MODE(char *file, uint line, char *func,              \
                        char *name1, char *name2,                       \
                        char *type1, char *type2,                       \
                        llong bits1, llong bits2,                       \
                        llong var1, ullong var2) {                      \
    if (!(compare_sign_with_unsign(var1, var2) SYMBOL 0)) {             \
        error2("\n%s: Assertion failed at %s:%u\n", func, file, line);  \
        error2("[%s%lld]%s = %lld " #SYMBOL " %llu = %s[%s%lld]\n",     \
               type1, bits1, name1, var1, var2, name2, type2, bits2);   \
        TRAP();                                                         \
    }                                                                   \
    return;                                                             \
}

GENERATE_ASSERT_SIGNED_UNSIGNED(equal,      ==)
GENERATE_ASSERT_SIGNED_UNSIGNED(not_equal,  !=)
GENERATE_ASSERT_SIGNED_UNSIGNED(less,        <)
GENERATE_ASSERT_SIGNED_UNSIGNED(less_equal, <=)
GENERATE_ASSERT_SIGNED_UNSIGNED(more,        >)
GENERATE_ASSERT_SIGNED_UNSIGNED(more_equal, >=)

#undef GENERATE_ASSERT_SIGNED_UNSIGNED

#define GENERATE_ASSERT_UNSIGNED_SIGNED(MODE, SYMBOL)                   \
static void                                                             \
a_unsigned_signed_##MODE(char *file, uint line, char *func,             \
                         char *name1, char *name2,                      \
                         char *type1, char *type2,                      \
                         llong bits1, llong bits2,                      \
                         ullong var1, llong var2) {                     \
    if (!((-compare_sign_with_unsign(var2, var1)) SYMBOL 0)) {          \
        error2("\n%s: Assertion failed at %s:%u\n", func, file, line);  \
        error2("[%s%lld]%s = %llu " #SYMBOL " %lld = %s[%s%lld]\n",     \
               type1, bits1, name1, var1, var2, name2, type2, bits2);   \
        TRAP();                                                         \
    }                                                                   \
    return;                                                             \
}

GENERATE_ASSERT_UNSIGNED_SIGNED(equal,      ==)
GENERATE_ASSERT_UNSIGNED_SIGNED(not_equal,  !=)
GENERATE_ASSERT_UNSIGNED_SIGNED(less,        <)
GENERATE_ASSERT_UNSIGNED_SIGNED(less_equal, <=)
GENERATE_ASSERT_UNSIGNED_SIGNED(more,        >)
GENERATE_ASSERT_UNSIGNED_SIGNED(more_equal, >=)

#undef GENERATE_ASSERT_UNSIGNED_SIGNED

#define GENERATE_ASSERT_LDOUBLE(MODE, SYMBOL)                                         \
static void                                                                           \
a_ldouble_##MODE(char *file, uint line, char *func,                                   \
                 char *name1, char *name2,                                            \
                 char *type1, char *type2,                                            \
                 llong bits1, llong bits2,                                            \
                 ldouble var1, ldouble var2) {                                        \
    if (!(var1 SYMBOL var2)) {                                                        \
        error2("\n%s: Assertion failed at %s:%u\n", func, file, line);                \
        error2("[%s%lld]%s = "LDOUBLE_FORMAT #SYMBOL LDOUBLE_FORMAT" = %s[%s%lld]\n", \
               type1, bits1, name1, var1, var2, name2, type2, bits2);                 \
        TRAP();                                                                       \
    }                                                                                 \
    return;                                                                           \
}

GENERATE_ASSERT_LDOUBLE(equal,      ==)
GENERATE_ASSERT_LDOUBLE(not_equal,  !=)
GENERATE_ASSERT_LDOUBLE(less,        <)
GENERATE_ASSERT_LDOUBLE(less_equal, <=)
GENERATE_ASSERT_LDOUBLE(more,        >)
GENERATE_ASSERT_LDOUBLE(more_equal, >=)

#undef GENERATE_ASSERT_LDOUBLE

#define GENERATE_ASSERT_BOOLS(MODE, SYMBOL)                            \
static void                                                            \
a_bool_##MODE(char *file, uint line, char *func,                       \
              char *name1, char *name2,                                \
              char *type1, char *type2,                                \
              llong bits1, llong bits2,                                \
              bool var1, bool var2) {                                  \
    if (!(var1 SYMBOL var2)) {                                         \
        char *s1 = "false";                                            \
        char *s2 = "false";                                            \
        if (var1) {                                                    \
            s1 = "true";                                               \
        }                                                              \
        if (var2) {                                                    \
            s2 = "true";                                               \
        }                                                              \
        error2("\n%s: Assertion failed at %s:%u\n", func, file, line); \
        error2("[%s%lld]%s = %s " #SYMBOL " %s = %s[%s%lld]\n",        \
               type1, bits1, name1, s1, s2, name2, type2, bits2);      \
        TRAP();                                                        \
    }                                                                  \
    return;                                                            \
}

GENERATE_ASSERT_BOOLS(equal,      ==)
GENERATE_ASSERT_BOOLS(not_equal,  !=)
GENERATE_ASSERT_BOOLS(less,        <)
GENERATE_ASSERT_BOOLS(less_equal, <=)
GENERATE_ASSERT_BOOLS(more,        >)
GENERATE_ASSERT_BOOLS(more_equal, >=)

#undef GENERATE_ASSERT_BOOLS

#define A_BOTH_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2)             \
    a_both_signed_##MODE(__FILE__, __LINE__, (char *)__func__,    \
                         #VAR1, #VAR2,                            \
                         typename(TYPE1), typename(TYPE2),        \
                         typebits(TYPE1), typebits(TYPE2),        \
                         (llong)(VAR1), (llong)(VAR2))

#define A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2)         \
    a_signed_unsigned##MODE(__FILE__, __LINE__, (char *)__func__, \
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

#define A_FIRST_SIGNED(MODE, VAR1, VAR2, TYPE1) \
_Generic((VAR2), \
    char:    A_CHAR_FOR_SIGNED(MODE, VAR1, VAR2, TYPE1),        \
    schar:   A_BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_SCHAR  ), \
    short:   A_BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_SHORT  ), \
    int:     A_BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_INT    ), \
    long:    A_BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_LONG   ), \
    llong:   A_BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_LLONG  ), \
    uchar:   A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_UCHAR  ), \
    ushort:  A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_USHORT ), \
    uint:    A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_UINT   ), \
    ulong:   A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_ULONG  ), \
    ullong:  A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_ULLONG ), \
    float:   A_BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_FLOAT  ), \
    double:  A_BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_DOUBLE ), \
    default: _Generic((VAR2), \
      ldouble: A_BOTH_LDOUBLE(MODE,  VAR1, VAR2, TYPE1, TYPE_LDOUBLE), \
      default: UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_SIGNED() \
    ) \
)
void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_SIGNED(void);

#define A_BOTH_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2)             \
    a_both_unsigned_##MODE(__FILE__, __LINE__, (char *)__func__,    \
                           #VAR1, #VAR2,                            \
                           typename(TYPE1), typename(TYPE2),        \
                           typebits(TYPE1), typebits(TYPE2),        \
                           (ullong)(VAR1), (ullong)(VAR2))

#define A_UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2)           \
    a_unsigned_signed_##MODE(__FILE__, __LINE__, (char *)__func__,  \
                             #VAR1, #VAR2,                          \
                             typename(TYPE1), typename(TYPE2),      \
                             typebits(TYPE1), typebits(TYPE2),      \
                             (ullong)(VAR1), (llong)(VAR2))

#define A_FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE1) \
_Generic((VAR2), \
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
    float:   A_BOTH_LDOUBLE(MODE,      VAR1, VAR2, TYPE1, TYPE_FLOAT  ), \
    double:  A_BOTH_LDOUBLE(MODE,      VAR1, VAR2, TYPE1, TYPE_DOUBLE ), \
    default: _Generic((VAR2), \
      ldouble: A_BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_LDOUBLE), \
      default: UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_UNSIGNED() \
    ) \
)
void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_UNSIGNED(void);

#define A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    a_ldouble_##MODE(__FILE__, __LINE__, (char *)__func__,       \
                     #VAR1, #VAR2,                     \
                     typename(TYPE1), typename(TYPE2), \
                     typebits(TYPE1), typebits(TYPE2), \
                     LDOUBLE_GET2(VAR1, TYPE1), LDOUBLE_GET2(VAR2, TYPE2))

#define A_FIRST_LDOUBLE(MODE, VAR1, VAR2, TYPE1) \
_Generic((VAR2), \
    char:    A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_CHAR   ),     \
    schar:   A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_SCHAR  ),     \
    short:   A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_SHORT  ),     \
    int:     A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_INT    ),     \
    long:    A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_LONG   ),     \
    llong:   A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_LLONG  ),     \
    uchar:   A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_UCHAR  ),     \
    ushort:  A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_USHORT ),     \
    uint:    A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_UINT   ),     \
    ulong:   A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_ULONG  ),     \
    ullong:  A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_ULLONG ),     \
    float:   A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_FLOAT  ),     \
    double:  A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_DOUBLE ),     \
    default: _Generic((VAR2),                                           \
        ldouble: A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_LDOUBLE), \
        default: UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_LDOUBLE()         \
    ) \
)
void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_LDOUBLE(void);

#define A_FIRST_BOOL(MODE, VAR1, VAR2, TYPE1)                  \
_Generic((VAR2),                                               \
    bool: a_bool_##MODE(__FILE__, __LINE__, (char *)__func__,  \
                        #VAR1, #VAR2,                          \
                        typename(TYPE1), typename(TYPE_BOOL),  \
                        typebits(TYPE1), typebits(TYPE_BOOL),  \
                        (VAR1), (VAR2)),                       \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_BOOL()       \
)
void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_BOOL(void);

#define A_POINTERS(MODE, VAR1, VAR2)                           \
    a_pointers_##MODE(__FILE__, __LINE__, (char *)__func__,    \
                      #VAR1, #VAR2,                            \
                      (void *)(uintptr_t)(VAR1),               \
                      (void *)(uintptr_t)(VAR2))

void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE_CHARP(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE_VOIDP(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE(void);
#define ASSERT_COMPARE(MODE, VAR1, VAR2)                                \
_Generic((VAR1),                                                        \
    void *: _Generic((VAR2),                                            \
        char *: A_POINTERS(MODE, VAR1, VAR2),                           \
        void *: A_POINTERS(MODE, VAR1, VAR2),                           \
        default: UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE_VOIDP()    \
    ),                                                                  \
    char *: _Generic((VAR2),                                            \
        char *: a_strings_##MODE(__FILE__, __LINE__, (char *)__func__,  \
                                 #VAR1, #VAR2,                          \
                                 (char *)(uintptr_t)(VAR1),             \
                                 (char *)(uintptr_t)(VAR2)),            \
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
    float:   A_FIRST_LDOUBLE(MODE,  VAR1, VAR2, TYPE_FLOAT  ),          \
    double:  A_FIRST_LDOUBLE(MODE,  VAR1, VAR2, TYPE_DOUBLE ),          \
    bool:    A_FIRST_BOOL(MODE,     VAR1, VAR2, TYPE_BOOL),             \
    default: _Generic((VAR1),                                           \
      ldouble: A_FIRST_LDOUBLE(MODE,  VAR1, VAR2, TYPE_LDOUBLE),        \
      default: UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE()            \
    )                                                                   \
)

#define ASSERT_EQUAL(VAR1, VAR2)      ASSERT_COMPARE(equal,      VAR1, VAR2)
#define ASSERT_NOT_EQUAL(VAR1, VAR2)  ASSERT_COMPARE(not_equal,  VAR1, VAR2)
#define ASSERT_LESS(VAR1, VAR2)       ASSERT_COMPARE(less,       VAR1, VAR2)
#define ASSERT_LESS_EQUAL(VAR1, VAR2) ASSERT_COMPARE(less_equal, VAR1, VAR2)
#define ASSERT_MORE(VAR1, VAR2)       ASSERT_COMPARE(more,       VAR1, VAR2)
#define ASSERT_MORE_EQUAL(VAR1, VAR2) ASSERT_COMPARE(more_equal, VAR1, VAR2)

#define ASSERT_NULL(VAR1) do {                                          \
    void *p = VAR1;                                                     \
    if (p != NULL) {                                                    \
        error2("\n%s: Assertion failed at %s:%d\n",                     \
               __func__, __FILE__, __LINE__);                           \
        error2("%s = %p == NULL\n", #VAR1, p);                          \
        TRAP();                                                         \
    }                                                                   \
} while (0)

#if 0 == TESTING_assert
static inline void
assert_functions_sink(void) {
    (void)a_strings_less;
    (void)a_strings_less_equal;
    (void)a_strings_equal;
    (void)a_strings_not_equal;
    (void)a_strings_more;
    (void)a_strings_more_equal;

    (void)a_pointers_less;
    (void)a_pointers_less_equal;
    (void)a_pointers_equal;
    (void)a_pointers_not_equal;
    (void)a_pointers_more;
    (void)a_pointers_more_equal;

    (void)a_both_signed_less;
    (void)a_both_signed_less_equal;
    (void)a_both_signed_equal;
    (void)a_both_signed_not_equal;
    (void)a_both_signed_more;
    (void)a_both_signed_more_equal;

    (void)a_both_unsigned_less;
    (void)a_both_unsigned_less_equal;
    (void)a_both_unsigned_equal;
    (void)a_both_unsigned_not_equal;
    (void)a_both_unsigned_more;
    (void)a_both_unsigned_more_equal;

    (void)a_signed_unsignedless;
    (void)a_signed_unsignedless_equal;
    (void)a_signed_unsignedequal;
    (void)a_signed_unsignednot_equal;
    (void)a_signed_unsignedmore;
    (void)a_signed_unsignedmore_equal;

    (void)a_unsigned_signed_less;
    (void)a_unsigned_signed_less_equal;
    (void)a_unsigned_signed_equal;
    (void)a_unsigned_signed_not_equal;
    (void)a_unsigned_signed_more;
    (void)a_unsigned_signed_more_equal;

    (void)a_ldouble_less;
    (void)a_ldouble_less_equal;
    (void)a_ldouble_equal;
    (void)a_ldouble_not_equal;
    (void)a_ldouble_more;
    (void)a_ldouble_more_equal;

    (void)a_bool_equal;
    (void)a_bool_not_equal;
    return;
}
#endif

#if TESTING_assert

#include <signal.h>
#include <setjmp.h>

static sig_atomic_t assertion_failed = false;
static sigjmp_buf assert_env;

static void __attribute__((noreturn))
handler_failed_assertion(int unused) {
    (void)unused;
    assertion_failed = true;
    siglongjmp(assert_env, 1);
}

int
main(void) {
    {
        char *string = NULL;
        void *pointer = NULL;
        ASSERT_EQUAL(string, pointer);
        ASSERT_NULL(string);
    }
    {
        int a = 1;
        int b = 1;
        ASSERT_EQUAL(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE_EQUAL(a, b);
    }{
        int a = 1;
        uint b = 1;
        ASSERT_EQUAL(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE_EQUAL(a, b);
    }{
        int a = 1;
        uint b = 2;
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    }{
        long a = -1;
        ulong b = 0;
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    }{
        long a = MINOF(a);
        ulong b = MAXOF(b);
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    }{
        ulong a = MINOF(a);
        long b = MAXOF(b);
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    }{
        char *a = "aaa";
        char *b = "aaa";
        ASSERT_EQUAL(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE_EQUAL(b, a);
    }{
        char *a = "aaa";
        char *b = "bbb";
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    }{
        long a = -1;
        ASSERT_NOT_EQUAL(a, 0);
        ASSERT_LESS(a, 0);
        ASSERT_LESS_EQUAL(a, 0);
        ASSERT_MORE(0, a);
        ASSERT_MORE_EQUAL(0, a);
    }{
        double a = 0.123;
        ASSERT_NOT_EQUAL(a, 0.123000001);
        ASSERT_LESS(a, 0.123000001);
        ASSERT_LESS_EQUAL(a, 0.123000001);
        ASSERT_MORE(0.123000001, a);
        ASSERT_MORE_EQUAL(0.123000001, a);
    }{
        long a = -1;
        double b = -1;
        ASSERT_EQUAL(a, b);
        ASSERT_EQUAL(b, b);
        ASSERT_MORE_EQUAL(a, b);
        ASSERT_LESS_EQUAL(a, b);
    }{
        double a = -1;
        long b = -1;
        ASSERT_EQUAL(a, b);
        ASSERT_EQUAL(b, b);
        ASSERT_MORE_EQUAL(a, b);
        ASSERT_LESS_EQUAL(a, b);
    }{
        double a = -1;
        double b = 0;
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    }{
        float a = -1;
        double b = 1;
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    }{
        llong a = 1;
        ldouble b = 1;
        ASSERT_EQUAL(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE_EQUAL(b, a);
    }{
        void *a = NULL;
        void *b = &a;
        ASSERT_NOT_EQUAL(a, b);
    }{
        int array[100];
        void *a = &array[0];
        void *b = &array[1];
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    }{
        bool a = true;
        bool b = true;
        ASSERT_EQUAL(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE_EQUAL(a, b);
    }{
        bool a = true;
        bool b = false;
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_MORE(a, b);
        ASSERT_MORE_EQUAL(a, b);
        ASSERT_LESS(b, a);
        ASSERT_LESS_EQUAL(b, a);
    }{
        // uncomment to trigger linking error
        /* double x = 0.1; */
        /* void *a = NULL; */
        /* ASSERT_MORE_EQUAL(x, a); */
        /* ASSERT_MORE_EQUAL(a, x); */
        /* bool b = true; */
        /* ASSERT_EQUAL(b, 1); */
    }{
        int a = 0;
        double b = 1;
        float array[10] = {0};
        char *string_null = NULL;
        char *string_some = "some";

        struct sigaction signal_action;
        signal_action.sa_handler = handler_failed_assertion;
        sigemptyset(&signal_action.sa_mask);
        signal_action.sa_flags = SA_RESTART;

        if (sigaction(SIGILL, &signal_action, NULL) != 0) {
            error2("Error in sigaction: %s.\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        error2("\nThe following assertions are supposed to fail");
        error2("\nand then check the 'assertion_failed' variable\n");

        if (sigsetjmp(assert_env, 1) == 0) {
            ASSERT_EQUAL(a, b);
        }
        ASSERT(assertion_failed);
        assertion_failed = false;

        if (sigsetjmp(assert_env, 1) == 0) {
            ASSERT_EQUAL(string_null, string_some);
        }
        ASSERT(assertion_failed);
        assertion_failed = false;

        if (sigsetjmp(assert_env, 1) == 0) {
            ASSERT_MORE(a, b);
        }
        ASSERT(assertion_failed);
        assertion_failed = false;

        if (sigsetjmp(assert_env, 1) == 0) {
            ASSERT_LESS(b, a);
        }
        ASSERT(assertion_failed);
        assertion_failed = false;

        if (sigsetjmp(assert_env, 1) == 0) {
            ASSERT_MORE_EQUAL(a, b);
        }
        ASSERT(assertion_failed);
        assertion_failed = false;

        if (sigsetjmp(assert_env, 1) == 0) {
            ASSERT_LESS_EQUAL(b, a);
        }
        ASSERT(assertion_failed);
        assertion_failed = false;

        if (sigsetjmp(assert_env, 1) == 0) {
            ASSERT_LESS((void *)&array[1], (void *)&array[0]);
        }
        ASSERT(assertion_failed);
        assertion_failed = false;

        if (sigsetjmp(assert_env, 1) == 0) {
            ASSERT_EQUAL(true, false);
        }
        ASSERT(assertion_failed);
        assertion_failed = false;
    }
    ASSERT(true);
    ASSERT(!false);
    ASSERT_EQUAL(true, true);
    ASSERT_EQUAL(0 < 1, 1 < 2);
    exit(EXIT_SUCCESS);
}
#endif

#endif /* ASSERT_C */
