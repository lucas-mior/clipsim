// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(RYU_C)
#define RYU_C

#if !defined(TESTING_ryu)
#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_ryu 1
#else
#define TESTING_ryu 0
#endif
#endif

#include "cbase.h"

#if 0 == TESTING_ryu
#define RYU_IMPLEMENT
#include "ryu.h"
#endif

#if TESTING_ryu
#define CBASE_IMPLEMENT
#include "cbase.h"

#define RYU_IMPLEMENT
#include "ryu.h"

static void
test_ryu_assert(bool condition) {
    if (!condition) {
        exit(EXIT_FAILURE);
    }

    return;
}

static void
test_ryu_result(char *actual, int32 actual_len, char *expected) {
    test_ryu_assert(actual_len > 0);

    actual[actual_len] = '\0';
    for (int32 i = 0; i < actual_len; i += 1) {
        test_ryu_assert(actual[i] == expected[i]);
    }
    test_ryu_assert(expected[actual_len] == '\0');

    return;
}

static uint64
test_ryu_double_bits(double value) {
    uint64 bits;

    memcpy(&bits, &value, SIZEOF(bits));
    return bits;
}

static double
test_ryu_double_from_bits(uint64 bits) {
    double value;

    memcpy(&value, &bits, SIZEOF(value));
    return value;
}

// Exercise word-width shifts directly: normal decimal inputs rarely require
// these shifts, but the rounding helpers must remain defined for them.
static void
test_ryu_round_shift_boundaries(void) {
    bool rounded_up;

    ASSERT_EQ(s2d_round_shift(5, 0, true, &rounded_up), 5);
    ASSERT(!rounded_up);
    ASSERT_EQ(s2d_round_shift(5, 1, true, &rounded_up), 2);
    ASSERT(!rounded_up);
    ASSERT_EQ(s2d_round_shift(7, 1, true, &rounded_up), 4);
    ASSERT(rounded_up);
    ASSERT_EQ(s2d_round_shift(0xc000000000000000ull, 63, true,
                              &rounded_up), 2);
    ASSERT(rounded_up);
    ASSERT_EQ(s2d_round_shift(0x8000000000000000ull, 64, true,
                              &rounded_up), 0);
    ASSERT(!rounded_up);
    ASSERT_EQ(s2d_round_shift(0x8000000000000000ull, 64, false,
                              &rounded_up), 1);
    ASSERT(rounded_up);
    ASSERT_EQ(s2d_round_shift(0x8000000000000001ull, 64, true,
                              &rounded_up), 1);
    ASSERT(rounded_up);
    ASSERT_EQ(s2d_round_shift(UINT64_MAX, 65, false, &rounded_up), 0);
    ASSERT(!rounded_up);

    ASSERT_EQ(s2f_round_shift(5, 0, true, &rounded_up), 5);
    ASSERT(!rounded_up);
    ASSERT_EQ(s2f_round_shift(5, 1, true, &rounded_up), 2);
    ASSERT(!rounded_up);
    ASSERT_EQ(s2f_round_shift(7, 1, true, &rounded_up), 4);
    ASSERT(rounded_up);
    ASSERT_EQ(s2f_round_shift(0xc0000000u, 31, true, &rounded_up), 2);
    ASSERT(rounded_up);
    ASSERT_EQ(s2f_round_shift(0x80000000u, 32, true, &rounded_up), 0);
    ASSERT(!rounded_up);
    ASSERT_EQ(s2f_round_shift(0x80000000u, 32, false, &rounded_up), 1);
    ASSERT(rounded_up);
    ASSERT_EQ(s2f_round_shift(0x80000001u, 32, true, &rounded_up), 1);
    ASSERT(rounded_up);
    ASSERT_EQ(s2f_round_shift(UINT32_MAX, 33, false, &rounded_up), 0);
    ASSERT(!rounded_up);
    return;
}

static void
test_ryu_s2d_leading_plus(void) {
    double value;
    int32 used;

    used = s2d("+1.25rest", &value);
    ASSERT_EQ(used, 5);
    ASSERT_EQ(test_ryu_double_bits(value), test_ryu_double_bits(1.25));

    used = s2d("+1e+2rest", &value);
    ASSERT_EQ(used, 5);
    ASSERT_EQ(test_ryu_double_bits(value), test_ryu_double_bits(100.0));

    used = s2d("+0x1.8p+2rest", &value);
    ASSERT_EQ(used, 9);
    ASSERT_EQ(test_ryu_double_bits(value), test_ryu_double_bits(6.0));

    used = s2d("+inf", &value);
    ASSERT_EQ(used, 4);
    ASSERT_EQ(test_ryu_double_bits(value), 0x7ff0000000000000ull);

    used = s2d("+nan", &value);
    ASSERT_EQ(used, 4);
    ASSERT_EQ(test_ryu_double_bits(value), 0x7ff8000000000000ull);

    used = s2d("+", &value);
    ASSERT_EQ(used, -MALFORMED_INPUT);

    {
        char input[] = {'+', '2', '.', '5', 'x'};

        used = s2d_n(input, 4, &value);
        ASSERT_EQ(used, 4);
        ASSERT_EQ(test_ryu_double_bits(value), test_ryu_double_bits(2.5));
    }
    return;
}

