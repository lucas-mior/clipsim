// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(FORMAT_C)
#define FORMAT_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_format 1
#elif !defined(TESTING_format)
#define TESTING_format 0
#endif

#include "cbase.h"

#if TESTING_format && !defined(CBASE_IMPLEMENT)
#define CBASE_IMPLEMENT
#include "cbase.h"
#endif

#include "ryu.h"

enum {
    FORMAT_FLOAT_RYU_BUFFER_SIZE = 2000,
    FORMAT_FLOAT_MAX_PRECISION = 1024,
    FORMAT_FLOAT_MAX_FIXED_PREFIX = 312,
    FORMAT_FLOAT_MAX_EXP_PREFIX = 8,
};

_Static_assert(FORMAT_FLOAT_MAX_FIXED_PREFIX
               + FORMAT_FLOAT_MAX_PRECISION < FORMAT_FLOAT_RYU_BUFFER_SIZE,
               "format fixed temporary buffer is too small");
_Static_assert(FORMAT_FLOAT_MAX_EXP_PREFIX
               + FORMAT_FLOAT_MAX_PRECISION < FORMAT_FLOAT_RYU_BUFFER_SIZE,
               "format scientific temporary buffer is too small");

static int32
format_float_validate_buffer(char *buffer, int64 capacity) {
    if (buffer == NULL) {
        return -EINVAL;
    }
    if (capacity <= 0) {
        return -EINVAL;
    }

    return 0;
}

static int32
format_float_validate_precision(int32 precision) {
    if (precision < 0) {
        return -EINVAL;
    }
    if (precision > FORMAT_FLOAT_MAX_PRECISION) {
        return -ERANGE;
    }

    return 0;
}

static int32
format_float_copy(char *buffer, int64 capacity,
                  char *source, int32 source_len) {
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT(source != NULL);
    ASSERT_NON_NEGATIVE(source_len);
    ASSERT_LESS(source_len, FORMAT_FLOAT_RYU_BUFFER_SIZE);

    if ((int64)source_len >= capacity) {
        return -ENOSPC;
    }

    memcpy(buffer, source, (size_t)source_len);
    buffer[source_len] = '\0';
    return source_len;
}

int32
format_float32_shortest(char *buffer, int64 capacity, float value) {
    int32 status;
    int32 len;
    char temp[FORMAT_FLOAT_RYU_BUFFER_SIZE];

    if ((status = format_float_validate_buffer(buffer, capacity)) < 0) {
        return status;
    }

    len = (int32)f2s_buffered_n(value, temp);
    return format_float_copy(buffer, capacity, temp, len);
}

int32
format_float64_shortest(char *buffer, int64 capacity, double value) {
    int32 status;
    int32 len;
    char temp[FORMAT_FLOAT_RYU_BUFFER_SIZE];

    if ((status = format_float_validate_buffer(buffer, capacity)) < 0) {
        return status;
    }

    len = (int32)d2s_buffered_n(value, temp);
    return format_float_copy(buffer, capacity, temp, len);
}

int32
format_float64_fixed(char *buffer, int64 capacity, double value,
                     int32 precision) {
    int32 status;
    int32 len;
    char temp[FORMAT_FLOAT_RYU_BUFFER_SIZE];

    if ((status = format_float_validate_buffer(buffer, capacity)) < 0) {
        return status;
    }
    if ((status = format_float_validate_precision(precision)) < 0) {
        return status;
    }

    len = (int32)d2fixed_buffered_n(value, (uint32_t)precision, temp);
    return format_float_copy(buffer, capacity, temp, len);
}

int32
format_float64_scientific(char *buffer, int64 capacity, double value,
                          int32 precision) {
    int32 status;
    int32 len;
    char temp[FORMAT_FLOAT_RYU_BUFFER_SIZE];

    if ((status = format_float_validate_buffer(buffer, capacity)) < 0) {
        return status;
    }
    if ((status = format_float_validate_precision(precision)) < 0) {
        return status;
    }

    len = (int32)d2exp_buffered_n(value, (uint32_t)precision, temp);
    return format_float_copy(buffer, capacity, temp, len);
}

#if TESTING_format
static void
test_format_float32_shortest(float value, char *expected) {
    char buffer[FORMAT_FLOAT_RYU_BUFFER_SIZE];
    int32 len;

    len = format_float32_shortest(buffer, SIZEOF(buffer), value);
    ASSERT_EQUAL(len, strlen32(expected));
    ASSERT_EQUAL(buffer, expected);

    return;
}

