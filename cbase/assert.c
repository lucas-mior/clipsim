// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(ASSERT_C)
#define ASSERT_C

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
               "assert.c ULP comparison requires 32-bit float");
_Static_assert(sizeof(double)*CHAR_BIT == 64,
               "assert.c ULP comparison requires 64-bit double");

static void __attribute__((format(printf, 4, 5)))
assert_error(char *file, int32 line, char *func, char *format, ...) {
    va_list ap;

    fprintf(stderr, "%s:%d:%s: ", file, line, func);

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);

    return;
}

static void __attribute__((noreturn))
assert_fatal(void) {
    if (DEBUGGING) {
        TRAP();
    }
    exit(EXIT_FAILURE);
}

static int32
assert_strlen32(char *string) {
    size_t length = strlen(string);

    if (length > INT32_MAX) {
        fprintf(stderr, "Assertion string is too long.\n");
        assert_fatal();
    }
    return (int32)length;
}

static void *
assert_memmem(char *haystack, int32 haystack_len,
              char *needle, int32 needle_len) {
    int32 i;

    if ((haystack_len <= 0) || (needle_len <= 0)
        || (needle_len > haystack_len)) {
        return NULL;
    }

    for (i = 0; i <= haystack_len - needle_len; i += 1) {
        if (!memcmp(haystack + i, needle, (size_t)needle_len)) {
            return haystack + i;
        }
    }
    return NULL;
}

static void
assert_file_contains(char *file, int32 line, char *func,
                     char *path, char *needle) {
    FILE *file_handle;
    char buffer[4096];
    bool found = false;
    int32 needle_len = strlen32(needle);

    if ((file_handle = fopen(path, "r")) == NULL) {
        assert_error(file, line, func,
                     "Error opening %s for reading: %s.\n",
                     path, strerror(errno));
        assert_fatal();
    }
    while (fgets(buffer, SIZEOF(buffer), file_handle)) {
        int32 n = strlen32(buffer);
        if (memmem64(buffer, n, needle, needle_len)) {
            found = true;
            break;
        }
    }
    if (fclose(file_handle)) {
        assert_error(file, line, func,
                     "Error closing %s: %s.\n", path, strerror(errno));
    }
    if (!found) {
        assert_error(file, line, func,
                     "Needle '%s' not found in '%s'.\n", needle, path);
        assert_fatal();
    }
    return;
}

static void
assert_contains(char *file, int32 line, char *func,
                char *haystack, int32 haystack_len, char *needle) {
    int32 needle_len = assert_strlen32(needle);
    if (assert_memmem(haystack, haystack_len, needle, needle_len) == NULL) {
        assert_error(file, line, func,
                     "expected to find substring:\n%.*s\n--- in ---\n%.*s",
                     needle_len, needle, haystack_len, haystack);
        assert_fatal();
    }
}

static void
assert_not_contains(char *file, int32 line, char *func,
                    char *haystack, int32 haystack_len, char *needle) {
    int32 needle_len = assert_strlen32(needle);
    if (assert_memmem(haystack, haystack_len, needle, needle_len)) {
        assert_error(file, line, func,
                     "expected to not find substring:\n%.*s\n--- in ---\n%.*s",
                     needle_len, needle, haystack_len, haystack);
        assert_fatal();
    }
}

