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

#if !defined(MINMAX_C)
#define MINMAX_C

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include <assert.h>
#include <signal.h>

#if !defined(error2)
#define error2(...) fprintf(stderr, __VA_ARGS__)
#endif

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_minmax 1
#elif !defined(TESTING_minmax)
#define TESTING_minmax 0
#endif

#if TESTING_minmax
#define trap(...) raise(SIGILL)
#elif !defined(trap)
#if defined(__GNUC__) || defined(__clang__)
#define trap(...) __builtin_trap()
#elif defined(_MSC_VER)
#define trap(...) __debugbreak()
#else
#define trap(...) *(volatile int *)0 = 0
#endif
#endif

#include "generic.c"
#include "assert.c"

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long ullong;

typedef signed char schar;
typedef long long llong;
typedef long double ldouble;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

// Note: NEVER delete lines with // clang-format
// clang-format off

#define GENERATE_COMPARE_POINTERS(MODE, SYMBOL) \
static void * \
get_pointer_##MODE(void *var1, void *var2) { \
    if ((uintptr_t)var1 SYMBOL (uintptr_t)var2) { \
        return var1; \
    } else { \
        return var2; \
    } \
}

GENERATE_COMPARE_POINTERS(min, <)
GENERATE_COMPARE_POINTERS(max, >)

#undef GENERATE_COMPARE_POINTERS

#define GENERATE_COMPARE_INTEGERS_SAME_SIGN(TYPE, SYMBOL, MODE) \
static TYPE long long \
get_both_##TYPE##_##MODE(TYPE long long var1, TYPE long long var2) { \
    if (var1 SYMBOL var2) { \
        return var1; \
    } else { \
        return var2; \
    } \
}

GENERATE_COMPARE_INTEGERS_SAME_SIGN(signed,   <,  min)
GENERATE_COMPARE_INTEGERS_SAME_SIGN(unsigned, <,  min)
GENERATE_COMPARE_INTEGERS_SAME_SIGN(signed,   >,  max)
GENERATE_COMPARE_INTEGERS_SAME_SIGN(unsigned, >,  max)

#undef GENERATE_COMPARE_INTEGERS_SAME_SIGN

#define GENERATE_COMPARE_SIGNED_UNSIGNED(MODE, SYMBOL) \
static llong \
get_signed_unsigned_##MODE(llong var1, ullong var2) { \
    if ((compare_sign_with_unsign(var1, var2) SYMBOL 0)) { \
        return var1; \
    } else { \
        return (llong)var2; \
    } \
}

GENERATE_COMPARE_SIGNED_UNSIGNED(min, <)
GENERATE_COMPARE_SIGNED_UNSIGNED(max, >)

#undef GENERATE_COMPARE_SIGNED_UNSIGNED

#define GENERATE_COMPARE_UNSIGNED_SIGNED(MODE, SYMBOL) \
static llong \
get_unsigned_signed_##MODE(ullong var1, llong var2) { \
    if (((-compare_sign_with_unsign(var2, var1)) SYMBOL 0)) { \
        return (llong)var1; \
    } else { \
        return var2; \
    } \
}

GENERATE_COMPARE_UNSIGNED_SIGNED(min, <)
GENERATE_COMPARE_UNSIGNED_SIGNED(max, >)

#undef GENERATE_COMPARE_UNSIGNED_SIGNED

#define GENERATE_COMPARE_LDOUBLE(MODE, SYMBOL) \
static ldouble \
get_ldouble_##MODE(ldouble var1, ldouble var2) { \
    if (var1 SYMBOL var2) { \
        return var1; \
    } else { \
        return var2; \
    } \
}

GENERATE_COMPARE_LDOUBLE(min, <)
GENERATE_COMPARE_LDOUBLE(max, >)

#undef GENERATE_COMPARE_LDOUBLE

#define BOTH_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    get_both_signed_##MODE((llong)(VAR1), (llong)(VAR2))

#define SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    get_signed_unsigned_##MODE((llong)(VAR1), (ullong)(VAR2))

#define FIRST_SIGNED(MODE, VAR1, VAR2, TYPE1) \
_Generic((VAR2), \
    schar:   BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_SCHAR  ), \
    short:   BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_SHORT  ), \
    int:     BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_INT    ), \
    long:    BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_LONG   ), \
    llong:   BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_LLONG  ), \
    uchar:   SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_UCHAR  ), \
    ushort:  SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_USHORT ), \
    uint:    SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_UINT   ), \
    ulong:   BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_ULONG  ), \
    ullong:  BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_ULLONG ), \
    float:   BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_FLOAT  ), \
    double:  BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_DOUBLE ), \
    ldouble: BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_LDOUBLE), \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_FIRST_SIGNED() \
)
void UNSUPPORTED_TYPE_FOR_GENERIC_FIRST_SIGNED(void);

#define BOTH_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    get_both_unsigned_##MODE((ullong)(VAR1), (ullong)(VAR2))

#define UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    get_unsigned_signed_##MODE((ullong)(VAR1), (llong)(VAR2))

#define FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE1) \
_Generic((VAR2), \
    schar:   UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_SCHAR  ), \
    short:   UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_SHORT  ), \
    int:     UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_INT    ), \
    long:    UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_LONG   ), \
    llong:   UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_LLONG  ), \
    uchar:   BOTH_UNSIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_UCHAR  ), \
    ushort:  BOTH_UNSIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_USHORT ), \
    uint:    BOTH_UNSIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_UINT   ), \
    ulong:   BOTH_UNSIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_ULONG  ), \
    ullong:  BOTH_UNSIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_ULLONG ), \
    float:   BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_FLOAT  ), \
    double:  BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_DOUBLE ), \
    ldouble: BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_LDOUBLE), \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_FIRST_UNSIGNED() \
)
void UNSUPPORTED_TYPE_FOR_GENERIC_FIRST_UNSIGNED(void);