static void
test_ryu_s2d_range_values(void) {
    double value;
    int32 used;

    used = s2d("1e309", &value);
    ASSERT_EQ(used, 5);
    ASSERT_EQ(test_ryu_double_bits(value), 0x7ff0000000000000ull);

    used = s2d("-1e309", &value);
    ASSERT_EQ(used, 6);
    ASSERT(test_ryu_double_bits(value) == 0xfff0000000000000ull);

    used = s2d("1.7976931348623157e308", &value);
    ASSERT_EQ(used, 22);
    ASSERT_EQ(test_ryu_double_bits(value), 0x7fefffffffffffffull);

    used = s2d("1.7976931348623159e308", &value);
    ASSERT_EQ(used, 22);
    ASSERT_EQ(test_ryu_double_bits(value), 0x7ff0000000000000ull);

    used = s2d("1e-325", &value);
    ASSERT_EQ(used, -FLOAT_UNDERFLOW);
    ASSERT_EQ(test_ryu_double_bits(value), 0);

    used = s2d("-1e-325", &value);
    ASSERT_EQ(used, -FLOAT_UNDERFLOW);
    ASSERT(test_ryu_double_bits(value) == 0x8000000000000000ull);

    used = s2d("5e-324", &value);
    ASSERT_EQ(used, -FLOAT_UNDERFLOW);
    ASSERT_EQ(test_ryu_double_bits(value), 0x0000000000000001ull);

    used = s2d("2e-324", &value);
    ASSERT_EQ(used, -FLOAT_UNDERFLOW);
    ASSERT_EQ(test_ryu_double_bits(value), 0);

    used = s2d("3e-324", &value);
    ASSERT_EQ(used, -FLOAT_UNDERFLOW);
    ASSERT_EQ(test_ryu_double_bits(value), 0x0000000000000001ull);

    // The last subnormal can round up into the normal range. Its status
    // must describe the rounded result rather than the unrounded exponent.
    used = s2d("2.2250738585072013e-308", &value);
    ASSERT_EQ(used, 23);
    ASSERT_EQ(test_ryu_double_bits(value), 0x0010000000000000ull);

    used = s2d("2.2250738585072014e-308", &value);
    ASSERT_EQ(used, 23);
    ASSERT_EQ(test_ryu_double_bits(value), 0x0010000000000000ull);

    used = s2d("inf", &value);
    ASSERT_EQ(used, 3);
    ASSERT_EQ(test_ryu_double_bits(value), 0x7ff0000000000000ull);

    used = s2d("1.00000000000000000", &value);
    ASSERT_EQ(used, 19);
    ASSERT_EQ(test_ryu_double_bits(value), 0x3ff0000000000000ull);

    {
        char input[] = {'1', 'e', '3', '0', '9', 'x'};

        used = s2d_n(input, 5, &value);
        ASSERT_EQ(used, 5);
        ASSERT_EQ(test_ryu_double_bits(value), 0x7ff0000000000000ull);
    }
    return;
}

static void
test_ryu_s2d_hex(void) {
    static uint64 cases[] = {
        0x0000000000000000ull,
        0x8000000000000000ull,
        0x0000000000000001ull,
        0x000fffffffffffffull,
        0x0010000000000000ull,
        0x3ff0000000000000ull,
        0x3ff0000000000001ull,
        0x400921fb54442d18ull,
        0x7fefffffffffffffull,
        0xffefffffffffffffull,
    };
    char buffer[128];
    double value;
    int32 used;
    int32 len;

    used = s2d("0x1.8p+2rest", &value);
    ASSERT_EQ(used, 8);
    ASSERT_EQ(test_ryu_double_bits(value), test_ryu_double_bits(6.0));

    used = s2d("0X1.8P+2", &value);
    ASSERT_EQ(used, 8);
    ASSERT_EQ(test_ryu_double_bits(value), test_ryu_double_bits(6.0));

    used = s2d("-0x0p+0", &value);
    ASSERT_EQ(used, 7);
    ASSERT(test_ryu_double_bits(value) == 0x8000000000000000ull);

    used = s2d("0x0.0000000000001p-1022", &value);
    ASSERT_EQ(used, -FLOAT_UNDERFLOW);
    ASSERT_EQ(test_ryu_double_bits(value), 0x0000000000000001ull);

    used = s2d("0x0.fffffffffffffp-1022", &value);
    ASSERT_EQ(used, -FLOAT_UNDERFLOW);
    ASSERT_EQ(test_ryu_double_bits(value), 0x000fffffffffffffull);

    used = s2d("0x0.fffffffffffff8p-1022", &value);
    ASSERT_EQ(used, 24);
    ASSERT_EQ(test_ryu_double_bits(value), 0x0010000000000000ull);

    used = s2d("0x1p-1022", &value);
    ASSERT_EQ(used, 9);
    ASSERT_EQ(test_ryu_double_bits(value), 0x0010000000000000ull);

    used = s2d("0x1.fffffffffffffp+1023", &value);
    ASSERT_EQ(used, 23);
    ASSERT_EQ(test_ryu_double_bits(value), 0x7fefffffffffffffull);

    used = s2d("0x1p+1024", &value);
    ASSERT_EQ(used, 9);
    ASSERT_EQ(test_ryu_double_bits(value), 0x7ff0000000000000ull);

    used = s2d("0x1p-1075", &value);
    ASSERT_EQ(used, -FLOAT_UNDERFLOW);
    ASSERT(test_ryu_double_bits(value) == 0);

    used = s2d("0x1.00000000000008p+0", &value);
    ASSERT_EQ(used, 21);
    ASSERT_EQ(test_ryu_double_bits(value), 0x3ff0000000000000ull);

    used = s2d("0x1.00000000000018p+0", &value);
    ASSERT_EQ(used, 21);
    ASSERT_EQ(test_ryu_double_bits(value), 0x3ff0000000000002ull);

    used = s2d("inf", &value);
    ASSERT_EQ(used, 3);
    ASSERT_EQ(test_ryu_double_bits(value), 0x7ff0000000000000ull);

    used = s2d("-infinity", &value);
    ASSERT_EQ(used, 9);
    ASSERT(test_ryu_double_bits(value) == 0xfff0000000000000ull);

    used = s2d("nan", &value);
    ASSERT_EQ(used, 3);
    ASSERT_EQ(test_ryu_double_bits(value), 0x7ff8000000000000ull);

    {
        char input[] = {'0', 'x', '1', '.', '8', 'p', '+', '2', 'x'};

        used = s2d_n(input, 8, &value);
        ASSERT_EQ(used, 8);
        ASSERT_EQ(test_ryu_double_bits(value), test_ryu_double_bits(6.0));
    }

    for (int32 i = 0; i < SIZEOF(cases)/SIZEOF(cases[0]); i += 1) {
        double original = test_ryu_double_from_bits(cases[i]);

        len = fmt_snprintf(buffer, SIZEOF(buffer), "%a", original);
        ASSERT_GT(len, 0);
        used = s2d(buffer, &value);
        if (((cases[i] >> 52) & 0x7ff) == 0
            && (cases[i] & 0x000fffffffffffffull) != 0) {
            ASSERT_EQ(used, -FLOAT_UNDERFLOW);
        } else {
            ASSERT_EQ(used, len);
        }
        ASSERT(test_ryu_double_bits(value) == cases[i]);
    }
    return;
}

