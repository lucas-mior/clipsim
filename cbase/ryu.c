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
    ASSERT_EQ(used, 23);
    ASSERT_EQ(test_ryu_double_bits(value), 0x0000000000000001ull);

    used = s2d("0x0.fffffffffffffp-1022", &value);
    ASSERT_EQ(used, 23);
    ASSERT_EQ(test_ryu_double_bits(value), 0x000fffffffffffffull);

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
    ASSERT_EQ(used, 9);
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
        ASSERT_EQ(used, len);
        ASSERT(test_ryu_double_bits(value) == cases[i]);
    }
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

    test_ryu_s2d_hex();

    exit(EXIT_SUCCESS);
}
#endif /* TESTING_ryu */

#endif /* RYU_C */
