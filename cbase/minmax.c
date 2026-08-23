// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(MINMAX_C)
#define MINMAX_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_minmax 1
#elif !defined(TESTING_minmax)
#define TESTING_minmax 0
#endif

#include "libc.h"
#include "primitives.h"
#include "base_macros.h"
#include "platform_detection.h"

#define GENERATE_COMPARE_POINTERS(MODE, SYMBOL) \
static void *                                   \
get_pointer_##MODE(void *var1, void *var2) {    \
    if ((uintptr)var1 SYMBOL (uintptr)var2) {   \
        return var1;                            \
    } else {                                    \
        return var2;                            \
    }                                           \
}

GENERATE_COMPARE_POINTERS(min, <)
GENERATE_COMPARE_POINTERS(max, >)

#undef GENERATE_COMPARE_POINTERS

#define GENERATE_COMPARE_INTEGERS_SAME_SIGN(TYPE, SYMBOL, MODE)      \
static TYPE long long                                                \
get_both_##TYPE##_##MODE(TYPE long long var1, TYPE long long var2) { \
    if (var1 SYMBOL var2) {                                          \
        return var1;                                                 \
    } else {                                                         \
        return var2;                                                 \
    }                                                                \
}

GENERATE_COMPARE_INTEGERS_SAME_SIGN(signed,   <,  min)
GENERATE_COMPARE_INTEGERS_SAME_SIGN(unsigned, <,  min)
GENERATE_COMPARE_INTEGERS_SAME_SIGN(signed,   >,  max)
GENERATE_COMPARE_INTEGERS_SAME_SIGN(unsigned, >,  max)

#undef GENERATE_COMPARE_INTEGERS_SAME_SIGN

static int
minmax_compare_sign_with_unsign(llong signed_value, ullong unsigned_value) {
    ullong converted;

    if (signed_value < 0) {
        return -1;
    }

    converted = (ullong)signed_value;
    if (converted < unsigned_value) {
        return -1;
    }
    if (converted == unsigned_value) {
        return 0;
    }
    return 1;
}

#define GENERATE_COMPARE_SIGNED_UNSIGNED(MODE, SYMBOL)                         \
static llong                                                                   \
get_signed_unsigned_##MODE(llong var1, ullong var2) {                          \
    if ((minmax_compare_sign_with_unsign(var1, var2) SYMBOL 0)) {              \
        return var1;                                                           \
    } else {                                                                   \
        if (var2 > LLONG_MAX) {                                                \
            fprintf(stderr, "You are working with a too large number.\n");     \
            TRAP();                                                            \
        }                                                                      \
        return (llong)var2;                                                    \
    }                                                                          \
}

GENERATE_COMPARE_SIGNED_UNSIGNED(min, <)
GENERATE_COMPARE_SIGNED_UNSIGNED(max, >)

#undef GENERATE_COMPARE_SIGNED_UNSIGNED

#define GENERATE_COMPARE_UNSIGNED_SIGNED(MODE, SYMBOL)                         \
static llong                                                                   \
get_unsigned_signed_##MODE(ullong var1, llong var2) {                          \
    if (((-minmax_compare_sign_with_unsign(var2, var1)) SYMBOL 0)) {           \
        if (var1 > LLONG_MAX) {                                                \
            fprintf(stderr, "You are working with a too large number.\n");     \
            TRAP();                                                            \
        }                                                                      \
        return (llong)var1;                                                    \
    } else {                                                                   \
        return var2;                                                           \
    }                                                                          \
}

GENERATE_COMPARE_UNSIGNED_SIGNED(min, <)
GENERATE_COMPARE_UNSIGNED_SIGNED(max, >)

#undef GENERATE_COMPARE_UNSIGNED_SIGNED

#define GENERATE_COMPARE_DOUBLE(MODE, SYMBOL)            \
static double                                            \
get_double_##MODE(double var1, double var2) {            \
    if (var1 SYMBOL var2) {                              \
        return var1;                                     \
    } else {                                             \
        return var2;                                     \
    }                                                    \
}

GENERATE_COMPARE_DOUBLE(min, <)
GENERATE_COMPARE_DOUBLE(max, >)

#undef GENERATE_COMPARE_DOUBLE

