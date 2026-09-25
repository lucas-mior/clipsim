// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(STRTONUM_C)
#define STRTONUM_C

#if !defined(TESTING_strtonum)
#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_strtonum 1
#else
#define TESTING_strtonum 0
#endif
#endif

#include "cbase.h"

static int32
integer_digit_value(char c) {
    if ((c >= '0') && (c <= '9')) {
        return c - '0';
    }
    if ((c >= 'a') && (c <= 'f')) {
        return c - 'a' + 10;
    }
    if ((c >= 'A') && (c <= 'F')) {
        return c - 'A' + 10;
    }
    return -1;
}

// High level, returns the number of parsed bytes or negative on failure.
// str_len is the maximum number of bytes that may be read.
// Non-decimal integers use 0b, 0o, or 0x prefixes.
int32
parse_integer(char *str, int32 str_len, llong *result) {
    int32 i = 0;
    int32 base = 10;
    llong value = 0;
    llong limit = -LLONG_MAX;
    bool negative = false;
    bool has_digit = false;

    if ((str == NULL) || (result == NULL) || (str_len < 0)) {
        return -EINVAL;
    }

    while ((i < str_len)
           && ((str[i] == ' ') || (str[i] == '\f') || (str[i] == '\n')
               || (str[i] == '\r') || (str[i] == '\t')
               || (str[i] == '\v'))) {
        i += 1;
    }

    if ((i < str_len) && ((str[i] == '-') || (str[i] == '+'))) {
        negative = str[i] == '-';
        if (negative) {
            limit = LLONG_MIN;
        }
        i += 1;
    }

    if ((str_len - i >= 2) && (str[i] == '0')) {
        switch (str[i + 1]) {
        case 'b':
        case 'B':
            base = 2;
            i += 2;
            break;
        case 'o':
        case 'O':
            base = 8;
            i += 2;
            break;
        case 'x':
        case 'X':
            base = 16;
            i += 2;
            break;
        default:
            break;
        }
    }

    while (i < str_len) {
        int32 digit = integer_digit_value(str[i]);

        if ((digit < 0) || (digit >= base)) {
            break;
        }

        has_digit = true;
        if (value < (limit + digit)/base) {
            return -ERANGE;
        }
        value = value*base - digit;
        i += 1;
    }

    if (!has_digit) {
        return -EINVAL;
    }

    if (negative) {
        *result = value;
    } else {
        *result = -value;
    }
    return i;
}

typedef struct AtoiLimit {
    llong quotient;
    int32 remainder;
    llong overflow_result;
} AtoiLimit;

#define ATOI_POSITIVE_LIMIT_INDEX 0
#define ATOI_NEGATIVE_LIMIT_INDEX 1
#define ATOI_LIMITS_PER_BASE 2

static const AtoiLimit atoi_limits[][ATOI_LIMITS_PER_BASE] = {
    [2] = {
        [ATOI_POSITIVE_LIMIT_INDEX] = {
            .quotient = -LLONG_MAX/2,
            .remainder = LLONG_MAX%2,
            .overflow_result = LLONG_MAX,
        },
        [ATOI_NEGATIVE_LIMIT_INDEX] = {
            .quotient = LLONG_MIN/2,
            .remainder = -(LLONG_MIN%2),
            .overflow_result = LLONG_MIN,
        },
    },
    [8] = {
        [ATOI_POSITIVE_LIMIT_INDEX] = {
            .quotient = -LLONG_MAX/8,
            .remainder = LLONG_MAX%8,
            .overflow_result = LLONG_MAX,
        },
        [ATOI_NEGATIVE_LIMIT_INDEX] = {
            .quotient = LLONG_MIN/8,
            .remainder = -(LLONG_MIN%8),
            .overflow_result = LLONG_MIN,
        },
    },
    [10] = {
        [ATOI_POSITIVE_LIMIT_INDEX] = {
            .quotient = -LLONG_MAX/10,
            .remainder = LLONG_MAX%10,
            .overflow_result = LLONG_MAX,
        },
        [ATOI_NEGATIVE_LIMIT_INDEX] = {
            .quotient = LLONG_MIN/10,
            .remainder = -(LLONG_MIN%10),
            .overflow_result = LLONG_MIN,
        },
    },
    [16] = {
        [ATOI_POSITIVE_LIMIT_INDEX] = {
            .quotient = -LLONG_MAX/16,
            .remainder = LLONG_MAX%16,
            .overflow_result = LLONG_MAX,
        },
        [ATOI_NEGATIVE_LIMIT_INDEX] = {
            .quotient = LLONG_MIN/16,
            .remainder = -(LLONG_MIN%16),
            .overflow_result = LLONG_MIN,
        },
    },
};

