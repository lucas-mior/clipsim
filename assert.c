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

#include "generic.c"

#define error2(...) fprintf(stderr, __VA_ARGS__)

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_assert 1
#elif !defined(TESTING_assert)
#define TESTING_assert 0
#endif

#if TESTING_assert
#define trap(...) raise(SIGILL)
#else
#if defined(__GNUC__) || defined(__clang__)
#define trap(...) __builtin_trap()
#elif defined(_MSC_VER)
#define trap(...) __debugbreak()
#else
#define trap(...) *(volatile int *)0 = 0
#endif
#endif

#define ASSERT(C) do { \
    if (!(C)) { \
        error2("Assertion '%s' failed at %s:%d\n", #C, __FILE__, __LINE__); \
        trap(); \
    } \
} while (0)

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

// clang-format off

#define GENERATE_ASSERT_STRINGS(MODE, SYMBOL) \
static void \
a_strings_##MODE(char *file, uint line, \
                 char *name1, char *name2, \
                 char *var1, char *var2) { \
    if (!(strcmp(var1, var2) SYMBOL 0)) { \
        error2("\n%s: Assertion failed at %s:%u\n", __func__, file, line); \
        error2("%s = %s " #SYMBOL " %s = %s\n", \
               name1, var1, var2, name2); \
        trap(); \
    } \
}

GENERATE_ASSERT_STRINGS(less,       <)
GENERATE_ASSERT_STRINGS(less_equal, <=)
GENERATE_ASSERT_STRINGS(equal,      ==)
GENERATE_ASSERT_STRINGS(not_equal,  !=)
GENERATE_ASSERT_STRINGS(more,       >)
GENERATE_ASSERT_STRINGS(more_equal, >=)

#undef GENERATE_ASSERT_STRINGS

#define GENERATE_ASSERT_POINTERS(MODE, SYMBOL) \
static void \
a_pointers_##MODE(char *file, uint line, \
                  char *name1, char *name2, \
                  void *var1, void *var2) { \
    if (!((uintptr_t)var1 SYMBOL (uintptr_t)var2)) { \
        error2("\n%s: Assertion failed at %s:%u\n", __func__, file, line); \
        error2("%s = %p " #SYMBOL " %p = %s\n", \
               name1, var1, var2, name2); \
        trap(); \
    } \
}

GENERATE_ASSERT_POINTERS(less,       <)
GENERATE_ASSERT_POINTERS(less_equal, <=)
GENERATE_ASSERT_POINTERS(equal,      ==)
GENERATE_ASSERT_POINTERS(not_equal,  !=)
GENERATE_ASSERT_POINTERS(more,       >)
GENERATE_ASSERT_POINTERS(more_equal, >=)

#define GENERATE_ASSERT_INTEGERS_SAME_SIGN(TYPE, FORMAT, SYMBOL, MODE) \
static void \
a_both_##TYPE##_##MODE(char *file, uint line, \
                       char *name1, char *name2, \
                       char *type1, char *type2, \
                       uint bits1, uint bits2, \
                       TYPE long long var1, TYPE long long var2) { \
    if (!(var1 SYMBOL var2)) { \
        error2("\n%s: Assertion failed at %s:%u\n", __func__, file, line); \
        error2("[%s%u]%s = "FORMAT" " #SYMBOL " "FORMAT" = %s[%s%u]\n", \
               type1, bits1, name1, var1, var2, name2, type2, bits2); \
        trap(); \
    } \
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

// clang-format on

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

// clang-format off

#define GENERATE_ASSERT_SIGNED_UNSIGNED(MODE, SYMBOL) \
static void \
a_signed_unsigned##MODE(char *file, uint line, \
                        char *name1, char *name2, \
                        char *type1, char *type2, \
                        uint bits1, uint bits2, \
                        llong var1, ullong var2) { \
    if (!(compare_sign_with_unsign(var1, var2) SYMBOL 0)) { \
        error2("\n%s: Assertion failed at %s:%u\n", __func__, file, line); \
        error2("[%s%u]%s = %lld " #SYMBOL " %llu = %s[%s%u]\n", \
               type1, bits1, name1, var1, var2, name2, type2, bits2); \
        trap(); \
    } \
}

GENERATE_ASSERT_SIGNED_UNSIGNED(equal,      ==)
GENERATE_ASSERT_SIGNED_UNSIGNED(not_equal,  !=)
GENERATE_ASSERT_SIGNED_UNSIGNED(less,       <)
GENERATE_ASSERT_SIGNED_UNSIGNED(less_equal, <=)
GENERATE_ASSERT_SIGNED_UNSIGNED(more,       >)
GENERATE_ASSERT_SIGNED_UNSIGNED(more_equal, >=)

#undef GENERATE_ASSERT_SIGNED_UNSIGNED