#if 0 == TESTING_minmax
static inline void
minmax_functions_sink(void) {
    (void)minmax_functions_sink;
    (void)get_pointer_min;
    (void)get_pointer_max;
    (void)get_both_signed_min;
    (void)get_both_signed_max;
    (void)get_both_unsigned_min;
    (void)get_both_unsigned_max;
    (void)get_signed_unsigned_min;
    (void)get_signed_unsigned_max;
    (void)get_unsigned_signed_min;
    (void)get_unsigned_signed_max;
    (void)get_double_min;
    (void)get_double_max;
    return;
}
#endif

void UNSUPPORTED_TYPE_FOR_GENERIC_FIRST_SIGNED(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_FIRST_UNSIGNED(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_FIRST_DOUBLE(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_MINMAX_COMPARE_VOIDP(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_MINMAX_COMPARE(void);

#define BOTH_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    get_both_signed_##MODE((llong)(VAR1), (llong)(VAR2))

#define SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    get_signed_unsigned_##MODE((llong)(VAR1), (ullong)(VAR2))

#define FIRST_SIGNED(MODE, VAR1, VAR2, TYPE1)                        \
_Generic((VAR2),                                                     \
    schar:   BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_SCHAR  ), \
    short:   BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_SHORT  ), \
    int:     BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_INT    ), \
    long:    BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_LONG   ), \
    llong:   BOTH_SIGNED(MODE,     VAR1, VAR2, TYPE1, TYPE_LLONG  ), \
    uchar:   SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_UCHAR  ), \
    ushort:  SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_USHORT ), \
    uint:    SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_UINT   ), \
    ulong:   SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_ULONG  ), \
    ullong:  SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE_ULLONG ), \
    float:   BOTH_DOUBLE(MODE,     VAR1, VAR2, TYPE1, TYPE_FLOAT  ), \
    double:  BOTH_DOUBLE(MODE,     VAR1, VAR2, TYPE1, TYPE_DOUBLE ), \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_FIRST_SIGNED() \
)
#define BOTH_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    get_both_unsigned_##MODE((ullong)(VAR1), (ullong)(VAR2))

#define UNSIGNED_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    get_unsigned_signed_##MODE((ullong)(VAR1), (llong)(VAR2))

#define FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE1)                      \
_Generic((VAR2),                                                     \
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
    float:   BOTH_DOUBLE(MODE,     VAR1, VAR2, TYPE1, TYPE_FLOAT  ), \
    double:  BOTH_DOUBLE(MODE,     VAR1, VAR2, TYPE1, TYPE_DOUBLE ), \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_FIRST_UNSIGNED()           \
)
#define BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE2) \
    get_double_##MODE(DOUBLE_GET2(VAR1, TYPE1), DOUBLE_GET2(VAR2, TYPE2))

#define FIRST_DOUBLE(MODE, VAR1, VAR2, TYPE1)                    \
_Generic((VAR2),                                                 \
    schar:   BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_SCHAR  ), \
    short:   BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_SHORT  ), \
    int:     BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_INT    ), \
    long:    BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_LONG   ), \
    llong:   BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_LLONG  ), \
    uchar:   BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_UCHAR  ), \
    ushort:  BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_USHORT ), \
    uint:    BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_UINT   ), \
    ulong:   BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_ULONG  ), \
    ullong:  BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_ULLONG ), \
    float:   BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_FLOAT  ), \
    double:  BOTH_DOUBLE(MODE, VAR1, VAR2, TYPE1, TYPE_DOUBLE ), \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_FIRST_DOUBLE()         \
)
#define POINTERS(MODE, VAR1, VAR2) \
    get_pointer_##MODE((void *)(uintptr)(VAR1), (void *)(uintptr)(VAR2))

#define MINMAX_COMPARE(MODE, VAR1, VAR2)                             \
_Generic((VAR1),                                                     \
    void *: _Generic((VAR2),                                         \
        char *: POINTERS(MODE, VAR1, VAR2),                          \
        void *: POINTERS(MODE, VAR1, VAR2),                          \
        default: UNSUPPORTED_TYPE_FOR_GENERIC_MINMAX_COMPARE_VOIDP() \
    ),                                                               \
    schar:   FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_SCHAR  ),         \
    short:   FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_SHORT  ),         \
    int:     FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_INT    ),         \
    long:    FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_LONG   ),         \
    llong:   FIRST_SIGNED(MODE,   VAR1, VAR2, TYPE_LLONG  ),         \
    uchar:   FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_UCHAR  ),         \
    ushort:  FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_USHORT ),         \
    uint:    FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_UINT   ),         \
    ulong:   FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_ULONG  ),         \
    ullong:  FIRST_UNSIGNED(MODE, VAR1, VAR2, TYPE_ULLONG ),         \
    float:   FIRST_DOUBLE(MODE,   VAR1, VAR2, TYPE_FLOAT  ),         \
    double:  FIRST_DOUBLE(MODE,   VAR1, VAR2, TYPE_DOUBLE ),         \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_MINMAX_COMPARE()           \
)