#define BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    get_ldouble_##MODE(LDOUBLE_GET2(VAR1, TYPE1), LDOUBLE_GET2(VAR2, TYPE2))

#define FIRST_LDOUBLE(MODE, VAR1, VAR2, TYPE1) \
_Generic((VAR2), \
    schar:   BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_SCHAR  ), \
    short:   BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_SHORT  ), \
    int:     BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_INT    ), \
    long:    BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_LONG   ), \
    llong:   BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_LLONG  ), \
    uchar:   BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_UCHAR  ), \
    ushort:  BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_USHORT ), \
    uint:    BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_UINT   ), \
    ulong:   BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_ULONG  ), \
    ullong:  BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_ULLONG ), \
    float:   BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_FLOAT  ), \
    double:  BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_DOUBLE ), \
    ldouble: BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_LDOUBLE), \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_FIRST_LDOUBLE() \
)
void UNSUPPORTED_TYPE_FOR_GENERIC_FIRST_LDOUBLE(void);

#define POINTERS(MODE, VAR1, VAR2) \
    get_pointer_##MODE((void *)(uintptr_t)(VAR1), (void *)(uintptr_t)(VAR2))

void UNSUPPORTED_TYPE_FOR_GENERIC_MINMAX_COMPARE_CHARP(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_MINMAX_COMPARE_VOIDP(void);
#define MINMAX_COMPARE(MODE, VAR1, VAR2) \
_Generic((VAR1), \
    void *: _Generic((VAR2), \
        char *: POINTERS(MODE, VAR1, VAR2), \
        void *: POINTERS(MODE, VAR1, VAR2), \
        default: UNSUPPORTED_TYPE_FOR_GENERIC_MINMAX_COMPARE_VOIDP() \
    ), \
    schar:   FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_SCHAR  ), \
    short:   FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_SHORT  ), \
    int:     FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_INT    ), \
    long:    FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_LONG   ), \
    llong:   FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_LLONG  ), \
    uchar:   FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_UCHAR  ), \
    ushort:  FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_USHORT ), \
    uint:    FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_UINT   ), \
    ulong:   FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_ULONG  ), \
    ullong:  FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_ULLONG ), \
    float:   FIRST_LDOUBLE(MODE,  VAR1, VAR2, TYPE_FLOAT  ), \
    double:  FIRST_LDOUBLE(MODE,  VAR1, VAR2, TYPE_DOUBLE ), \
    ldouble: FIRST_LDOUBLE(MODE,  VAR1, VAR2, TYPE_LDOUBLE)  \
)

#if !defined(MIN)
#define MIN(VAR1, VAR2) MINMAX_COMPARE(min, VAR1, VAR2)
#define MAX(VAR1, VAR2) MINMAX_COMPARE(max, VAR1, VAR2)
#endif

// clang-format on

#if TESTING_minmax

// Note: NEVER delete lines with // clang-format
// clang-format off
int
main(void) {
    {
        long min01 = MIN(0, 1);
        long min11 = MIN(1, 1);
        long max11 = MAX(1, 1);
        long max01 = MAX(0, 1);

        ASSERT_EQUAL(min01, 0);
        ASSERT_EQUAL(min11, 1);
        ASSERT_EQUAL(max11, 1);
        ASSERT_EQUAL(max01, 1);
    }{
        int a = 1;
        int b = 1;
        long min = MIN(a, b);
        long max = MAX(a, b);
        ASSERT_EQUAL(min, a);
        ASSERT_EQUAL(max, a);
    }{
        int a = 1;
        uint b = 2;
        long min = MIN(a, b);
        long max = MAX(a, b);
        ASSERT_EQUAL(min, a);
        ASSERT_EQUAL(max, b);
    }{
        long a = -1;
        ulong b = 0;
        long double min = MIN(a, b);
        long double max = MAX(a, b);
        ASSERT_EQUAL(min, a);
        ASSERT_EQUAL(max, b);
    }{
        long a = MINOF(a);
        ulong b = MAXOF(b);
        long double min = MIN(a, b);
        long double max = MAX(a, b);
        ASSERT_EQUAL(min, a);
        ASSERT_EQUAL(max, (long double)b);
    }{
        ulong a = MINOF(a);
        long b = MAXOF(b);
        long min = MIN(a, b);
        long max = MAX(a, b);
        ASSERT_EQUAL(min, a);
        ASSERT_EQUAL(max, b);
    }{
        long a = -1;
        long min = MIN(a, 0);
        long max = MAX(a, 0);
        ASSERT_EQUAL(min, -1);
        ASSERT_EQUAL(max, 0);
    }{
        double a = 0.123;
        ldouble min = MIN(a, 0);
        ldouble max = MAX(a, 0);
        ASSERT_EQUAL(min, 0.0);
        ASSERT_EQUAL(max, a);
    }{
        int array[100];
        void *a = &array[0];
        void *b = &array[1];
        void *min = MIN(a, b);
        void *max = MAX(a, b);
        ASSERT_EQUAL(min, a);
        ASSERT_EQUAL(max, b);
    }
    exit(EXIT_SUCCESS);
}
// clang-format on
#endif

#endif
