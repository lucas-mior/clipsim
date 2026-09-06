// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(RYU_INCLUDE_C)
#define RYU_INCLUDE_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_ryu_include 1
#elif !defined(TESTING_ryu_include)
#define TESTING_ryu_include 0
#endif

#if TESTING_ryu_include
#define CBASE_IMPLEMENT
#include "ryu.h"

static void
test_ryu_include_assert(bool condition) {
    if (!condition) {
        exit(EXIT_FAILURE);
    }

    return;
}

static void
test_ryu_include_result(char *actual, int actual_len, char *expected) {
    test_ryu_include_assert(actual_len > 0);

    actual[actual_len] = '\0';
    for (int i = 0; i < actual_len; i += 1) {
        test_ryu_include_assert(actual[i] == expected[i]);
    }
    test_ryu_include_assert(expected[actual_len] == '\0');

    return;
}

int
main(void) {
    char buffer[2000];
    int len;

    len = d2s_buffered_n(0.1, buffer);
    test_ryu_include_result(buffer, len, "1E-1");

    len = f2s_buffered_n(0.1f, buffer);
    test_ryu_include_result(buffer, len, "1E-1");

    len = d2fixed_buffered_n(1.25, 2, buffer);
    test_ryu_include_result(buffer, len, "1.25");

    len = d2exp_buffered_n(1234.0, 2, buffer);
    test_ryu_include_result(buffer, len, "1.23e+03");

    exit(EXIT_SUCCESS);
}
#endif

#endif /* RYU_INCLUDE_C */
