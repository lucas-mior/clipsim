// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(NUMTOSTR_C)
#define NUMTOSTR_C

#if !defined(TESTING_numtostr)
#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_numtostr 1
#else
#define TESTING_numtostr 0
#endif
#endif

#include "cbase.h"

#if !defined(EOVERFLOW)
#define EOVERFLOW ERANGE
#endif

enum {
    NUMTOSTR_FLOAT_RYU_BUFFER_SIZE = 2000,
    NUMTOSTR_FLOAT_MAX_PRECISION = 1024,
};

int32
itoa2(char *buffer, int32 size, llong num) {
    ullong magnitude;
    int i = 0;
    bool negative = false;

    ASSERT_MORE_EQUAL(size, 22);

    if (num < 0) {
        negative = true;
        magnitude = (ullong)(-(num + 1)) + 1;
    } else {
        magnitude = (ullong)num;
    }

    do {
        buffer[i] = (char)(magnitude % 10 + '0');
        i += 1;
        magnitude /= 10;
    } while (magnitude > 0);

    if (negative) {
        buffer[i] = '-';
        i += 1;
    }

    buffer[i] = '\0';

    for (long j = 0; j < i / 2; j += 1) {
        char temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }

    return i;
}

static int32
fmt_float_validate_buffer(char *buffer, int64 capacity) {
    if (buffer == NULL) {
        return -EINVAL;
    }
    if (capacity <= 0) {
        return -EINVAL;
    }

    return 0;
}

static int32
fmt_float_validate_precision(int32 precision) {
    if (precision < 0) {
        return -EINVAL;
    }
    if (precision > NUMTOSTR_FLOAT_MAX_PRECISION) {
        return -ERANGE;
    }

    return 0;
}

static int32
fmt_float_copy(char *buffer, int64 capacity, char *source, int32 source_len) {
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT(source != NULL);
    ASSERT_NON_NEGATIVE(source_len);
    ASSERT_LESS(source_len, NUMTOSTR_FLOAT_RYU_BUFFER_SIZE);

    if ((int64)source_len >= capacity) {
        return -ENOSPC;
    }

    memcpy64(buffer, source, source_len);
    buffer[source_len] = '\0';
    return source_len;
}

int32
fmt_float32_shortest(char *buffer, int64 capacity, float value) {
    int32 status;
    int32 len;
    char temp[NUMTOSTR_FLOAT_RYU_BUFFER_SIZE];

    if ((status = fmt_float_validate_buffer(buffer, capacity)) < 0) {
        return status;
    }

    len = f2s_buffered_n(value, temp);
    return fmt_float_copy(buffer, capacity, temp, len);
}

int32
fmt_float64_shortest(char *buffer, int64 capacity, double value) {
    int32 status;
    int32 len;
    char temp[NUMTOSTR_FLOAT_RYU_BUFFER_SIZE];

    if ((status = fmt_float_validate_buffer(buffer, capacity)) < 0) {
        return status;
    }

    len = d2s_buffered_n(value, temp);
    return fmt_float_copy(buffer, capacity, temp, len);
}

int32
fmt_float64_fixed(char *buffer, int64 capacity, double value, int32 precision) {
    int32 status;
    int32 len;
    char temp[NUMTOSTR_FLOAT_RYU_BUFFER_SIZE];

    if ((status = fmt_float_validate_buffer(buffer, capacity)) < 0) {
        return status;
    }
    if ((status = fmt_float_validate_precision(precision)) < 0) {
        return status;
    }

    len = d2fixed_buffered_n(value, (uint32)precision, temp);
    return fmt_float_copy(buffer, capacity, temp, len);
}

int32
fmt_float64_scientific(char *buffer, int64 capacity,
                       double value, int32 precision) {
    int32 status;
    int32 len;
    char temp[NUMTOSTR_FLOAT_RYU_BUFFER_SIZE];

    if ((status = fmt_float_validate_buffer(buffer, capacity)) < 0) {
        return status;
    }
    if ((status = fmt_float_validate_precision(precision)) < 0) {
        return status;
    }

    len = d2exp_buffered_n(value, (uint32)precision, temp);
    return fmt_float_copy(buffer, capacity, temp, len);
}