static void
test_ryu_s2d_long_decimal(void) {
    static char *inputs[] = {
        "1757.51723797804425",
        "1658.93064536183874",
        "1543.01447896234890",
        "1414.27642536463622",
        "1770.56667151757733",
    };
    static uint64 expected[] = {
        0x409b7611a6d51fccull,
        0x4099ebb8fb190516ull,
        0x40981c0ed392b713ull,
        0x4096191b0f403397ull,
        0x409baa444589ce47ull,
    };
    char buffer[2000];
    double value;
    int32 used;

    for (int32 i = 0; i < 5; i += 1) {
        used = s2d(inputs[i], &value);
        ASSERT_EQ(used, (int32)strlen(inputs[i]));
        ASSERT_EQ(test_ryu_double_bits(value), expected[i]);
    }

    // Check exact halfway cases and one decimal unit on either side.
    used = s2d("1.00000000000000011102230246251565404236316680908203125",
               &value);
    ASSERT_EQ(used, 55);
    ASSERT_EQ(test_ryu_double_bits(value), 0x3ff0000000000000ull);

    used = s2d("1.00000000000000011102230246251565404236316680908203126",
               &value);
    ASSERT_EQ(used, 55);
    ASSERT_EQ(test_ryu_double_bits(value), 0x3ff0000000000001ull);

    used = s2d("1.00000000000000033306690738754696212708950042724609375",
               &value);
    ASSERT_EQ(used, 55);
    ASSERT_EQ(test_ryu_double_bits(value), 0x3ff0000000000002ull);

    used = s2d("1.000000000000000000123rest", &value);
    ASSERT_EQ(used, 23);
    ASSERT_EQ(test_ryu_double_bits(value), 0x3ff0000000000000ull);

    used = s2d("-1e-99999999999999999999999999", &value);
    ASSERT_EQ(used, -FLOAT_UNDERFLOW);
    ASSERT(test_ryu_double_bits(value) == 0x8000000000000000ull);

    // All finite doubles have an exact fixed-point expansion with at most
    // 1074 fractional digits. Round-trip diverse binary64 bit patterns.
    {
        static uint64 cases[] = {
            0x0000000000000001ull,
            0x000fffffffffffffull,
            0x0010000000000000ull,
            0x3fd5555555555555ull,
            0x3ff0000000000001ull,
            0x400921fb54442d18ull,
            0x7fefffffffffffffull,
        };

        for (int32 i = 0; i < SIZEOF(cases)/SIZEOF(cases[0]); i += 1) {
            double original = test_ryu_double_from_bits(cases[i]);
            int32 len = d2fixed_buffered_n(original, 1074, buffer);

            used = s2d_n(buffer, len, &value);
            if (cases[i] < 0x0010000000000000ull) {
                ASSERT_EQ(used, -FLOAT_UNDERFLOW);
            } else {
                ASSERT_EQ(used, len);
            }
            ASSERT_EQ(test_ryu_double_bits(value), cases[i]);
        }
    }

    // Digits after the retained prefix must resolve an exact halfway tie.
    {
        char *midpoint =
            "1.00000000000000011102230246251565404236316680908203125";
        int32 len = (int32)strlen(midpoint);

        memcpy(buffer, midpoint, (uint64)len);
        for (int32 i = len; i < 1000; i += 1) {
            buffer[i] = '0';
        }
        used = s2d_n(buffer, 1000, &value);
        ASSERT_EQ(used, 1000);
        ASSERT_EQ(test_ryu_double_bits(value), 0x3ff0000000000000ull);

        buffer[999] = '1';
        used = s2d_n(buffer, 1000, &value);
        ASSERT_EQ(used, 1000);
        ASSERT_EQ(test_ryu_double_bits(value), 0x3ff0000000000001ull);
    }

    // Very long zero padding and a nonzero digit beyond the retained prefix.
    buffer[0] = '1';
    buffer[1] = '.';
    for (int32 i = 2; i < 1500; i += 1) {
        buffer[i] = '0';
    }
    buffer[1499] = '1';
    used = s2d_n(buffer, 1500, &value);
    ASSERT_EQ(used, 1500);
    ASSERT_EQ(test_ryu_double_bits(value), 0x3ff0000000000000ull);

    return;
}