#define GENERATE_ASSERT_STRINGS(MODE, SYMBOL)                                  \
static void                                                                    \
a_strings_##MODE(char *file, int32 line, char *func,                           \
                 char *name1, char *name2,                                     \
                 char *var1, char *var2) {                                     \
    if (var1 == NULL) {                                                        \
        fprintf(stderr,                                                        \
                "\nError in assertion at %s:%d:%s\n", file, line, func);       \
        fprintf(stderr, "%s is NULL.\n", name1);                               \
        TRAP();                                                                \
    }                                                                          \
    if (var2 == NULL) {                                                        \
        fprintf(stderr,                                                        \
                "\nError in assertion at %s:%d:%s\n", file, line, func);       \
        fprintf(stderr, "%s is NULL.\n", name2);                               \
        TRAP();                                                                \
    }                                                                          \
    if (!(strcmp(var1, var2) SYMBOL 0)) {                                      \
        fprintf(stderr,                                                        \
                "\nError in assertion at %s:%d:%s\n", file, line, func);       \
        fprintf(stderr,                                                        \
                "%s = %s " #SYMBOL " %s = %s\n", name1, var1, var2, name2);    \
        TRAP();                                                                \
    }                                                                          \
    return;                                                                    \
}

GENERATE_ASSERT_STRINGS(less, <)
GENERATE_ASSERT_STRINGS(less_equal, <=)
GENERATE_ASSERT_STRINGS(equal, ==)
GENERATE_ASSERT_STRINGS(not_equal, !=)
GENERATE_ASSERT_STRINGS(more, >)
GENERATE_ASSERT_STRINGS(more_equal, >=)

#undef GENERATE_ASSERT_STRINGS

#define GENERATE_ASSERT_POINTERS(MODE, SYMBOL)                                 \
static void                                                                    \
a_pointers_##MODE(char *file, int32 line, char *func,                          \
                  char *name1, char *name2,                                    \
                  void *var1, void *var2) {                                    \
    if (!((uintptr)var1 SYMBOL (uintptr)var2)) {                               \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
        fprintf(stderr,                                                        \
                "\nAssertion failed at %s:%d:%s\n", file, line, func);         \
        fprintf(stderr,                                                        \
                "%s = %p " #SYMBOL " %p = %s\n", name1, var1, var2, name2);    \
        TRAP();                                                                \
    }                                                                          \
    return;                                                                    \
}

GENERATE_ASSERT_POINTERS(less, <)
GENERATE_ASSERT_POINTERS(less_equal, <=)
GENERATE_ASSERT_POINTERS(equal, ==)
GENERATE_ASSERT_POINTERS(not_equal, !=)
GENERATE_ASSERT_POINTERS(more, >)
GENERATE_ASSERT_POINTERS(more_equal, >=)

#undef GENERATE_ASSERT_POINTERS

#define GENERATE_ASSERT_INTEGERS_SAME_SIGN(TYPE, FORMAT, SYMBOL, MODE)         \
static void                                                                    \
a_both_##TYPE##_##MODE(char *file, int32 line, char *func,                     \
                       char *name1, char *name2,                               \
                       char *type1, char *type2,                               \
                       llong bits1, llong bits2,                               \
                       TYPE long long var1, TYPE long long var2) {             \
    if (!(var1 SYMBOL var2)) {                                                 \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
        fprintf(stderr,                                                        \
                "\nAssertion failed at %s:%d:%s\n", file, line, func);         \
        fprintf(stderr,                                                        \
                "[%s%lld]%s = "FORMAT" " #SYMBOL " "FORMAT" = %s[%s%lld]\n",   \
               type1, bits1, name1, var1, var2, name2, type2, bits2);          \
        TRAP();                                                                \
    }                                                                          \
    return;                                                                    \
}

GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed, "%lld", ==, equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llx", ==, equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed, "%lld", !=, not_equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llx", !=, not_equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed, "%lld", <, less)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llx", <, less)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed, "%lld", <=, less_equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llx", <=, less_equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed, "%lld", >, more)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llx", >, more)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed, "%lld", >=, more_equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llx", >=, more_equal)

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

#define GENERATE_ASSERT_SIGNED_UNSIGNED(MODE, SYMBOL)                          \
static void                                                                    \
a_signed_unsigned##MODE(char *file, int32 line, char *func,                    \
                        char *name1, char *name2,                              \
                        char *type1, char *type2,                              \
                        llong bits1, llong bits2,                              \
                        llong var1, ullong var2) {                             \
    if (!(compare_sign_with_unsign(var1, var2) SYMBOL 0)) {                    \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
        fprintf(stderr,                                                        \
                "\nAssertion failed at %s:%d:%s\n", file, line, func);         \
        fprintf(stderr,                                                        \
                "[%s%lld]%s = %lld " #SYMBOL " %llu = %s[%s%lld]\n",           \
                type1, bits1, name1, var1, var2, name2, type2, bits2);         \
        TRAP();                                                                \
    }                                                                          \
    return;                                                                    \
}

GENERATE_ASSERT_SIGNED_UNSIGNED(equal, ==)
GENERATE_ASSERT_SIGNED_UNSIGNED(not_equal, !=)
GENERATE_ASSERT_SIGNED_UNSIGNED(less, <)
GENERATE_ASSERT_SIGNED_UNSIGNED(less_equal, <=)
GENERATE_ASSERT_SIGNED_UNSIGNED(more, >)
GENERATE_ASSERT_SIGNED_UNSIGNED(more_equal, >=)