int32
bytes_pretty(char *buffer, int64 raw) {
    char *suffixes[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    double aux_pretty;
    int64 i;
    int32 precision;
    int32 n;
    int32 suffix_len;
    char *suffix;

    if (raw < 0) {
        *buffer = '\0';
        return 0;
    }

    if (raw <= 1023) {
        n = itoa2(buffer, 22, raw);
        buffer[n] = 'B';
        n += 1;
        buffer[n] = '\0';
        return n;
    }

    aux_pretty = (double)raw;
    i = 0;
    while ((aux_pretty >= 1024.0) && (i < LENGTH(suffixes))) {
        aux_pretty /= 1024.0;
        i += 1;
    }

    if (aux_pretty >= 1000) {
        precision = 1;
    } else if (aux_pretty >= 100) {
        precision = 2;
    } else if (aux_pretty >= 10) {
        precision = 3;
    } else {
        precision = 4;
    }

    suffix = suffixes[i];
    suffix_len = strlen32(suffix);
    n = fmt_float64_fixed(buffer, 16 - suffix_len, aux_pretty, precision);
    if ((n < 0) || (n + suffix_len >= 16)) {
        error("Error formatting bytes: %d\n", n);
        fatal(EXIT_FAILURE);
    }

    memcpy64(buffer + n, suffix, suffix_len);
    n += suffix_len;
    buffer[n] = '\0';
    return n;
}

#if 0 == TESTING_numtostr
static inline void
numtostr_functions_sink(void) {
    (void)numtostr_functions_sink;
    (void)itoa2;
    (void)bytes_pretty;
    (void)fmt_float32_shortest;
    (void)fmt_float64_shortest;
    (void)fmt_float64_fixed;
    (void)fmt_float64_scientific;
    return;
}
#endif

#if TESTING_numtostr
#define CBASE_IMPLEMENT
#include "cbase.h"
#include "ryu.h"

static void
test_numtostr_itoa(void) {
    char buffer[32];
    int32 len;

    len = itoa2(buffer, SIZEOF(buffer), 0);
    ASSERT_EQUAL(len, 1);
    ASSERT_EQUAL((char *)buffer, "0");

    len = itoa2(buffer, SIZEOF(buffer), -9223372036854775807LL - 1);
    ASSERT_EQUAL(len, 20);
    ASSERT_EQUAL((char *)buffer, "-9223372036854775808");

    len = itoa2(buffer, SIZEOF(buffer), 9223372036854775807LL);
    ASSERT_EQUAL(len, 19);
    ASSERT_EQUAL((char *)buffer, "9223372036854775807");
    return;
}

static void
test_numtostr_bytes_pretty(void) {
    char buffer[32];
    int32 len;

    len = bytes_pretty(buffer, -1);
    ASSERT_EQUAL(len, 0);
    ASSERT_EQUAL((char *)buffer, "");

    len = bytes_pretty(buffer, 512);
    ASSERT_EQUAL(len, 4);
    ASSERT_EQUAL((char *)buffer, "512B");

    len = bytes_pretty(buffer, 1024);
    ASSERT_EQUAL(len, 8);
    ASSERT_EQUAL((char *)buffer, "1.0000kB");

    len = bytes_pretty(buffer, SIZEMB(2));
    ASSERT_EQUAL(len, 8);
    ASSERT_EQUAL((char *)buffer, "2.0000MB");
    return;
}

static void
test_numtostr_float_buffers(void) {
    char buffer[64];
    int32 len;

    len = fmt_float64_shortest(buffer, SIZEOF(buffer), 0.1);
    ASSERT_EQUAL(len, 4);
    ASSERT_EQUAL((char *)buffer, "1E-1");

    len = fmt_float32_shortest(buffer, SIZEOF(buffer), 0.1f);
    ASSERT_EQUAL(len, 4);
    ASSERT_EQUAL((char *)buffer, "1E-1");

    len = fmt_float64_fixed(buffer, SIZEOF(buffer), 1.25, 2);
    ASSERT_EQUAL(len, 4);
    ASSERT_EQUAL((char *)buffer, "1.25");

    len = fmt_float64_scientific(buffer, SIZEOF(buffer), 1234.0, 2);
    ASSERT_EQUAL(len, 8);
    ASSERT_EQUAL((char *)buffer, "1.23e+03");

    ASSERT_EQUAL(fmt_float64_shortest(NULL, 64, 1.0), -EINVAL);
    ASSERT_EQUAL(fmt_float64_shortest(buffer, 0, 1.0), -EINVAL);
    ASSERT_EQUAL(fmt_float64_fixed(buffer, SIZEOF(buffer), 1.0, -1),
                 -EINVAL);
    ASSERT_EQUAL(fmt_float64_fixed(buffer, SIZEOF(buffer), 1.0, 1025),
                 -ERANGE);
    ASSERT_EQUAL(fmt_float64_fixed(buffer, 4, 1.25, 2), -ENOSPC);
    return;
}

int
main(void) {
    test_numtostr_itoa();
    test_numtostr_bytes_pretty();
    test_numtostr_float_buffers();
    exit(EXIT_SUCCESS);
}

#endif

#endif /* NUMTOSTR_C */