static bool
atoi_would_overflow(llong value, int32 digit, AtoiLimit limit) {
    if (value < limit.quotient) {
        return true;
    }
    if ((value == limit.quotient) && (digit > limit.remainder)) {
        return true;
    }
    return false;
}

static int32
atoi_detect_base(char *str, int32 *i) {
    int32 base = 10;

    if (str[*i] != '0') {
        return base;
    }

    switch (str[*i + 1]) {
    case 'b':
    case 'B':
        base = 2;
        *i += 2;
        break;
    case 'o':
    case 'O':
        base = 8;
        *i += 2;
        break;
    case 'x':
    case 'X':
        base = 16;
        *i += 2;
        break;
    default:
        break;
    }

    return base;
}

static llong
atoi_impl(char *str, int32 str_len, bool detect_base, bool saturate) {
    int32 i = 0;
    int32 base = 10;
    int32 limit_index = ATOI_POSITIVE_LIMIT_INDEX;
    llong value = 0;
    AtoiLimit limit;
    bool negative = false;

    if ((str == NULL) || (str_len <= 0)) {
        return 0;
    }

    if ((str[i] == '-') || (str[i] == '+')) {
        negative = str[i] == '-';
        if (negative) {
            limit_index = ATOI_NEGATIVE_LIMIT_INDEX;
        }
        i += 1;
    }

    if (detect_base) {
        base = atoi_detect_base(str, &i);
    }

    limit = atoi_limits[base][limit_index];

    while (true) {
        int32 digit = integer_digit_value(str[i]);

        if ((digit < 0) || (digit >= base)) {
            break;
        }

        if (saturate) {
            if (atoi_would_overflow(value, digit, limit)) {
                return limit.overflow_result;
            }
        } else if (DEBUGGING) {
            if (atoi_would_overflow(value, digit, limit)) {
                TRAP("overflow");
            }
        }
        value = value*base - digit;
        i += 1;
    }

    if (negative) {
        return value;
    }
    return -value;
}

// low level without error checking, returns 0 on invalid input.
// only to be used in the following situations:
// - when the string was pre-parsed,
//   so we know that it will not get invalid input
// - or when the caller only needs positive values;
// - or when the caller only needs non-zero values;
//   in this case, zero is used as one of:
//   - "don't use this number"
//   - "do 0 actions of this thing"
//   - "do this forever, don't limit it"
llong
atoi2(char *str, int32 str_len) {
    return atoi_impl(str, str_len, false, false);
}

// Like atoi2, but detects 0b, 0o, and 0x prefixes.
llong
atoi_base(char *str, int32 str_len) {
    return atoi_impl(str, str_len, true, false);
}

// Like atoi2, but saturates on overflow instead of trapping.
llong
atoi2sat(char *str, int32 str_len) {
    return atoi_impl(str, str_len, false, true);
}

// Like atoi_base, but saturates on overflow instead of trapping.
llong
atoi_base_sat(char *str, int32 str_len) {
    return atoi_impl(str, str_len, true, true);
}

#undef ATOI_LIMITS_PER_BASE
#undef ATOI_NEGATIVE_LIMIT_INDEX
#undef ATOI_POSITIVE_LIMIT_INDEX

bool
util_is_integer(char *string) {
    char c;

    while ((c = *string)) {
        if (!isdigit(c)) {
            return false;
        }
        string += 1;
    }

    return true;
}

#if 0 == TESTING_strtonum
static inline void
strtonum_functions_sink(void) {
    (void)strtonum_functions_sink;
    (void)parse_integer;
    (void)atoi2;
    (void)atoi_base;
    (void)atoi2sat;
    (void)atoi_base_sat;
    (void)util_is_integer;
    return;
}
#endif

#if TESTING_strtonum
#define CBASE_IMPLEMENT
#include "cbase.h"