#undef GENERATE_ASSERT_SIGNED_UNSIGNED

#define GENERATE_ASSERT_UNSIGNED_SIGNED(MODE, SYMBOL)                          \
static void                                                                    \
a_unsigned_signed_##MODE(char *file, int32 line, char *func,                   \
                         char *name1, char *name2,                             \
                         char *type1, char *type2,                             \
                         llong bits1, llong bits2,                             \
                         ullong var1, llong var2) {                            \
    if (!((-compare_sign_with_unsign(var2, var1)) SYMBOL 0)) {                 \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
        fprintf(stderr,                                                        \
                "\nAssertion failed at %s:%d:%s\n", file, line, func);         \
        fprintf(stderr,                                                        \
                "[%s%lld]%s = %llu " #SYMBOL " %lld = %s[%s%lld]\n",           \
                type1, bits1, name1, var1, var2, name2, type2, bits2);         \
        TRAP();                                                                \
    }                                                                          \
    return;                                                                    \
}

GENERATE_ASSERT_UNSIGNED_SIGNED(equal, ==)
GENERATE_ASSERT_UNSIGNED_SIGNED(not_equal, !=)
GENERATE_ASSERT_UNSIGNED_SIGNED(less, <)
GENERATE_ASSERT_UNSIGNED_SIGNED(less_equal, <=)
GENERATE_ASSERT_UNSIGNED_SIGNED(more, >)
GENERATE_ASSERT_UNSIGNED_SIGNED(more_equal, >=)

#undef GENERATE_ASSERT_UNSIGNED_SIGNED

#define GENERATE_ASSERT_DOUBLE(SYMBOL, MODE)                                   \
static void                                                                    \
a_double_##MODE(char *file, int32 line, char *func,                            \
                char *name1, char *name2,                                      \
                char *type1, char *type2,                                      \
                llong bits1, llong bits2,                                      \
                double var1, double var2) {                                    \
    if (!(var1 SYMBOL var2)) {                                                 \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
        fprintf(stderr,                                                        \
                "\nAssertion failed at %s:%d:%s\n", file, line, func);         \
        fprintf(stderr,                                                        \
                "[%s%lld]%s = %f " #SYMBOL " %f = %s[%s%lld]\n",               \
                type1, bits1, name1, var1, var2, name2, type2, bits2);         \
        TRAP();                                                                \
    }                                                                          \
    return;                                                                    \
}

GENERATE_ASSERT_DOUBLE(==, equal)
GENERATE_ASSERT_DOUBLE(!=, not_equal)
GENERATE_ASSERT_DOUBLE(<,  less)
GENERATE_ASSERT_DOUBLE(>,  more)
GENERATE_ASSERT_DOUBLE(<=, less_equal)
GENERATE_ASSERT_DOUBLE(>=, more_equal)

#undef GENERATE_ASSERT_DOUBLE

static double
assert_double_abs(double x) {
    if (x < (double)0) {
        return -x;
    }
    return x;
}

static bool
assert_double_is_nan(double x) {
    return x != x;
}

static bool
assert_double_is_infinite(double x) {
    return (x > (double)DBL_MAX) || (x < -(double)DBL_MAX);
}

static bool
assert_double_is_negative(double x) {
    return signbit(x) != 0;
}

static bool
assert_double_is_zero(double x) {
    return x == (double)0;
}

static int
assert_fp_common_kind(int kind1, int kind2) {
    if ((kind1 == ASSERT_FP_KIND_FLOAT) || (kind2 == ASSERT_FP_KIND_FLOAT)) {
        return ASSERT_FP_KIND_FLOAT;
    }
    if ((kind1 == ASSERT_FP_KIND_DOUBLE) || (kind2 == ASSERT_FP_KIND_DOUBLE)) {
        return ASSERT_FP_KIND_DOUBLE;
    }
    return ASSERT_FP_KIND_DOUBLE;
}

static ullong
assert_fp_max_ulps(int common_kind) {
    if (common_kind == ASSERT_FP_KIND_FLOAT) {
        return (ullong)ASSERT_FLOAT_MAX_ULPS;
    }
    if (common_kind == ASSERT_FP_KIND_DOUBLE) {
        return (ullong)ASSERT_DOUBLE_MAX_ULPS;
    }
    return (ullong)ASSERT_DOUBLE_MAX_ULPS;
}

