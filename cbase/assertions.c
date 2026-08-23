// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(ASSERTIONS_C)
#define ASSERTIONS_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_assertions 1
#elif !defined(TESTING_assertions)
#define TESTING_assertions 0
#endif

#include "cbase.h"

void
assert_error(char *file, int32 line, char *func, char *format, ...) {
    va_list ap;

    fprintf(stderr,
            "%s:%d:"RED("%s()")": Assertion failed:\n", file, line, func);

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);

    return;
}

void
assert_file_contains(char *file, int32 line, char *func,
                     char *path, char *needle) {
    char *buffer;
    int32 buffer_len;
    int32 needle_len = strlen32(needle);

    if (!read_entire_file(path, &buffer, &buffer_len)) {
        TRAP();
    }
    if (!memmem64(buffer, buffer_len, needle, needle_len)) {
        assert_error(file, line, func,
                     "Needle '%s' not found in file '%s'.\n", needle, path);
        TRAP();
    }
    free2(buffer, buffer_len + 1);
    return;
}

void
assert_contains(char *file, int32 line, char *func,
                char *haystack, int32 haystack_len, char *needle) {
    int32 needle_len = strlen32(needle);
    if (memmem64(haystack, haystack_len, needle, needle_len) == NULL) {
        assert_error(file, line, func,
                     "expected to find substring'''\n"
                     GREEN("%.*s")"''' in '''"BLUE("%.*s")"'''\n",
                     needle_len, needle, haystack_len, haystack);
        TRAP();
    }
}

void
assert_not_contains(char *file, int32 line, char *func,
                    char *haystack, int32 haystack_len, char *needle) {
    int32 needle_len = strlen32(needle);
    if (memmem64(haystack, haystack_len, needle, needle_len)) {
        assert_error(file, line, func,
                     "expected to not find substring'''\n"
                     GREEN("%.*s")"''' in '''"BLUE("%.*s")"'''\n",
                     needle_len, needle, haystack_len, haystack);
        TRAP();
    }
}

void
assert_glob_match_failed(char *file, int32 line, char *func,
                         char *string_name, char *glob_name,
                         char *string, int32 string_len,
                         char *glob, int32 glob_len,
                         bool expected) {
    char *expected_text;

    if (expected) {
        expected_text = "expected glob to match";
    } else {
        expected_text = "expected glob to not match";
    }

    assert_error(file, line, func,
                 "%s:\n"
                 "  string: %s[%d] = '''"BLUE("%.*s")"'''\n"
                 "  glob:   %s[%d] = '''"GREEN("%.*s")"'''\n",
                 expected_text,
                 string_name, string_len, string_len, string,
                 glob_name, glob_len, glob_len, glob);
    return;
}