#if defined(MIN)
#undef MIN
#endif
#if defined(MAX)
#undef MAX
#endif

#define MINMAX_COMPARE_SAME_TYPE(SYMBOL, VAR1, VAR2) \
    ((VAR1) SYMBOL (VAR2) ? (VAR1) : (VAR2))

#define MINMAX_COMPARE_SAME_TYPE_min(VAR1, VAR2) \
    MINMAX_COMPARE_SAME_TYPE(<, VAR1, VAR2)
#define MINMAX_COMPARE_SAME_TYPE_max(VAR1, VAR2) \
    MINMAX_COMPARE_SAME_TYPE(>, VAR1, VAR2)

#if CC_GCC || CC_CLANG

#define MINMAX_COMPARE_TYPE_PRESERVING(VAR) \
_Generic((VAR),                             \
    schar:  1,                              \
    short:  1,                              \
    int:    1,                              \
    long:   1,                              \
    llong:  1,                              \
    uchar:  1,                              \
    ushort: 1,                              \
    uint:   1,                              \
    ulong:  1,                              \
    ullong: 1,                              \
    float:  1,                              \
    double: 1,                              \
    default: 0                              \
)

#define MINMAX_COMPARE_DIAGNOSTIC_(RESULT, MODE, VAR1, VAR2) __extension__ ({ \
    _Pragma("GCC diagnostic push")                                            \
    _Pragma("GCC diagnostic ignored \"-Wpedantic\"")                          \
    _Pragma("GCC diagnostic ignored \"-Wsign-compare\"")                      \
    _Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")                   \
    __auto_type RESULT = __builtin_choose_expr(                               \
        MINMAX_COMPARE_TYPE_PRESERVING(VAR1)                                  \
        && __builtin_types_compatible_p(__typeof__(VAR1), __typeof__(VAR2)),  \
        CAT(MINMAX_COMPARE_SAME_TYPE_, MODE)(VAR1, VAR2),                     \
        MINMAX_COMPARE(MODE, VAR1, VAR2)                                      \
    );                                                                        \
    _Pragma("GCC diagnostic pop")                                             \
    RESULT;                                                                   \
})

#define MINMAX_COMPARE_DIAGNOSTIC(MODE, VAR1, VAR2) \
    MINMAX_COMPARE_DIAGNOSTIC_(CAT(minmax_result_, __LINE__), \
                               MODE, VAR1, VAR2)

#define MIN(VAR1, VAR2) MINMAX_COMPARE_DIAGNOSTIC(min, VAR1, VAR2)
#define MAX(VAR1, VAR2) MINMAX_COMPARE_DIAGNOSTIC(max, VAR1, VAR2)
#elif CC_MSVC
#define MINMAX_COMPARE_MSVC_TYPE(MODE, VAR1, VAR2, TYPE) \
_Generic((VAR2),                                         \
    TYPE: CAT(MINMAX_COMPARE_SAME_TYPE_, MODE)(VAR1, VAR2), \
    default: MINMAX_COMPARE(MODE, VAR1, VAR2)             \
)

#define MINMAX_COMPARE_MSVC(MODE, VAR1, VAR2)             \
_Generic((VAR1),                                         \
    int:    MINMAX_COMPARE_MSVC_TYPE(MODE, VAR1, VAR2, int), \
    llong:   MINMAX_COMPARE_MSVC_TYPE(MODE, VAR1, VAR2, llong), \
    default: MINMAX_COMPARE(MODE, VAR1, VAR2)              \
)