static uint32
assert_float_ordered_bits(float var) {
    union {
        float as_float;
        uint32 as_uint;
    } bits;
    uint32 sign_mask = 0x80000000u;

    bits.as_float = var;
    if (bits.as_uint & sign_mask) {
        return ~bits.as_uint;
    }
    return bits.as_uint|sign_mask;
}

static uint64
assert_double_ordered_bits(double var) {
    union {
        double as_double;
        uint64 as_uint;
    } bits;
    uint64 sign_mask = 0x8000000000000000ull;

    bits.as_double = var;
    if (bits.as_uint & sign_mask) {
        return ~bits.as_uint;
    }
    return bits.as_uint|sign_mask;
}

static ullong
assert_uint32_distance(uint32 a, uint32 b) {
    if (a > b) {
        return (ullong)(a - b);
    }
    return (ullong)(b - a);
}

static ullong
assert_uint64_distance(uint64 a, uint64 b) {
    if (a > b) {
        return (ullong)(a - b);
    }
    return (ullong)(b - a);
}

static ullong
assert_float_ulp_distance(float var1, float var2) {
    uint32 bits1 = assert_float_ordered_bits(var1);
    uint32 bits2 = assert_float_ordered_bits(var2);

    return assert_uint32_distance(bits1, bits2);
}

static ullong
assert_double_ulp_distance(double var1, double var2) {
    uint64 bits1 = assert_double_ordered_bits(var1);
    uint64 bits2 = assert_double_ordered_bits(var2);

    return assert_uint64_distance(bits1, bits2);
}

static bool
assert_double_special_close(double var1, double var2,
                            double *diff_out, bool *handled_out) {
    bool sign1;
    bool sign2;

    if (diff_out != NULL) {
        *diff_out = (double)0;
    }
    *handled_out = true;

    if (assert_double_is_infinite(var1) || assert_double_is_infinite(var2)) {
        if (var1 == var2) {
            return true;
        }
        if (diff_out != NULL) {
            *diff_out = assert_double_abs(var1 - var2);
        }
        return false;
    }

    if (assert_double_is_nan(var1) || assert_double_is_nan(var2)) {
        if (diff_out != NULL) {
            *diff_out = var1 - var2;
        }
        return false;
    }

    sign1 = assert_double_is_negative(var1);
    sign2 = assert_double_is_negative(var2);
    if (sign1 != sign2) {
        if (assert_double_is_zero(var1) && assert_double_is_zero(var2)) {
            return true;
        }
        if (diff_out != NULL) {
            *diff_out = assert_double_abs(var1 - var2);
        }
        return false;
    }

    if (var1 == var2) {
        return true;
    }

    *handled_out = false;
    return false;
}

static bool
assert_double_close_ulps(double var1, double var2,
                         int kind1, int kind2,
                         double *diff_out,
                         ullong *ulps_out,
                         ullong *max_ulps_out) {
    int common_kind;
    bool handled;
    double diff;
    ullong ulps;
    ullong max_ulps;

    common_kind = assert_fp_common_kind(kind1, kind2);
    max_ulps = assert_fp_max_ulps(common_kind);

    if (ulps_out != NULL) {
        *ulps_out = 0;
    }
    if (max_ulps_out != NULL) {
        *max_ulps_out = max_ulps;
    }

    if (assert_double_special_close(var1, var2, diff_out, &handled)) {
        return true;
    }
    if (handled) {
        return false;
    }

    diff = assert_double_abs(var1 - var2);
    if (diff_out != NULL) {
        *diff_out = diff;
    }

    if (common_kind == ASSERT_FP_KIND_FLOAT) {
        float float1;
        float float2;

        if ((assert_double_abs(var1) > (double)FLT_MAX)
            || (assert_double_abs(var2) > (double)FLT_MAX)) {
            if (ulps_out != NULL) {
                *ulps_out = ULLONG_MAX;
            }
            return false;
        }

        float1 = (float)var1;
        float2 = (float)var2;
        ulps = assert_float_ulp_distance(float1, float2);
    } else {
        ulps = assert_double_ulp_distance(var1, var2);
    }

    if (ulps_out != NULL) {
        *ulps_out = ulps;
    }

    return ulps <= max_ulps;
}