#define GENERATE_ASSERT_UNSIGNED_SIGNED(MODE, SYMBOL) \
static void \
a_unsigned_signed_##MODE(char *file, uint line, \
                         char *name1, char *name2, \
                         char *type1, char *type2, \
                         uint bits1, uint bits2, \
                         ullong var1, llong var2) { \
    if (!((-compare_sign_with_unsign(var2, var1)) SYMBOL 0)) { \
        error2("\n%s: Assertion failed at %s:%u\n", __func__, file, line); \
        error2("[%s%u]%s = %llu " #SYMBOL " %lld = %s[%s%u]\n", \
               type1, bits1, name1, var1, var2, name2, type2, bits2); \
        trap(); \
    } \
}

GENERATE_ASSERT_UNSIGNED_SIGNED(equal,      ==)
GENERATE_ASSERT_UNSIGNED_SIGNED(not_equal,  !=)
GENERATE_ASSERT_UNSIGNED_SIGNED(less,       <)
GENERATE_ASSERT_UNSIGNED_SIGNED(less_equal, <=)
GENERATE_ASSERT_UNSIGNED_SIGNED(more,       >)
GENERATE_ASSERT_UNSIGNED_SIGNED(more_equal, >=)

#undef GENERATE_ASSERT_UNSIGNED_SIGNED

#define GENERATE_ASSERT_LDOUBLE(MODE, SYMBOL) \
static void \
a_ldouble_##MODE(char *file, uint line, \
                 char *name1, char *name2, \
                 char *type1, char *type2, \
                 uint bits1, uint bits2, \
                 ldouble var1, ldouble var2) { \
    if (!(var1 SYMBOL var2)) { \
        error2("\n%s: Assertion failed at %s:%u\n", __func__, file, line); \
        error2("[%s%u]%s = %Lf " #SYMBOL " %Lf = %s[%s%u]\n", \
               type1, bits1, name1, var1, var2, name2, type2, bits2); \
        trap(); \
    } \
}

GENERATE_ASSERT_LDOUBLE(equal,      ==)
GENERATE_ASSERT_LDOUBLE(not_equal,  !=)
GENERATE_ASSERT_LDOUBLE(less,       <)
GENERATE_ASSERT_LDOUBLE(less_equal, <=)
GENERATE_ASSERT_LDOUBLE(more,       >)
GENERATE_ASSERT_LDOUBLE(more_equal, >=)

#undef GENERATE_ASSERT_LDOUBLE

#define A_BOTH_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    a_both_signed_##MODE(__FILE__, __LINE__,               \
                         #VAR1, #VAR2,                     \
                         typename(TYPE1), typename(TYPE2), \
                         TYPEBITS(TYPE1), TYPEBITS(TYPE2), \
                         (llong)(VAR1), (llong)(VAR2))     \

#define A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    a_signed_unsigned##MODE(__FILE__, __LINE__,               \
                            #VAR1, #VAR2,                     \
                            typename(TYPE1), typename(TYPE2), \
                            TYPEBITS(TYPE1), TYPEBITS(TYPE2), \
                            (llong)(VAR1), (ullong)(VAR2))

#define A_FIRST_SIGNED(MODE, VAR1, VAR2, TYPE1) \
_Generic((VAR2), \
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
    ldouble: A_BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_LDOUBLE), \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_SIGNED() \
)
void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_SIGNED(void);

#define A_BOTH_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    a_both_unsigned_##MODE(__FILE__, __LINE__,               \
                           #VAR1, #VAR2,                     \
                           typename(TYPE1), typename(TYPE2), \
                           TYPEBITS(TYPE1), TYPEBITS(TYPE2), \
                           (ullong)(VAR1), (ullong)(VAR2))

#define A_UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    a_unsigned_signed_##MODE(__FILE__, __LINE__,               \
                             #VAR1, #VAR2,                     \
                             typename(TYPE1), typename(TYPE2), \
                             TYPEBITS(TYPE1), TYPEBITS(TYPE2), \
                             (ullong)(VAR1), (llong)(VAR2))

#define A_FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE1) \
_Generic((VAR2), \
    schar:   A_UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_SCHAR  ), \
    short:   A_UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_SHORT  ), \
    int:     A_UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_INT    ), \
    long:    A_UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_LONG   ), \
    llong:   A_UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_LLONG  ), \
    uchar:   A_BOTH_UNSIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_UCHAR  ), \
    ushort:  A_BOTH_UNSIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_USHORT ), \
    uint:    A_BOTH_UNSIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_UINT   ), \
    ulong:   A_BOTH_UNSIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_ULONG  ), \
    ullong:  A_BOTH_UNSIGNED(MODE,   VAR1, VAR2, TYPE1, TYPE_ULLONG ), \
    float:   A_BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_FLOAT  ), \
    double:  A_BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_DOUBLE ), \
    ldouble: A_BOTH_LDOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_LDOUBLE), \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_UNSIGNED() \
)
void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_UNSIGNED(void);