#define MIN(VAR1, VAR2) MINMAX_COMPARE_MSVC(min, VAR1, VAR2)
#define MAX(VAR1, VAR2) MINMAX_COMPARE_MSVC(max, VAR1, VAR2)
#else
#define MIN(VAR1, VAR2) MINMAX_COMPARE(min, VAR1, VAR2)
#define MAX(VAR1, VAR2) MINMAX_COMPARE(max, VAR1, VAR2)
#endif

#if TESTING_minmax
#define CBASE_IMPLEMENT
#include "cbase.h"

int
main(void) {
    {
        double x = 0.0;
        double y = 1.0;
        double min = MIN(x, y);
        double max = MAX(x, y);
        ASSERT_EQUAL(min, x);
        ASSERT_EQUAL(max, y);
    }
    {
        long min01 = MIN(0, 1);
        long min11 = MIN(1, 1);
        long max11 = MAX(1, 1);
        long max01 = MAX(0, 1);

        ASSERT_ZERO(min01);
        ASSERT_EQUAL(min11, 1);
        ASSERT_EQUAL(max11, 1);
        ASSERT_EQUAL(max01, 1);
    } {
        int a = 1;
        int b = 1;
        long min = MIN(a, b);
        long max = MAX(a, b);
        ASSERT_EQUAL(min, a);
        ASSERT_EQUAL(max, a);
    } {
        int a = 1;
        uint b = 2;
        long min = MIN(a, b);
        long max = MAX(a, b);
        ASSERT_EQUAL(min, a);
        ASSERT_EQUAL(max, b);
    } {
        long a = -1;
        ulong b = 0;
        double min = (double)MIN(a, b);
        double max = (double)MAX(a, b);
        ASSERT_EQUAL(min, a);
        ASSERT_EQUAL(max, b);
    } {
        long a = MINOF(a);
        ulong b = MAXOF(a);
        double min = (double)MIN(a, b);
        ullong max = (ullong)MAX(a, b);
        ASSERT_EQUAL((long)min, a);
        ASSERT_EQUAL(max, b);
    } {
        ulong a = MINOF(a);
        long b = MAXOF(b);
        long min = MIN(a, b);
        long max = MAX(a, b);
        ASSERT_EQUAL(min, a);
        ASSERT_EQUAL(max, b);
    } {
        long a = -1;
        long min = MIN(a, 0);
        long max = MAX(a, 0);
        ASSERT_EQUAL(min, -1);
        ASSERT_ZERO(max);
    } {
        double a = 0.123;
        double min = MIN(a, 0);
        double max = MAX(a, 0);
        ASSERT_EQUAL(min, 0.0);
        ASSERT_EQUAL(max, a);
    } {
        int array[100];
        void *a = &array[0];
        void *b = &array[1];
        void *min = MIN(a, b);
        void *max = MAX(a, b);
        ASSERT_EQUAL(min, a);
        ASSERT_EQUAL(max, b);
    }

    // Testing if conversion warnings are correctly given
    // when using MIN with integers of different signedness.
    // This block only is compiled from the self-compiling block below,
    // so that normal compilation (TESTING=1) does not give warnings
    if (!TESTING) {
        int32 x = 0;
        uint32 y = 1;
        int32 a = MIN(x, y);  // NOLINT
        (void)a;
    }
    if (OS_LINUX) {
        Command command = {0};

        COMMAND_PUSH(&command, "gcc", "-std=c11");
        COMMAND_PUSH(&command, "-Wconversion");
        COMMAND_PUSH(&command, "-o", "/tmp/a.out");
        command_printf(&command, "%s", __FILE__);

        if (!command_run(&command,
                         COMMAND_CAPTURE_STDOUT|COMMAND_CAPTURE_STDERR)) {
            exit(EXIT_SUCCESS);
        } else {
            ASSERT_GLOB_MATCH(command.result.stderr_output,
                              command.result.stderr_len,
                              "*minmax.c*warning:*conversion*");
        }

        command_argv0_set(&command, "clang");
        COMMAND_PUSH(&command, "-Wshorten-64-to-32");

        if (!command_run(&command,
                         COMMAND_CAPTURE_STDOUT|COMMAND_CAPTURE_STDERR)) {
            exit(EXIT_SUCCESS);
        } else {
            ASSERT_GLOB_MATCH(command.result.stderr_output,
                              command.result.stderr_len,
                              "*minmax.c*warning:*conversion*");
        }
    }
    exit(EXIT_SUCCESS);
}
#endif

#endif /* MINMAX_C */