static bool
assert_double_close_tolerance(double var1, double var2,
                              double tolerance,
                              double *diff_out,
                              double *tolerance_out) {
    bool handled;
    double diff;

    if (tolerance < 0.0) {
        tolerance = -tolerance;
    }

    if (tolerance_out != NULL) {
        *tolerance_out = tolerance;
    }

    if (assert_double_special_close(var1, var2, diff_out, &handled)) {
        return true;
    }
    if (handled) {
        return false;
    }

    diff = assert_double_abs(var1 - var2);
    if (diff_out != NULL) {
        *diff_out = diff;
    }

    return diff <= tolerance;
}

static void __attribute((noreturn))
assert_double_failure(char *file, int32 line, char *func,
                      char *name1, char *name2,
                      char *type1, char *type2,
                      llong bits1, llong bits2,
                      double var1, double var2, char *symbol,
                      double diff, double tolerance,
                      ullong ulps, ullong max_ulps,
                      bool use_tolerance) {
    if (!DEBUGGING) {
        UNREACHABLE();
    }
    fprintf(stderr,
            "\nAssertion failed at %s:%d:%s\n", file, line, func);
    fprintf(stderr,
            "[%s%lld]%s = %.17g %s %.17g = %s[%s%lld]\n",
            type1, bits1, name1, var1, symbol, var2, name2, type2, bits2);
    if (use_tolerance) {
        fprintf(stderr,
                "floating diff = %.17g, tolerance = %.17g\n",
                diff, tolerance);
    } else {
        fprintf(stderr,
                "floating diff = %.17g, ulps = %llu, max_ulps = %llu\n",
                diff, ulps, max_ulps);
    }
    TRAP();
}

#define GENERATE_A_DOUBLE_CLOSE(MODE, SYMBOL, EXPECT_CLOSE)                    \
static void                                                                    \
a_double_##MODE(char *file, int32 line, char *func,                            \
                char *name1, char *name2,                                      \
                char *type1, char *type2,                                      \
                llong bits1, llong bits2,                                      \
                int kind1, int kind2,                                          \
                double var1, double var2) {                                    \
    double diff;                                                               \
    ullong ulps;                                                               \
    ullong max_ulps;                                                           \
                                                                               \
    if (assert_double_close_ulps(var1, var2, kind1, kind2,                     \
                                 &diff, &ulps, &max_ulps) != EXPECT_CLOSE) {    \
        assert_double_failure(file, line, func, name1, name2,                  \
                              type1, type2, bits1, bits2,                     \
                              var1, var2, SYMBOL, diff, (double)0,             \
                              ulps, max_ulps, false);                          \
    }                                                                          \
    return;                                                                    \
}

GENERATE_A_DOUBLE_CLOSE(close, "~=", true)
GENERATE_A_DOUBLE_CLOSE(not_close, "!~=", false)

#undef GENERATE_A_DOUBLE_CLOSE

#define GENERATE_A_DOUBLE_CLOSE_TOLERANCE(MODE, SYMBOL, EXPECT_CLOSE)          \
static void                                                                    \
a_double_##MODE(char *file, int32 line, char *func,                            \
                char *name1, char *name2,                                      \
                char *type1, char *type2,                                      \
                llong bits1, llong bits2,                                      \
                double var1, double var2,                                      \
                double tolerance) {                                            \
    double diff;                                                               \
    double tolerance_abs;                                                       \
                                                                               \
    if (assert_double_close_tolerance(var1, var2, tolerance,                   \
                                      &diff, &tolerance_abs) != EXPECT_CLOSE) { \
        assert_double_failure(file, line, func, name1, name2,                  \
                              type1, type2, bits1, bits2,                     \
                              var1, var2, SYMBOL, diff, tolerance_abs,         \
                              0, 0, true);                                     \
    }                                                                          \
    return;                                                                    \
}

GENERATE_A_DOUBLE_CLOSE_TOLERANCE(close_tolerance, "~=", true)
GENERATE_A_DOUBLE_CLOSE_TOLERANCE(not_close_tolerance, "!~=", false)

#undef GENERATE_A_DOUBLE_CLOSE_TOLERANCE

#define GENERATE_ASSERT_BOOLS(MODE, SYMBOL)                                    \
static void                                                                    \
a_bool_##MODE(char *file, int32 line, char *func,                              \
              char *name1, char *name2,                                        \
              char *type1, char *type2,                                        \
              llong bits1, llong bits2,                                        \
              bool var1, bool var2) {                                          \
    if (!(var1 SYMBOL var2)) {                                                 \
        char *s1 = "false";                                                    \
        char *s2 = "false";                                                    \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
        if (var1) {                                                            \
            s1 = "true";                                                       \
        }                                                                      \
        if (var2) {                                                            \
            s2 = "true";                                                       \
        }                                                                      \
        fprintf(stderr,                                                        \
                "\nAssertion failed at %s:%d:%s\n", file, line, func);         \
        fprintf(stderr, "[%s%lld]%s = %s " #SYMBOL " %s = %s[%s%lld]\n",       \
                        type1, bits1, name1, s1, s2, name2, type2, bits2);     \
        TRAP();                                                                \
    }                                                                          \
    return;                                                                    \
}