static void
test_ryu_s2d_decimal_interval(void) {
    static struct {
        char *input;
        uint64 bits;
    } cases[] = {
        {"9.999999999999999999999999999e-1", 0x3ff0000000000000ull},
        {"999999999999999999999999999999999999999e-39",
         0x3ff0000000000000ull},
        {"0.99999999999999999999999999999", 0x3ff0000000000000ull},
        {"1.00000000000000011102230246251565404236316680908203125",
         0x3ff0000000000000ull},
        {"1.00000000000000011102230246251565404236316680908203126",
         0x3ff0000000000001ull},
        {"2.225073858507201136057409796709131975934819546351645648e-308",
         0x000fffffffffffffull},
        {"2.225073858507201383090232717332404064219215980462331830e-308",
         0x0010000000000000ull},
        {"4.940656458412465441765687928682213723650598e-324",
         0x0000000000000001ull},
        {"1.79769313486231570814527423731704356798070e308",
         0x7fefffffffffffffull},
        {"1.79769313486231580814527423731704356798070e308",
         0x7ff0000000000000ull},
        {"-1.234567890123456789012345678901e-320",
         0x80000000000009c3ull},
    };
    double value;

    for (int32 i = 0; i < LENGTH(cases); i += 1) {
        int32 len = strlen32(cases[i].input);
        int32 expected_status = len;

        if ((cases[i].bits & 0x7ff0000000000000ull) == 0) {
            expected_status = -FLOAT_UNDERFLOW;
        }
        ASSERT_EQ(s2d_fast(cases[i].input, &value), expected_status);
        ASSERT(test_ryu_double_bits(value) == cases[i].bits);
        ASSERT_EQ(s2d_n(cases[i].input, len, &value), expected_status);
        ASSERT(test_ryu_double_bits(value) == cases[i].bits);
    }
    return;
}

static void
test_ryu_s2d_fast(void) {
    static char *inputs[] = {
        "0", "-0", "0.0", "+1.25rest", "-1e+2,rest",
        "1757.51723797804425\tmore", "-123.456e-15\n",
        "1.00000000000000011102230246251565404236316680908203125",
        "0x1.8p+2,", "-0x1.0p-1074!", "nan,", "-infinity\n",
        "1e-400,", "1e9999,", "1e+", "12.5.6",
        ".5 ", "2.", ".", "-", "", "abc", "12abc",
        "3.1415926535897932384626433832795028841971!",
    };
    double fast_value;
    double bounded_value;
    int32 fast_used;
    int32 bounded_used;

    for (int32 i = 0; i < LENGTH(inputs); i += 1) {
        char *input = inputs[i];

        fast_used = s2d_fast(input, &fast_value);
        bounded_used = s2d_n(input, strlen32(input), &bounded_value);
        ASSERT_EQ(fast_used, bounded_used);
        if (fast_used > 0 || fast_used == -FLOAT_UNDERFLOW) {
            ASSERT(test_ryu_double_bits(fast_value)
                   == test_ryu_double_bits(bounded_value));
        }
    }

    {
        char buffer[1601];

        buffer[0] = '1';
        buffer[1] = '.';
        for (int32 i = 2; i < 1599; i += 1) {
            buffer[i] = '0';
        }
        buffer[1599] = '1';
        buffer[1600] = '\0';
        fast_used = s2d_fast(buffer, &fast_value);
        bounded_used = s2d_n(buffer, 1600, &bounded_value);
        ASSERT_EQ(fast_used, bounded_used);
        ASSERT(test_ryu_double_bits(fast_value)
               == test_ryu_double_bits(bounded_value));
    }
    return;
}

typedef struct TestRyuDoubleFormatCase {
    uint64 bits;
    char *expected;
} TestRyuDoubleFormatCase;

typedef struct TestRyuFloatFormatCase {
    uint32 bits;
    char *expected;
} TestRyuFloatFormatCase;

typedef struct TestRyuPrecisionCase {
    uint64 bits;
    uint32 precision;
    char *expected;
} TestRyuPrecisionCase;

typedef struct TestRyuParseCase {
    char *input;
    int32 used;
    uint64 bits;
} TestRyuParseCase;

static uint32
tryu_float_bits(float value) {
    uint32 bits;

    memcpy(&bits, &value, SIZEOF(bits));
    return bits;
}

static float
tryu_float_from_bits(uint32 bits) {
    float value;

    memcpy(&value, &bits, SIZEOF(value));
    return value;
}

static uint64
tryu_next_random(uint64 *state) {
    uint64 value = *state;

    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    *state = value;
    return value;
}

static bool
tryu_double_bits_are_subnormal(uint64 bits) {
    bool result;

    result = ((bits & 0x7ff0000000000000ull) == 0)
             && ((bits & 0x000fffffffffffffull) != 0);
    return result;
}