static void
test_format_float64_shortest(double value, char *expected) {
    char buffer[FORMAT_FLOAT_RYU_BUFFER_SIZE];
    int32 len;

    len = format_float64_shortest(buffer, SIZEOF(buffer), value);
    ASSERT_EQUAL(len, strlen32(expected));
    ASSERT_EQUAL(buffer, expected);

    return;
}

static void
test_format_float64_fixed(double value, int32 precision, char *expected) {
    char buffer[FORMAT_FLOAT_RYU_BUFFER_SIZE];
    int32 len;

    len = format_float64_fixed(buffer, SIZEOF(buffer), value, precision);
    ASSERT_EQUAL(len, strlen32(expected));
    ASSERT_EQUAL(buffer, expected);

    return;
}

static void
test_format_float64_scientific(double value, int32 precision, char *expected) {
    char buffer[FORMAT_FLOAT_RYU_BUFFER_SIZE];
    int32 len;

    len = format_float64_scientific(buffer, SIZEOF(buffer), value, precision);
    ASSERT_EQUAL(len, strlen32(expected));
    ASSERT_EQUAL(buffer, expected);

    return;
}

static uint32
test_format_float32_bits(float value) {
    uint32 bits;

    memcpy(&bits, &value, SIZEOF(bits));
    return bits;
}

static uint64
test_format_float64_bits(double value) {
    uint64 bits;

    memcpy(&bits, &value, SIZEOF(bits));
    return bits;
}

static void
test_format_float32_round_trip(float value) {
    char buffer[FORMAT_FLOAT_RYU_BUFFER_SIZE];
    char *end;
    float parsed;
    int32 len;

    len = format_float32_shortest(buffer, SIZEOF(buffer), value);
    ASSERT_POSITIVE(len);

    end = NULL;
    parsed = strtof(buffer, &end);
    ASSERT(end == buffer + len);
    ASSERT_EQUAL(test_format_float32_bits(parsed),
                 test_format_float32_bits(value));

    return;
}

static void
test_format_float64_round_trip(double value) {
    char buffer[FORMAT_FLOAT_RYU_BUFFER_SIZE];
    char *end;
    double parsed;
    int32 len;

    len = format_float64_shortest(buffer, SIZEOF(buffer), value);
    ASSERT_POSITIVE(len);

    end = NULL;
    parsed = strtod(buffer, &end);
    ASSERT(end == buffer + len);
    ASSERT_EQUAL(test_format_float64_bits(parsed),
                 test_format_float64_bits(value));

    return;
}

int
main(void) {
    char buffer[16];

    test_format_float64_shortest(0.0, "0E0");
    test_format_float64_shortest(-0.0, "-0E0");
    test_format_float64_shortest(1.0, "1E0");
    test_format_float64_shortest(0.1, "1E-1");
    test_format_float64_shortest(1234567.89, "1.23456789E6");
    test_format_float64_shortest(1e-7, "1E-7");

    test_format_float32_shortest(0.0f, "0E0");
    test_format_float32_shortest(-0.0f, "-0E0");
    test_format_float32_shortest(1.0f, "1E0");
    test_format_float32_shortest(0.1f, "1E-1");
    test_format_float32_shortest(1e-7f, "1E-7");

    test_format_float64_fixed(1.25, 2, "1.25");
    test_format_float64_fixed(1.2, 4, "1.2000");
    test_format_float64_fixed(-0.0, 3, "-0.000");

    test_format_float64_scientific(1234.0, 2, "1.23e+03");
    test_format_float64_scientific(0.00123, 3, "1.230e-03");

    ASSERT_EQUAL(format_float64_shortest(NULL, 64, 1.0), -EINVAL);
    ASSERT_EQUAL(format_float64_shortest(buffer, 0, 1.0), -EINVAL);
    ASSERT_EQUAL(format_float64_fixed(buffer, SIZEOF(buffer), 1.0, -1),
                 -EINVAL);
    ASSERT_EQUAL(format_float64_fixed(buffer, 4, 1.25, 2), -ENOSPC);
    ASSERT_EQUAL(format_float64_fixed(buffer, SIZEOF(buffer), 1.0,
                                      FORMAT_FLOAT_MAX_PRECISION + 1),
                 -ERANGE);

    test_format_float64_round_trip(0.1);
    test_format_float32_round_trip(0.1f);

    exit(EXIT_SUCCESS);
}
#endif

#endif /* FORMAT_C */