GENERATE_ASSERT_BOOLS(equal, ==)
GENERATE_ASSERT_BOOLS(not_equal, !=)

static void __attribute((noreturn))
a_bool_more(void *p, ...) {
    (void)p;
    TRAP();
}
static void __attribute((noreturn))
a_bool_less(void *p, ...) {
    (void)p;
    TRAP();
}
static void __attribute((noreturn))
a_bool_more_equal(void *p, ...) {
    (void)p;
    TRAP();
}
static void __attribute((noreturn))
a_bool_less_equal(void *p, ...) {
    (void)p;
    TRAP();
}

#undef GENERATE_ASSERT_BOOLS

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

    (void)a_double_close;
    (void)a_double_not_close;
    (void)a_double_less;
    (void)a_double_less_equal;
    (void)a_double_equal;
    (void)a_double_not_equal;
    (void)a_double_more;
    (void)a_double_more_equal;
    (void)a_double_close_tolerance;
    (void)a_double_not_close_tolerance;
    (void)assert_file_contains;
    (void)assert_contains;
    (void)assert_not_contains;

    (void)a_bool_equal;
    (void)a_bool_not_equal;
    (void)a_bool_less_equal;
    (void)a_bool_less;
    (void)a_bool_more_equal;
    (void)a_bool_more;
    return;
}
#endif