static bool
tryu_double_bits_are_finite(uint64 bits) {
    bool result;

    result = (bits & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;
    return result;
}

static bool
tryu_float_bits_are_finite(uint32 bits) {
    bool result;

    result = (bits & 0x7f800000u) != 0x7f800000u;
    return result;
}

static void
tryu_check_result(char *actual, int32 actual_len, char *expected) {
    ASSERT_EQ(actual, actual_len, expected);
    return;
}

static void
tryu_check_double_format_case(TestRyuDoubleFormatCase test_case) {
    char buffer[128];
    double value;
    int32 len;

    value = test_ryu_double_from_bits(test_case.bits);
    len = d2s_buffered_n(value, buffer);
    tryu_check_result(buffer, len, test_case.expected);

    d2s_buffered(value, buffer);
    ASSERT_EQ(buffer, test_case.expected);
    return;
}

static void
tryu_check_float_format_case(TestRyuFloatFormatCase test_case) {
    char buffer[128];
    float value;
    int32 len;

    value = tryu_float_from_bits(test_case.bits);
    len = f2s_buffered_n(value, buffer);
    tryu_check_result(buffer, len, test_case.expected);

    f2s_buffered(value, buffer);
    ASSERT_EQ(buffer, test_case.expected);
    return;
}

static void
tryu_check_fixed_case(TestRyuPrecisionCase test_case) {
    char buffer[128];
    double value;
    int32 len;

    value = test_ryu_double_from_bits(test_case.bits);
    len = d2fixed_buffered_n(value, test_case.precision, buffer);
    tryu_check_result(buffer, len, test_case.expected);

    d2fixed_buffered(value, test_case.precision, buffer);
    ASSERT_EQ(buffer, test_case.expected);
    return;
}

static void
tryu_check_exp_case(TestRyuPrecisionCase test_case) {
    char buffer[128];
    double value;
    int32 len;

    value = test_ryu_double_from_bits(test_case.bits);
    len = d2exp_buffered_n(value, test_case.precision, buffer);
    tryu_check_result(buffer, len, test_case.expected);

    d2exp_buffered(value, test_case.precision, buffer);
    ASSERT_EQ(buffer, test_case.expected);
    return;
}

static void
tryu_check_parse_case(TestRyuParseCase test_case) {
    double value = 123.0;
    int32 used;

    used = s2d(test_case.input, &value);
    ASSERT_EQ(used, test_case.used);
    ASSERT(test_ryu_double_bits(value) == test_case.bits);
    return;
}

static void
tryu_test_reference_format_outputs(void) {
    // Reference strings captured from the unmodified vendored Ryu sources.
    static TestRyuDoubleFormatCase double_cases[] = {
        {0x0000000000000000ull, "0E0"},
        {0x8000000000000000ull, "-0E0"},
        {0x0000000000000001ull, "5E-324"},
        {0x000fffffffffffffull, "2.225073858507201E-308"},
        {0x0010000000000000ull, "2.2250738585072014E-308"},
        {0x3ff0000000000000ull, "1E0"},
        {0x3ff0000000000001ull, "1.0000000000000002E0"},
        {0x3fd5555555555555ull, "3.333333333333333E-1"},
        {0x400921fb54442d18ull, "3.141592653589793E0"},
        {0x7fefffffffffffffull, "1.7976931348623157E308"},
        {0x7ff0000000000000ull, "Infinity"},
        {0xfff0000000000000ull, "-Infinity"},
        {0x7ff8000000000000ull, "NaN"},
    };
    static TestRyuFloatFormatCase float_cases[] = {
        {0x00000000u, "0E0"},
        {0x80000000u, "-0E0"},
        {0x00000001u, "1E-45"},
        {0x007fffffu, "1.1754942E-38"},
        {0x00800000u, "1.1754944E-38"},
        {0x3f800000u, "1E0"},
        {0x3f800001u, "1.0000001E0"},
        {0x3eaaaaabu, "3.3333334E-1"},
        {0x40490fdbu, "3.1415927E0"},
        {0x7f7fffffu, "3.4028235E38"},
        {0x7f800000u, "Infinity"},
        {0xff800000u, "-Infinity"},
        {0x7fc00000u, "NaN"},
    };
    static TestRyuPrecisionCase fixed_cases[] = {
        {0x0000000000000000ull, 0, "0"},
        {0x8000000000000000ull, 3, "-0.000"},
        {0x3ff4000000000000ull, 2, "1.25"},
        {0x400921fb54442d18ull, 6, "3.141593"},
        {0x3fe0000000000000ull, 0, "0"},
        {0x3fe0000000000000ull, 1, "0.5"},
        {0x4028ae147ae147aeull, 2, "12.34"},
        {0x0000000000000001ull, 5, "0.00000"},
    };
    static TestRyuPrecisionCase exp_cases[] = {
        {0x0000000000000000ull, 0, "0e+00"},
        {0x8000000000000000ull, 3, "-0.000e+00"},
        {0x3ff4000000000000ull, 2, "1.25e+00"},
        {0x400921fb54442d18ull, 6, "3.141593e+00"},
        {0x3fe0000000000000ull, 0, "5e-01"},
        {0x3fe0000000000000ull, 1, "5.0e-01"},
        {0x4028ae147ae147aeull, 2, "1.23e+01"},
        {0x7fefffffffffffffull, 0, "2e+308"},
        {0x0000000000000001ull, 5, "4.94066e-324"},
        {0x7ff0000000000000ull, 2, "Infinity"},
        {0x7ff8000000000000ull, 2, "nan"},
    };

    for (int32 i = 0; i < LENGTH(double_cases); i += 1) {
        tryu_check_double_format_case(double_cases[i]);
    }
    for (int32 i = 0; i < LENGTH(float_cases); i += 1) {
        tryu_check_float_format_case(float_cases[i]);
    }
    for (int32 i = 0; i < LENGTH(fixed_cases); i += 1) {
        tryu_check_fixed_case(fixed_cases[i]);
    }
    for (int32 i = 0; i < LENGTH(exp_cases); i += 1) {
        tryu_check_exp_case(exp_cases[i]);
    }
    return;
}

static void
tryu_test_s2d_reference_outputs(void) {
    static TestRyuParseCase cases[] = {
        {"", -INPUT_TOO_SHORT, 0x405ec00000000000ull},
        {"+", -MALFORMED_INPUT, 0x405ec00000000000ull},
        {"-", -MALFORMED_INPUT, 0x405ec00000000000ull},
        {".", -MALFORMED_INPUT, 0x405ec00000000000ull},
        {"1e+", 1, 0x3ff0000000000000ull},
        {"1e-", 1, 0x3ff0000000000000ull},
        {"0x", 1, 0x0000000000000000ull},
        {"0x1p", 1, 0x0000000000000000ull},
        {"0x1p+", 1, 0x0000000000000000ull},
        {"nanx", 3, 0x7ff8000000000000ull},
        {"infinityx", 8, 0x7ff0000000000000ull},
        {"12.5.6", 4, 0x4029000000000000ull},
        {"abc", -MALFORMED_INPUT, 0x405ec00000000000ull},
        {" 1", -MALFORMED_INPUT, 0x405ec00000000000ull},
        {"+inf", 4, 0x7ff0000000000000ull},
        {"-nan", 4, 0xfff8000000000000ull},
        {"1e309", 5, 0x7ff0000000000000ull},
        {"-1e309", 6, 0xfff0000000000000ull},
        {"1e-325", -FLOAT_UNDERFLOW, 0x0000000000000000ull},
        {"-1e-325", -FLOAT_UNDERFLOW, 0x8000000000000000ull},
        {"5e-324", -FLOAT_UNDERFLOW, 0x0000000000000001ull},
        {"-5e-324", -FLOAT_UNDERFLOW, 0x8000000000000001ull},
    };

    for (int32 i = 0; i < LENGTH(cases); i += 1) {
        tryu_check_parse_case(cases[i]);
    }
    return;
}

static void
tryu_test_output_buffer_boundaries(void) {
    char scratch[128];
    char *buffer = scratch + 8;
    int32 len;

    memset(scratch, 'x', SIZEOF(scratch));
    len = d2s_buffered_n(1.0, buffer);
    tryu_check_result(buffer, len, "1E0");
    ASSERT_EQ(scratch[7], 'x');
    ASSERT_EQ(buffer[len], 'x');

    memset(scratch, 'x', SIZEOF(scratch));
    len = f2s_buffered_n(1.0f, buffer);
    tryu_check_result(buffer, len, "1E0");
    ASSERT_EQ(scratch[7], 'x');
    ASSERT_EQ(buffer[len], 'x');

    memset(scratch, 'x', SIZEOF(scratch));
    len = d2fixed_buffered_n(1.25, 2, buffer);
    tryu_check_result(buffer, len, "1.25");
    ASSERT_EQ(scratch[7], 'x');
    ASSERT_EQ(buffer[len], 'x');

    memset(scratch, 'x', SIZEOF(scratch));
    len = d2exp_buffered_n(1.25, 2, buffer);
    tryu_check_result(buffer, len, "1.25e+00");
    ASSERT_EQ(scratch[7], 'x');
    ASSERT_EQ(buffer[len], 'x');

    memset(scratch, 'x', SIZEOF(scratch));
    d2s_buffered(1.0, buffer);
    ASSERT_EQ(buffer, "1E0");
    ASSERT_EQ(scratch[7], 'x');
    ASSERT_EQ(buffer[3], '\0');

    memset(scratch, 'x', SIZEOF(scratch));
    f2s_buffered(1.0f, buffer);
    ASSERT_EQ(buffer, "1E0");
    ASSERT_EQ(scratch[7], 'x');
    ASSERT_EQ(buffer[3], '\0');

    memset(scratch, 'x', SIZEOF(scratch));
    d2fixed_buffered(1.25, 2, buffer);
    ASSERT_EQ(buffer, "1.25");
    ASSERT_EQ(scratch[7], 'x');
    ASSERT_EQ(buffer[4], '\0');

    memset(scratch, 'x', SIZEOF(scratch));
    d2exp_buffered(1.25, 2, buffer);
    ASSERT_EQ(buffer, "1.25e+00");
    ASSERT_EQ(scratch[7], 'x');
    ASSERT_EQ(buffer[8], '\0');
    return;
}

static void
tryu_test_deterministic_double_roundtrip(void) {
    uint64 state = 0xbadc0ffee0ddf00dull;
    char buffer[128];
    double original;
    double parsed;
    uint64 bits;
    int32 len;
    int32 used;

    for (int32 i = 0; i < 4096; i += 1) {
        bits = tryu_next_random(&state);
        if (!tryu_double_bits_are_finite(bits)) {
            bits &= 0x7fefffffffffffffull;
        }
        original = test_ryu_double_from_bits(bits);
        len = d2s_buffered_n(original, buffer);
        used = s2d_n(buffer, len, &parsed);
        if (tryu_double_bits_are_subnormal(bits)) {
            ASSERT_EQ(used, -FLOAT_UNDERFLOW);
        } else {
            ASSERT_EQ(used, len);
        }
        ASSERT(test_ryu_double_bits(parsed) == bits);
    }
    return;
}

static void
tryu_test_deterministic_float_roundtrip(void) {
    uint64 state = 0x123456789abcdef0ull;
    char buffer[128];
    double parsed;
    float original;
    uint32 bits;
    int32 len;
    int32 used;

    for (int32 i = 0; i < 4096; i += 1) {
        bits = (uint32)tryu_next_random(&state);
        if (!tryu_float_bits_are_finite(bits)) {
            bits &= 0x7f7fffffu;
        }
        original = tryu_float_from_bits(bits);
        len = f2s_buffered_n(original, buffer);
        used = s2d_n(buffer, len, &parsed);
        ASSERT_EQ(used, len);
        ASSERT(tryu_float_bits((float)parsed) == bits);
    }
    return;
}

typedef struct TryuS2fCase {
    char *input;
    enum Status status;
    uint32 bits;
} TryuS2fCase;

static uint32
tryu_s2f_bits(float value) {
    uint32 bits;

    memcpy(&bits, &value, SIZEOF(bits));
    return bits;
}

static float
tryu_s2f_from_bits(uint32 bits) {
    float value;

    memcpy(&value, &bits, SIZEOF(value));
    return value;
}

static uint64
tryu_s2f_next_random(uint64 *state) {
    uint64 value = *state;

    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    *state = value;
    return value;
}

static bool
tryu_s2f_is_finite(uint32 bits) {
    bool result;

    result = (bits & 0x7f800000u) != 0x7f800000u;
    return result;
}

static void
tryu_s2f_reference_outputs(void) {
    // Expected IEEE-754 results, including correct subnormal rounding.
    static TryuS2fCase cases[] = {
        {"0", SUCCESS, 0x00000000u},
        {"-0", SUCCESS, 0x80000000u},
        {"1", SUCCESS, 0x3f800000u},
        {"1.0000001", SUCCESS, 0x3f800001u},
        {"3.4028235e38", SUCCESS, 0x7f7fffffu},
        {"3.5e38", SUCCESS, 0x7f800000u},
        {"1e-46", SUCCESS, 0x00000000u},
        {"1e-45", SUCCESS, 0x00000001u},
        {"-1e-45", SUCCESS, 0x80000001u},
        {"1e-40", SUCCESS, 0x000116c2u},
        {"-257334811e-52", SUCCESS, 0x80000012u},
        {"1.1754944e-38", SUCCESS, 0x00800000u},
        {"1.1754942e-38", SUCCESS, 0x007fffffu},
        {"0.33333334", SUCCESS, 0x3eaaaaabu},
        {"16777217", SUCCESS, 0x4b800000u},
        {"nan", MALFORMED_INPUT, 0x42280000u},
        {"inf", MALFORMED_INPUT, 0x42280000u},
        {"-inf", MALFORMED_INPUT, 0x42280000u},
        {"abc", MALFORMED_INPUT, 0x42280000u},
        {"1e", SUCCESS, 0x3f800000u},
        {"1e99999", INPUT_TOO_LONG, 0x42280000u},
        {"1234567890", INPUT_TOO_LONG, 0x42280000u},
    };
    float value;
    enum Status status;

    for (int32 i = 0; i < LENGTH(cases); i += 1) {
        value = 42.0f;
        status = s2f(cases[i].input, &value);
        ASSERT(status == cases[i].status);
        ASSERT(tryu_s2f_bits(value) == cases[i].bits);
    }
    return;
}

static void
tryu_s2f_bounded_inputs(void) {
    char input[] = {'1', '.', '2', '5', 'x'};
    float value;
    enum Status status;

    value = 0.0f;
    status = s2f_n(input, 4, &value);
    ASSERT(status == SUCCESS);
    ASSERT(tryu_s2f_bits(value) == 0x3fa00000u);

    value = 42.0f;
    status = s2f_n(input, 5, &value);
    ASSERT(status == MALFORMED_INPUT);
    ASSERT(tryu_s2f_bits(value) == 0x42280000u);
    return;
}

static void
tryu_s2f_deterministic_roundtrip(void) {
    uint64 state = 0xfeedfacecafebeefull;
    char buffer[128];
    float original;
    float parsed;
    uint32 bits;
    int32 len;

    for (int32 i = 0; i < 4096; i += 1) {
        bits = (uint32)tryu_s2f_next_random(&state);
        if (!tryu_s2f_is_finite(bits)) {
            bits &= 0x7f7fffffu;
        }
        if ((bits & 0x7f800000u) == 0 && (bits & 0x007fffffu) != 0) {
            bits |= 0x00800000u;
        }
        original = tryu_s2f_from_bits(bits);
        len = f2s_buffered_n(original, buffer);
        ASSERT(s2f_n(buffer, len, &parsed) == SUCCESS);
        ASSERT(tryu_s2f_bits(parsed) == bits);
    }
    return;
}


static void
tryu_benchmark_d2s(int32 nitems) {
    uint64 state = 0x243f6a8885a308d3ull;
    char buffer[128];
    int64 t0;
    int64 t1;
    int64 total_len = 0;

    t0 = time_monotonic_now();
    for (int32 i = 0; i < nitems; i += 1) {
        uint64 bits = tryu_next_random(&state) & 0x7fefffffffffffffull;
        double value = test_ryu_double_from_bits(bits);

        total_len += d2s_buffered_n(value, buffer);
    }
    t1 = time_monotonic_now();
    fmt_printf("ryu bench d2s_buffered_n: %lld ns/op sink=%lld\n",
               (llong)((t1 - t0) / nitems), (llong)total_len);
    return;
}

static void
tryu_benchmark_f2s(int32 nitems) {
    uint64 state = 0x13198a2e03707344ull;
    char buffer[128];
    int64 t0;
    int64 t1;
    int64 total_len = 0;

    t0 = time_monotonic_now();
    for (int32 i = 0; i < nitems; i += 1) {
        uint32 bits = (uint32)tryu_next_random(&state) & 0x7f7fffffu;
        float value = tryu_float_from_bits(bits);

        total_len += f2s_buffered_n(value, buffer);
    }
    t1 = time_monotonic_now();
    fmt_printf("ryu bench f2s_buffered_n: %lld ns/op sink=%lld\n",
               (llong)((t1 - t0) / nitems), (llong)total_len);
    return;
}

static void
tryu_benchmark_d2fixed(int32 nitems) {
    uint64 state = 0xa4093822299f31d0ull;
    char buffer[2000];
    int64 t0;
    int64 t1;
    int64 total_len = 0;

    t0 = time_monotonic_now();
    for (int32 i = 0; i < nitems; i += 1) {
        uint64 bits = tryu_next_random(&state) & 0x7fdfffffffffffffull;
        double value = test_ryu_double_from_bits(bits);

        total_len += d2fixed_buffered_n(value, 6, buffer);
    }
    t1 = time_monotonic_now();
    fmt_printf("ryu bench d2fixed_buffered_n: %lld ns/op sink=%lld\n",
               (llong)((t1 - t0) / nitems), (llong)total_len);
    return;
}

static void
tryu_benchmark_d2exp(int32 nitems) {
    uint64 state = 0x082efa98ec4e6c89ull;
    char buffer[128];
    int64 t0;
    int64 t1;
    int64 total_len = 0;

    t0 = time_monotonic_now();
    for (int32 i = 0; i < nitems; i += 1) {
        uint64 bits = tryu_next_random(&state) & 0x7fefffffffffffffull;
        double value = test_ryu_double_from_bits(bits);

        total_len += d2exp_buffered_n(value, 6, buffer);
    }
    t1 = time_monotonic_now();
    fmt_printf("ryu bench d2exp_buffered_n: %lld ns/op sink=%lld\n",
               (llong)((t1 - t0) / nitems), (llong)total_len);
    return;
}

static void
tryu_benchmark_s2d_fast(int32 nitems) {
    static char *inputs[] = {
        "0", "-0", "1.25", "3.141592653589793E0",
        "1.7976931348623157E308", "5E-324",
        "2.2250738585072014E-308", "123456789.125e-7",
    };
    double value;
    int64 t0;
    int64 t1;
    int64 total_used = 0;

    t0 = time_monotonic_now();
    for (int32 i = 0; i < nitems; i += 1) {
        int32 index = i%LENGTH(inputs);

        total_used += s2d_fast(inputs[index], &value);
    }
    t1 = time_monotonic_now();
    fmt_printf("ryu bench s2d_fast: %lld ns/op sink=%lld\n",
               (llong)((t1 - t0) / nitems), (llong)total_used);
    return;
}

static void
tryu_run_optional_benchmarks(void) {
    int32 nitems = 200000;

    if (!getenv("RYU_BENCHMARK")) {
        return;
    }

#if CC_CLANG
    fmt_printf("ryu bench compiler: clang\n");
#elif CC_GCC
    fmt_printf("ryu bench compiler: gcc\n");
#elif CC_TCC
    fmt_printf("ryu bench compiler: tcc\n");
#else
    fmt_printf("ryu bench compiler: unknown\n");
#endif
    fmt_printf("ryu bench sizes: float=%lld double=%lld long_double=%lld\n",
               (llong)SIZEOF(float), (llong)SIZEOF(double),
               (llong)SIZEOF(long double));
#if defined(__SIZEOF_INT128__)
    fmt_printf("ryu bench uint128: available\n");
#else
    fmt_printf("ryu bench uint128: unavailable\n");
#endif

    tryu_benchmark_d2s(nitems);
    tryu_benchmark_f2s(nitems);
    tryu_benchmark_d2fixed(nitems / 20);
    tryu_benchmark_d2exp(nitems / 20);
    tryu_benchmark_s2d_fast(nitems);
    return;
}

int
main(void) {
    char buffer[2000];
    int32 len;

    len = d2s_buffered_n(0.1, buffer);
    test_ryu_result(buffer, len, "1E-1");

    len = f2s_buffered_n(0.1f, buffer);
    test_ryu_result(buffer, len, "1E-1");

    len = d2fixed_buffered_n(1.25, 2, buffer);
    test_ryu_result(buffer, len, "1.25");

    len = d2exp_buffered_n(1234.0, 2, buffer);
    test_ryu_result(buffer, len, "1.23e+03");

    len = d2exp_buffered_n(1e100, 0, buffer);
    test_ryu_result(buffer, len, "1e+100");

    len = d2exp_buffered_n(test_ryu_double_from_bits(1), 0, buffer);
    test_ryu_result(buffer, len, "5e-324");

    test_ryu_round_shift_boundaries();
    test_ryu_s2d_leading_plus();
    test_ryu_s2d_range_values();
    test_ryu_s2d_hex();
    test_ryu_s2d_long_decimal();
    test_ryu_s2d_decimal_interval();
    test_ryu_s2d_fast();
    tryu_test_reference_format_outputs();
    tryu_test_s2d_reference_outputs();
    tryu_test_output_buffer_boundaries();
    tryu_test_deterministic_double_roundtrip();
    tryu_test_deterministic_float_roundtrip();
    tryu_s2f_reference_outputs();
    tryu_s2f_bounded_inputs();
    tryu_s2f_deterministic_roundtrip();
    tryu_run_optional_benchmarks();

    exit(EXIT_SUCCESS);
}
#endif /* TESTING_ryu */

#endif /* RYU_C */