#define GENERATE_ASSERT_STRINGS(MODE, SYMBOL)                                  \
void                                                                           \
a_strings_##MODE(char *file, int32 line, char *func,                           \
                 char *name1, char *name2,                                     \
                 char *var1, char *var2) {                                     \
    if (var2 && (var1 == NULL)) {                                              \
        assert_error(file, line, func,                                         \
                     "%s is NULL, %s is \"%s\"\n", name1, name2, var2);        \
        TRAP();                                                                \
    }                                                                          \
    if (var1 && (var2 == NULL)) {                                              \
        assert_error(file, line, func,                                         \
                     "%s is NULL, %s is \"%s\"\n", name2, name1, var1);        \
        TRAP();                                                                \
    }                                                                          \
    if (!(strcmp(var1, var2) SYMBOL 0)) {                                      \
        assert_error(file, line, func,                                         \
                     "%s = %s " #SYMBOL " %s = %s\n",                          \
                     name1, var1, var2, name2);                                \
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
void                                                                           \
a_pointers_##MODE(char *file, int32 line, char *func,                          \
                  char *name1, char *name2,                                    \
                  void *var1, void *var2) {                                    \
    if (!((uintptr)var1 SYMBOL (uintptr)var2)) {                               \
        assert_error(file, line, func,                                         \
                     "%s = %p " #SYMBOL " %p = %s\n",                          \
                     name1, var1, var2, name2);                                \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
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

#define GENERATE_ASSERT_INTEGERS_SAME_SIGN(SIGN, FMT, SYMBOL, MODE)            \
void                                                                           \
a_both_##SIGN##_##MODE(char *file, int32 line, char *func,                     \
                       char *name1, char *name2,                               \
                       char *type1, char *type2,                               \
                       llong bits1, llong bits2,                               \
                       SIGN long long var1, SIGN long long var2) {             \
    if (!(var1 SYMBOL var2)) {                                                 \
        assert_error(file, line, func,                                         \
                     "[%s%lld]%s = "FMT" " #SYMBOL " "FMT" = %s[%s%lld]\n",    \
                     type1, bits1, name1, var1, var2, name2, type2, bits2);    \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
        TRAP();                                                                \
    }                                                                          \
    return;                                                                    \
}

GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed,   "%lld", ==, equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llx", ==, equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed,   "%lld", !=, not_equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llx", !=, not_equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed,   "%lld", <,  less)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llx", <,  less)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed,   "%lld", <=, less_equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llx", <=, less_equal)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed,   "%lld", >,  more)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(unsigned, "%llx", >,  more)
GENERATE_ASSERT_INTEGERS_SAME_SIGN(signed,   "%lld", >=, more_equal)
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
void                                                                           \
a_signed_unsigned##MODE(char *file, int32 line, char *func,                    \
                        char *name1, char *name2,                              \
                        char *type1, char *type2,                              \
                        llong bits1, llong bits2,                              \
                        llong var1, ullong var2) {                             \
    if (!(compare_sign_with_unsign(var1, var2) SYMBOL 0)) {                    \
        assert_error(file, line, func,                                         \
                     "[%s%lld]%s = %lld " #SYMBOL " %llu = %s[%s%lld]\n",      \
                     type1, bits1, name1, var1, var2, name2, type2, bits2);    \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
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
void                                                                           \
a_unsigned_signed_##MODE(char *file, int32 line, char *func,                   \
                         char *name1, char *name2,                             \
                         char *type1, char *type2,                             \
                         llong bits1, llong bits2,                             \
                         ullong var1, llong var2) {                            \
    if (!((-compare_sign_with_unsign(var2, var1)) SYMBOL 0)) {                 \
        assert_error(file, line, func,                                         \
                     "[%s%lld]%s = %llu " #SYMBOL " %lld = %s[%s%lld]\n",      \
                     type1, bits1, name1, var1, var2, name2, type2, bits2);    \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
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
void                                                                           \
a_double_##MODE(char *file, int32 line, char *func,                            \
                char *name1, char *name2,                                      \
                char *type1, char *type2,                                      \
                llong bits1, llong bits2,                                      \
                double var1, double var2) {                                    \
    if (!(var1 SYMBOL var2)) {                                                 \
        assert_error(file, line, func,                                         \
                     "[%s%lld]%s = %f " #SYMBOL " %f = %s[%s%lld]\n",          \
                     type1, bits1, name1, var1, var2, name2, type2, bits2);    \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
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
    union {
        double as_double;
        uint64 as_uint;
    } bits;
    uint64 sign_mask = 0x8000000000000000ull;

    bits.as_double = x;
    return (bits.as_uint & sign_mask) != 0;
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
                            double *diff_out, bool *handled_out,
                            bool reject_opposite_sign) {
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
        if (!reject_opposite_sign) {
            *handled_out = false;
            return false;
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

    if (assert_double_special_close(var1, var2, diff_out, &handled, true)) {
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
assert_double_close_tol(double var1, double var2,
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

    if (assert_double_special_close(var1, var2, diff_out, &handled, false)) {
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

static noreturn void
assert_double_failure(char *file, int32 line, char *func,
                      char *name1, char *name2,
                      char *type1, char *type2,
                      llong bits1, llong bits2,
                      double var1, double var2, char *symbol,
                      double diff, double tolerance,
                      ullong ulps, ullong max_ulps,
                      bool use_tol) {
    assert_error(file, line, func,
                 "[%s%lld]%s = %.17g %s %.17g = %s[%s%lld]\n",
                 type1, bits1, name1, var1, symbol, var2, name2, type2, bits2);
    if (use_tol) {
        fprintf(stderr,
                "floating diff = %.17g, tolerance = %.17g\n",
                diff, tolerance);
    } else {
        fprintf(stderr,
                "floating diff = %.17g, ulps = %llu, max_ulps = %llu\n",
                diff, ulps, max_ulps);
    }
    if (!DEBUGGING) {
        UNREACHABLE();
    }
    TRAP();
}

#define GENERATE_A_DOUBLE_CLOSE(MODE, SYMBOL, EXPECT_CLOSE)                    \
void                                                                           \
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
                                 &diff, &ulps, &max_ulps) != EXPECT_CLOSE) {   \
        assert_double_failure(file, line, func, name1, name2,                  \
                              type1, type2, bits1, bits2,                      \
                              var1, var2, SYMBOL, diff, (double)0,             \
                              ulps, max_ulps, false);                          \
    }                                                                          \
    return;                                                                    \
}

GENERATE_A_DOUBLE_CLOSE(close, "~=", true)
GENERATE_A_DOUBLE_CLOSE(not_close, "!~=", false)

#undef GENERATE_A_DOUBLE_CLOSE

#define GENERATE_A_DOUBLE_CLOSE_TOL(MODE, SYMBOL, EXPECT_CLOSE)                \
void                                                                           \
a_double_##MODE(char *file, int32 line, char *func,                            \
                char *name1, char *name2,                                      \
                char *type1, char *type2,                                      \
                llong bits1, llong bits2,                                      \
                double var1, double var2,                                      \
                double tolerance) {                                            \
    double diff;                                                               \
    double tolerance_abs;                                                      \
                                                                               \
    if (assert_double_close_tol(var1, var2, tolerance,                         \
                                &diff, &tolerance_abs) != EXPECT_CLOSE) {      \
        assert_double_failure(file, line, func, name1, name2,                  \
                              type1, type2, bits1, bits2,                      \
                              var1, var2, SYMBOL, diff, tolerance_abs,         \
                              0, 0, true);                                     \
    }                                                                          \
    return;                                                                    \
}

GENERATE_A_DOUBLE_CLOSE_TOL(close_tol, "~=", true)
GENERATE_A_DOUBLE_CLOSE_TOL(not_close_tol, "!~=", false)

#undef GENERATE_A_DOUBLE_CLOSE_TOL

#define GENERATE_ASSERT_BOOLS(MODE, SYMBOL)                                    \
void                                                                           \
a_bool_##MODE(char *file, int32 line, char *func,                              \
              char *name1, char *name2,                                        \
              char *type1, char *type2,                                        \
              llong bits1, llong bits2,                                        \
              bool var1, bool var2) {                                          \
    if (!(var1 SYMBOL var2)) {                                                 \
        char *s1 = "false";                                                    \
        char *s2 = "false";                                                    \
        if (var1) {                                                            \
            s1 = "true";                                                       \
        }                                                                      \
        if (var2) {                                                            \
            s2 = "true";                                                       \
        }                                                                      \
        assert_error(file, line, func,                                         \
                     "[%s%lld]%s = %s " #SYMBOL " %s = %s[%s%lld]\n",          \
                     type1, bits1, name1, s1, s2, name2, type2, bits2);        \
        if (!DEBUGGING) {                                                      \
            UNREACHABLE();                                                     \
        }                                                                      \
        TRAP();                                                                \
    }                                                                          \
    return;                                                                    \
}

GENERATE_ASSERT_BOOLS(equal, ==)
GENERATE_ASSERT_BOOLS(not_equal, !=)

noreturn void
a_bool_more(void *p, ...) {
    (void)p;
    TRAP();
}
noreturn void
a_bool_less(void *p, ...) {
    (void)p;
    TRAP();
}
noreturn void
a_bool_more_equal(void *p, ...) {
    (void)p;
    TRAP();
}
noreturn void
a_bool_less_equal(void *p, ...) {
    (void)p;
    TRAP();
}

#undef GENERATE_ASSERT_BOOLS

#if 0 == TESTING_assertions
static inline void
assert_functions_sink(void) {
    (void)assert_functions_sink;
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
    (void)a_double_close_tol;
    (void)a_double_not_close_tol;
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

#if TESTING_assertions
#define CBASE_IMPLEMENT
#include "cbase.h"

#if OS_LINUX
static sig_atomic_t assertion_failed = false;
static sigjmp_buf assert_env;

static noreturn void
handler_failed_assertion(int unused) {
    (void)unused;
    assertion_failed = true;
    siglongjmp(assert_env, 1);
}
#endif

int
main(void) {
    ASSERT(true);
    ASSERT(!false);
    ASSERT(1);
    ASSERT_ZERO(0);
    ASSERT_ZERO(0u);
    ASSERT_ZERO(0ll);

    ASSERT_POSITIVE(1);
    ASSERT_NEGATIVE(-1);

    ASSERT_NON_NEGATIVE(0);
    ASSERT_NON_POSITIVE(0);
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
        char *a = "aaabbb";
        ASSERT_EQUAL(a, 3, "aaa");
    } {
        char *a = "aaabbb";
        char *b = "aaaccc";
        ASSERT_EQUAL(a, 3, b, 3);
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
        ASSERT_NEGATIVE(a);
        ASSERT_NEGATIVE(a);
        ASSERT_NON_POSITIVE(a);
        ASSERT_NEGATIVE(a);
        ASSERT_NON_POSITIVE(a);
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
        double c = -1.0e-16;
        ASSERT_CLOSE(a, b);
        ASSERT_CLOSE(b, a);
        ASSERT_NOT_EQUAL(a, b + 1.0e-9);
        ASSERT_LESS(b, a);
        ASSERT_MORE(a, b);
        ASSERT_CLOSE(a, b, 0.01);
        ASSERT_CLOSE(c, 0.0, 0.01);
        ASSERT_CLOSE(0.0, c, 0.01);
        ASSERT_NOT_CLOSE(a, b + 1.0e-9);
        ASSERT_NOT_CLOSE(a, b + 0.02, 0.01);
        ASSERT_NOT_CLOSE(c, 0.02, 0.01);
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
        ASSERT_GLOB_MATCH(haystack, "alpha*gamma");
        ASSERT_GLOB_MATCH(binary_haystack, SIZEOF(binary_haystack), "a*d");
        ASSERT_GLOB_NO_MATCH(haystack, "alpha*delta");
        ASSERT_GLOB_NO_MATCH(binary_haystack, SIZEOF(binary_haystack), "a*c");
    } {
        // uncomment to trigger linking error
        /* double x = 0.1; */
        /* void *a = NULL; */
        /* ASSERT_MORE_EQUAL(x, a); */
        /* ASSERT_MORE_EQUAL(a, x); */
        /* bool b = true; */
        /* ASSERT_EQUAL(b, 1); */
    } 

#if OS_LINUX
    {
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
            ASSERT_EQUAL(string_some, 3, "none");
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

        if (sigsetjmp(assert_env, 1) == 0) {
            ASSERT_GLOB_MATCH("alpha beta gamma", "alpha*delta");
        }
        ASSERT(assertion_failed);
        assertion_failed = false;

        if (sigsetjmp(assert_env, 1) == 0) {
            ASSERT_GLOB_NO_MATCH("alpha beta gamma", "alpha*gamma");
        }
        ASSERT(assertion_failed);
        assertion_failed = false;
    }
#endif

    ASSERT(true);
    ASSERT(!false);
    ASSERT_EQUAL(true, true);
    ASSERT_EQUAL(0 < 1, 1 < 3);

    {
        char temp_dir[PATH_MAX];
        char path[PATH_MAX];
        FILE *file;

        test_make_temp_dir(temp_dir, SIZEOF(temp_dir), "assertions");
        test_join_path(path, SIZEOF(path), temp_dir, "test_assert.txt");

        if ((file = XFOPEN(path, "w")) == NULL) {
            fatal(EXIT_FAILURE);
        }

        fprintf(file, "contents\n");
        if (fclose(file)) {
            fatal(EXIT_FAILURE);
        }
        ASSERT_FILE_CONTAINS(path, "ontents");
        test_remove_tree(temp_dir);
    }

    printf("All assertions tests passed.\n");
    exit(EXIT_SUCCESS);
}
#endif

#endif /* ASSERTIONS_C */