void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_SIGNED(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_UNSIGNED(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_DOUBLE(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_BOOL(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE_CHARP(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE_VOIDP(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_COMPARE(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_CLOSE_FIRST(void);
void UNSUPPORTED_TYPE_FOR_GENERIC_ASSERT_CLOSE_SECOND(void);

#define ASSERT(C) do {                                                         \
    if (!(C)) {                                                                \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        } else {                                                               \
            fprintf(stderr, "Assertion '%s' failed at %s:%d:%s\n",             \
                            #C, __FILE__, __LINE__, FUNC__);                   \
            TRAP();                                                            \
        }                                                                      \
    }                                                                          \
} while (0)

#define ASSERT_FILE_CONTAINS(PATH, NEEDLE) \
    assert_file_contains(__FILE__, __LINE__, FUNC__, \
                         PATH, NEEDLE)

#define ASSERT_CONTAINS(HAYSTACK, HAYSTACK_LEN, NEEDLE) \
    assert_contains(__FILE__, __LINE__, FUNC__, \
                    HAYSTACK, HAYSTACK_LEN, NEEDLE)

#define ASSERT_NOT_CONTAINS(HAYSTACK, HAYSTACK_LEN, NEEDLE) \
    assert_not_contains(__FILE__, __LINE__, FUNC__, \
                        HAYSTACK, HAYSTACK_LEN, NEEDLE)

#define A_BOTH_SIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2)             \
    a_both_signed_##MODE(__FILE__, __LINE__, FUNC__,    \
                         #VAR1, #VAR2,                            \
                         typename(TYPE1), typename(TYPE2),        \
                         typebits(TYPE1), typebits(TYPE2),        \
                         (llong)(VAR1), (llong)(VAR2))

#define A_SIGNED_UNSIGNED(MODE, VAR1, VAR2, TYPE1, TYPE2)         \
    a_signed_unsigned##MODE(__FILE__, __LINE__, FUNC__, \
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

#define A_FIRST_SIGNED(MODE, VAR1, VAR2, TYPE1)                        \
_Generic((VAR2),                                                       \
    char:    A_CHAR_FOR_SIGNED(MODE, VAR1, VAR2, TYPE1),               \
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
    float:   A_BOTH_DOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_FLOAT  ),  \
    double:  A_BOTH_DOUBLE(MODE,    VAR1, VAR2, TYPE1, TYPE_DOUBLE ),  \
    default: UNSUPPORTED_TYPE_FOR_GENERIC_A_FIRST_SIGNED()             \
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
    ASSERT_DOUBLE_CLOSE_TOL_DIAGNOSTIC(close_tolerance, VAR1, VAR2, TOL)

#define ASSERT_NOT_CLOSE_2(VAR1, VAR2) \
    ASSERT_DOUBLE_CLOSE_ULPS_DIAGNOSTIC(not_close, VAR1, VAR2)

#define ASSERT_NOT_CLOSE_3(VAR1, VAR2, TOL) \
    ASSERT_DOUBLE_CLOSE_TOL_DIAGNOSTIC(not_close_tolerance, VAR1, VAR2, TOL)
#else
#define ASSERT_CLOSE_2(VAR1, VAR2) \
    ASSERT_DOUBLE_CLOSE_ULPS(close, VAR1, VAR2)

#define ASSERT_CLOSE_3(VAR1, VAR2, TOL) \
    ASSERT_DOUBLE_CLOSE_TOL(close_tolerance, VAR1, VAR2, TOL)

#define ASSERT_NOT_CLOSE_2(VAR1, VAR2) \
    ASSERT_DOUBLE_CLOSE_ULPS(not_close, VAR1, VAR2)

#define ASSERT_NOT_CLOSE_3(VAR1, VAR2, TOL) \
    ASSERT_DOUBLE_CLOSE_TOL(not_close_tolerance, VAR1, VAR2, TOL)
#endif

#define ASSERT_CLOSE(...) SELECT_ON_NUM_ARGS(ASSERT_CLOSE_, __VA_ARGS__)
#define ASSERT_NOT_CLOSE(...) SELECT_ON_NUM_ARGS(ASSERT_NOT_CLOSE_, __VA_ARGS__)

#define ASSERT_NULL(VAR1) do {                                                 \
    void *p = VAR1;                                                            \
    if (p != NULL) {                                                           \
        fprintf(stderr,                                                        \
                "\nAssertion failed at %s:%d:%s\n",                            \
                __FILE__, __LINE__, FUNC__);                                   \
        fprintf(stderr, "%s = %p == NULL\n", #VAR1, p);                        \
        TRAP();                                                                \
    }                                                                          \
} while (0)

#if TESTING_assert
#define CBASE_IMPLEMENT
#include "cbase.h"

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
    } {
        int a = 1;
        uint b = 1;
        ASSERT_EQUAL(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE_EQUAL(a, b);
    } {
        int a = 1;
        uint b = 2;
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    } {
        long a = -1;
        ulong b = 0;
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    } {
        long a = MINOF(a);
        ulong b = MAXOF(b);
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    } {
        ulong a = MINOF(a);
        long b = MAXOF(b);
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    } {
        char *a = "aaa";
        char *b = "aaa";
        ASSERT_EQUAL(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE_EQUAL(b, a);
    } {
        char *a = "aaa";
        char *b = "bbb";
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    } {
        long a = -1;
        ASSERT_NOT_EQUAL(a, 0);
        ASSERT_LESS(a, 0);
        ASSERT_LESS_EQUAL(a, 0);
        ASSERT_MORE(0, a);
        ASSERT_MORE_EQUAL(0, a);
    } {
        double a = 0.123;
        ASSERT_NOT_EQUAL(a, 0.123000001);
        ASSERT_LESS(a, 0.123000001);
        ASSERT_LESS_EQUAL(a, 0.123000001);
        ASSERT_MORE(0.123000001, a);
        ASSERT_MORE_EQUAL(0.123000001, a);
    } {
        double a = 0.1 + 0.2;
        double b = 0.3;
        ASSERT_CLOSE(a, b);
        ASSERT_CLOSE(b, a);
        ASSERT_NOT_EQUAL(a, b + 1.0e-9);
        ASSERT_LESS(b, a);
        ASSERT_MORE(a, b);
        ASSERT_CLOSE(a, b, 0.01);
        ASSERT_NOT_CLOSE(a, b + 1.0e-9);
        ASSERT_NOT_CLOSE(a, b + 0.02, 0.01);
    } {
        float a = 0.3f;
        double b = 0.3;
        ASSERT_CLOSE(a, b);
        ASSERT_NOT_CLOSE(a, b + 1.0);
    } {
        float a = 0.1f + 0.2f;
        double b = 0.3;
        ASSERT_CLOSE(a, b);
    } {
        long a = -1;
        double b = -1;
        ASSERT_EQUAL(a, b);
        ASSERT_EQUAL(b, b);
        ASSERT_MORE_EQUAL(a, b);
        ASSERT_LESS_EQUAL(a, b);
    } {
        double a = -1;
        long b = -1;
        ASSERT_EQUAL(a, b);
        ASSERT_EQUAL(b, b);
        ASSERT_MORE_EQUAL(a, b);
        ASSERT_LESS_EQUAL(a, b);
    } {
        double a = -1;
        double b = 0;
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    } {
        float a = -1;
        double b = 1;
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    } {
        llong a = 1;
        double b = 1;
        ASSERT_EQUAL(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE_EQUAL(b, a);
    } {
        void *a = NULL;
        void *b = &a;
        ASSERT_NOT_EQUAL(a, b);
    } {
        int array[100];
        void *a = &array[0];
        void *b = &array[1];
        ASSERT_NOT_EQUAL(a, b);
        ASSERT_LESS(a, b);
        ASSERT_LESS_EQUAL(a, b);
        ASSERT_MORE(b, a);
        ASSERT_MORE_EQUAL(b, a);
    } {
        bool a = true;
        bool b = true;
        ASSERT_EQUAL(a, b);
    } {
        bool a = true;
        bool b = false;
        ASSERT_NOT_EQUAL(a, b);
    } {
        char haystack[] = "alpha beta gamma";
        char binary_haystack[] = { 'a', 'b', '\0', 'c', 'd' };

        ASSERT_CONTAINS(haystack, SIZEOF(haystack) - 1, "alpha");
        ASSERT_CONTAINS(haystack, SIZEOF(haystack) - 1, "beta");
        ASSERT_CONTAINS(binary_haystack, SIZEOF(binary_haystack), "cd");
        ASSERT_NOT_CONTAINS(haystack, SIZEOF(haystack) - 1, "delta");
        ASSERT_NOT_CONTAINS(haystack, 10, "gamma");
        ASSERT_NOT_CONTAINS(binary_haystack, SIZEOF(binary_haystack), "bc");
    } {
        // uncomment to trigger linking error
        /* double x = 0.1; */
        /* void *a = NULL; */
        /* ASSERT_MORE_EQUAL(x, a); */
        /* ASSERT_MORE_EQUAL(a, x); */
        /* bool b = true; */
        /* ASSERT_EQUAL(b, 1); */
    } {
        int a = 0;
        double b = 1;
        double close_a = 0.1 + 0.2;
        double close_b = 0.3;
        float array[10] = {0};
        char *string_null = NULL;
        char *string_some = "some";

        struct sigaction signal_action;
        signal_action.sa_handler = handler_failed_assertion;
        sigemptyset(&signal_action.sa_mask);
        signal_action.sa_flags = SA_RESTART;

        if (sigaction(SIGILL, &signal_action, NULL) != 0) {
            fprintf(stderr, "Error in sigaction: %s.\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "\nThe following assertions are supposed to fail");
        fprintf(stderr, "\nand then check the 'assertion_failed' variable\n");

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

        if (sigsetjmp(assert_env, 1) == 0) {
            ASSERT_NOT_CLOSE(close_a, close_b);
        }
        ASSERT(assertion_failed);
        assertion_failed = false;

        if (sigsetjmp(assert_env, 1) == 0) {
            ASSERT_NOT_CLOSE(close_a, close_b, 0.01);
        }
        ASSERT(assertion_failed);
        assertion_failed = false;

        if (sigsetjmp(assert_env, 1) == 0) {
            ASSERT_CONTAINS("alpha beta gamma\n", 17, "delta\n");
        }
        ASSERT(assertion_failed);
        assertion_failed = false;

        if (sigsetjmp(assert_env, 1) == 0) {
            ASSERT_NOT_CONTAINS("alpha beta\n gamma\n", 18, "beta\n");
        }
        ASSERT(assertion_failed);
        assertion_failed = false;
    }

    ASSERT(true);
    ASSERT(!false);
    ASSERT_EQUAL(true, true);
    ASSERT_EQUAL(0 < 1, 1 < 3);

    {
        char *path = "/tmp/test_assert.txt";
        FILE *file;

        if ((file = XFOPEN(path, "w")) == NULL) {
            fatal(EXIT_FAILURE);
        }

        fprintf(file, "contents\n");
        if (fclose(file)) {
            fatal(EXIT_FAILURE);
        }
        ASSERT_FILE_CONTAINS(path, "ontents");
    }

    exit(EXIT_SUCCESS);
}
#endif

#endif /* ASSERT_C */