int
main(void) {
    llong result;

    ASSERT_EQUAL(parse_integer(STRLIT("0"), &result), 1);
    ASSERT_EQUAL(result, 0);
    ASSERT_EQUAL(parse_integer(STRLIT("  +123  "), &result), 6);
    ASSERT_EQUAL(result, 123);
    ASSERT_EQUAL(parse_integer(STRLIT("0b101010"), &result), 8);
    ASSERT_EQUAL(result, 42);
    ASSERT_EQUAL(parse_integer(STRLIT("-0B101010"), &result), 9);
    ASSERT_EQUAL(result, -42);
    ASSERT_EQUAL(parse_integer(STRLIT("0o755"), &result), 5);
    ASSERT_EQUAL(result, 493);
    ASSERT_EQUAL(parse_integer(STRLIT("+0O17"), &result), 5);
    ASSERT_EQUAL(result, 15);
    ASSERT_EQUAL(parse_integer(STRLIT("0x7f"), &result), 4);
    ASSERT_EQUAL(result, 127);
    ASSERT_EQUAL(parse_integer(STRLIT("-0X7F"), &result), 5);
    ASSERT_EQUAL(result, -127);
    ASSERT_EQUAL(parse_integer(STRLIT("0123"), &result), 4);
    ASSERT_EQUAL(result, 123);
    ASSERT_EQUAL(parse_integer(
                     STRLIT("0b1111111111111111111111111111111"
                            "11111111111111111111111111111111"),
                     &result),
                 65);
    ASSERT_EQUAL(result, LLONG_MAX);
    ASSERT_EQUAL(parse_integer(
                     STRLIT("-0b1000000000000000000000000000000"
                            "000000000000000000000000000000000"),
                     &result),
                 67);
    ASSERT_EQUAL(result, LLONG_MIN);
    ASSERT_EQUAL(parse_integer(STRLIT("0o777777777777777777777"), &result),
                 23);
    ASSERT_EQUAL(result, LLONG_MAX);
    ASSERT_EQUAL(parse_integer(STRLIT("-0o1000000000000000000000"), &result),
                 25);
    ASSERT_EQUAL(result, LLONG_MIN);
    ASSERT_EQUAL(parse_integer(STRLIT("0x7fffffffffffffff"), &result), 18);
    ASSERT_EQUAL(result, LLONG_MAX);
    ASSERT_EQUAL(parse_integer(STRLIT("-0x8000000000000000"), &result), 19);
    ASSERT_EQUAL(result, LLONG_MIN);

    ASSERT_EQUAL(parse_integer(STRLIT(""), &result), -EINVAL);
    ASSERT_EQUAL(parse_integer(STRLIT("0x"), &result), -EINVAL);
    ASSERT_EQUAL(parse_integer(STRLIT("0o8"), &result), -EINVAL);
    ASSERT_EQUAL(parse_integer(STRLIT("9223372036854775808"), &result),
                 -ERANGE);
    ASSERT_EQUAL(parse_integer(STRLIT("-9223372036854775809"), &result),
                 -ERANGE);
    ASSERT_EQUAL(parse_integer(
                     STRLIT("0b10000000000000000000000000000000"
                            "00000000000000000000000000000000"),
                     &result),
                 -ERANGE);
    ASSERT_EQUAL(parse_integer(STRLIT("0o1000000000000000000000"), &result),
                 -ERANGE);
    ASSERT_EQUAL(parse_integer(STRLIT("0x8000000000000000"), &result),
                 -ERANGE);
    ASSERT_EQUAL(parse_integer(STRLIT("-0x8000000000000001"), &result),
                 -ERANGE);

    ASSERT_EQUAL(parse_integer("123xyz", 6, &result), 3);
    ASSERT_EQUAL(result, 123);
    ASSERT_EQUAL(parse_integer("12345", 3, &result), 3);
    ASSERT_EQUAL(result, 123);
    ASSERT_EQUAL(parse_integer("123", 100, &result), 3);
    ASSERT_EQUAL(result, 123);
    ASSERT_EQUAL(parse_integer("0x7fZZ", 6, &result), 4);
    ASSERT_EQUAL(result, 127);
    ASSERT_EQUAL(parse_integer("0b102", 5, &result), 4);
    ASSERT_EQUAL(result, 2);
    ASSERT_EQUAL(parse_integer("0x7f", 1, &result), 1);
    ASSERT_EQUAL(result, 0);
    ASSERT_EQUAL(parse_integer("0x7f", 2, &result), -EINVAL);
    ASSERT_EQUAL(parse_integer("  -42 rest", 10, &result), 5);
    ASSERT_EQUAL(result, -42);
    ASSERT_EQUAL(parse_integer("123", 0, &result), -EINVAL);

    ASSERT_EQUAL(atoi2("-123x", 4), -123);
    ASSERT_EQUAL(atoi2("42", 0), 0);
    ASSERT_EQUAL(atoi2(STRLIT("9223372036854775807")), LLONG_MAX);
    ASSERT_EQUAL(atoi2(STRLIT("-9223372036854775808")), LLONG_MIN);
#if OS_UNIX
    ASSERT_TRAPS(atoi2(STRLIT("9223372036854775808")));
    ASSERT_TRAPS(atoi2(STRLIT("-9223372036854775809")));
    ASSERT_TRAPS(atoi2(STRLIT("99999999999999999999999999")));
    ASSERT_TRAPS(atoi2(STRLIT("-1111111111111111111111111")));
#endif

    ASSERT_EQUAL(atoi2sat("-123x", 4), -123);
    ASSERT_EQUAL(atoi2sat("42", 0), 0);
    ASSERT_EQUAL(atoi2sat(STRLIT("9223372036854775807")), LLONG_MAX);
    ASSERT_EQUAL(atoi2sat(STRLIT("-9223372036854775808")), LLONG_MIN);
    ASSERT_EQUAL(atoi2sat(STRLIT("9223372036854775808")), LLONG_MAX);
    ASSERT_EQUAL(atoi2sat(STRLIT("-9223372036854775809")), LLONG_MIN);
    ASSERT_EQUAL(atoi2sat(STRLIT("999999999999999999999999999999")),
                 LLONG_MAX);
    ASSERT_EQUAL(atoi2sat(STRLIT("-999999999999999999999999999999")),
                 LLONG_MIN);

    ASSERT_EQUAL(atoi_base("-123x", 4), -123);
    ASSERT_EQUAL(atoi_base("42", 0), 0);
    ASSERT_EQUAL(atoi_base(STRLIT("123")), 123);
    ASSERT_EQUAL(atoi_base(STRLIT("0123")), 123);
    ASSERT_EQUAL(atoi_base(STRLIT("0b101010")), 42);
    ASSERT_EQUAL(atoi_base(STRLIT("-0B101010")), -42);
    ASSERT_EQUAL(atoi_base(STRLIT("0o755")), 493);
    ASSERT_EQUAL(atoi_base(STRLIT("+0O17")), 15);
    ASSERT_EQUAL(atoi_base(STRLIT("0x7f")), 127);
    ASSERT_EQUAL(atoi_base(STRLIT("-0X7F")), -127);
    ASSERT_EQUAL(atoi_base(STRLIT("0b102")), 2);
    ASSERT_EQUAL(atoi_base(STRLIT("0o8")), 0);
    ASSERT_EQUAL(atoi_base(STRLIT("0x")), 0);
    ASSERT_EQUAL(atoi_base(STRLIT("0x7fffffffffffffff")), LLONG_MAX);
    ASSERT_EQUAL(atoi_base(STRLIT("-0x8000000000000000")), LLONG_MIN);
    ASSERT_EQUAL(atoi_base(STRLIT("0b1111111111111111111111111111111"
                                  "11111111111111111111111111111111")),
                 LLONG_MAX);
    ASSERT_EQUAL(atoi_base(STRLIT("-0b1000000000000000000000000000000"
                                  "000000000000000000000000000000000")),
                 LLONG_MIN);
#if OS_UNIX
    ASSERT_TRAPS(atoi_base(STRLIT("0x8000000000000000")));
    ASSERT_TRAPS(atoi_base(STRLIT("-0x8000000000000001")));
    ASSERT_TRAPS(atoi_base(STRLIT("0xffffffffffffffff")));
#endif

    ASSERT_EQUAL(atoi_base_sat("-123x", 4), -123);
    ASSERT_EQUAL(atoi_base_sat("42", 0), 0);
    ASSERT_EQUAL(atoi_base_sat(STRLIT("0b101010")), 42);
    ASSERT_EQUAL(atoi_base_sat(STRLIT("-0B101010")), -42);
    ASSERT_EQUAL(atoi_base_sat(STRLIT("0o755")), 493);
    ASSERT_EQUAL(atoi_base_sat(STRLIT("+0O17")), 15);
    ASSERT_EQUAL(atoi_base_sat(STRLIT("0x7f")), 127);
    ASSERT_EQUAL(atoi_base_sat(STRLIT("-0X7F")), -127);
    ASSERT_EQUAL(atoi_base_sat(STRLIT("0x7fffffffffffffff")), LLONG_MAX);
    ASSERT_EQUAL(atoi_base_sat(STRLIT("-0x8000000000000000")),
                 LLONG_MIN);
    ASSERT_EQUAL(atoi_base_sat(STRLIT("0x8000000000000000")), LLONG_MAX);
    ASSERT_EQUAL(atoi_base_sat(STRLIT("-0x8000000000000001")),
                 LLONG_MIN);
    ASSERT_EQUAL(atoi_base_sat(STRLIT("0xffffffffffffffff")), LLONG_MAX);
    ASSERT_EQUAL(atoi_base_sat(STRLIT("-0xffffffffffffffff")),
                 LLONG_MIN);

    ASSERT(util_is_integer(""));
    ASSERT(util_is_integer("0123456789"));
    ASSERT(!util_is_integer("-1"));
    ASSERT(!util_is_integer("12x"));

    exit(EXIT_SUCCESS);
}

#endif

#endif /* STRTONUM_C */