#define A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    a_ldouble_##MODE(__FILE__, __LINE__, \
                     #VAR1, #VAR2, \
                     typename(TYPE1), typename(TYPE2), \
                     TYPEBITS(TYPE1), TYPEBITS(TYPE2), \
                     LDOUBLE_GET2(VAR1, TYPE1), LDOUBLE_GET2(VAR2, TYPE2))

#define A_FIRST_LDOUBLE(MODE, VAR1, VAR2, TYPE1) \
_Generic((VAR2), \
    schar:   A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_SCHAR  ), \
    short:   A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_SHORT  ), \
    int:     A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_INT    ), \
    long:    A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_LONG   ), \
    llong:   A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_LLONG  ), \
    uchar:   A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_UCHAR  ), \
    ushort:  A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_USHORT ), \
    uint:    A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_UINT   ), \
    ulong:   A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_ULONG  ), \
    ullong:  A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_ULLONG ), \
    float:   A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_FLOAT  ), \
    double:  A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_DOUBLE ), \
    ldouble: A_BOTH_LDOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_LDOUBLE), \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_LDOUBLE() \
)
void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_LDOUBLE(void);

#define A_POINTERS(MODE, VAR1, VAR2) \
    a_pointers_##MODE(__FILE__, __LINE__, \
                      #VAR1, #VAR2, \
                      (void *)(uintptr_t)(VAR1), \
                      (void *)(uintptr_t)(VAR2))

void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE_CHARP(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE_VOIDP(void);
#define ASSERT_COMPARE(MODE, VAR1, VAR2) \
_Generic((VAR1), \
    void *: _Generic((VAR2), \
        char *: A_POINTERS(MODE, VAR1, VAR2), \
        void *: A_POINTERS(MODE, VAR1, VAR2), \
        default: UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE_VOIDP() \
    ), \
    char *: _Generic((VAR2), \
        char *: a_strings_##MODE(__FILE__, __LINE__, \
                                 #VAR1, #VAR2, \
                                 (char *)(uintptr_t)(VAR1), \
                                 (char *)(uintptr_t)(VAR2)), \
        default: UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE_CHARP() \
    ), \
    schar:   A_FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_SCHAR  ), \
    short:   A_FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_SHORT  ), \
    int:     A_FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_INT    ), \
    long:    A_FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_LONG   ), \
    llong:   A_FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_LLONG  ), \
    uchar:   A_FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_UCHAR  ), \
    ushort:  A_FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_USHORT ), \
    uint:    A_FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_UINT   ), \
    ulong:   A_FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_ULONG  ), \
    ullong:  A_FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_ULLONG ), \
    float:   A_FIRST_LDOUBLE(MODE,  VAR1, VAR2, TYPE_FLOAT  ), \
    double:  A_FIRST_LDOUBLE(MODE,  VAR1, VAR2, TYPE_DOUBLE ), \
    ldouble: A_FIRST_LDOUBLE(MODE,  VAR1, VAR2, TYPE_LDOUBLE)  \
)

#define ASSERT_EQUAL(VAR1, VAR2)      ASSERT_COMPARE(equal,      VAR1, VAR2)
#define ASSERT_NOT_EQUAL(VAR1, VAR2)  ASSERT_COMPARE(not_equal,  VAR1, VAR2)
#define ASSERT_LESS(VAR1, VAR2)       ASSERT_COMPARE(less,       VAR1, VAR2)
#define ASSERT_LESS_EQUAL(VAR1, VAR2) ASSERT_COMPARE(less_equal, VAR1, VAR2)
#define ASSERT_MORE(VAR1, VAR2)       ASSERT_COMPARE(more,       VAR1, VAR2)
#define ASSERT_MORE_EQUAL(VAR1, VAR2) ASSERT_COMPARE(more_equal, VAR1, VAR2)

// clang-format on

#if TESTING_assert

#include <signal.h>
#include <setjmp.h>

static volatile sig_atomic_t assertion_failed = false;
static sigjmp_buf assert_env;

static void __attribute__((noreturn))
handler_failed_assertion(int unused) {
    (void)unused;
    assertion_failed = true;
    siglongjmp(assert_env, 1);
}

// clang-format off
int
main(void) {
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
        // uncomment to trigger linking error
        /* double x = 0.1; */
        /* void *a = NULL; */
        /* ASSERT_MORE_EQUAL(x, a); */
        /* ASSERT_MORE_EQUAL(a, x); */
    }{
        int a = 0;
        double b = 1;
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
    }
    ASSERT(true);
    exit(EXIT_SUCCESS);
}
// clang-format on
#endif

#endif
