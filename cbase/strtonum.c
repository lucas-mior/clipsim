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

// high level, returns negative on failure
int32
parse_integer(char *str, int32 str_len, llong *result) {
    int32 i = 0;
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

    while ((i < str_len) && (str[i] >= '0') && (str[i] <= '9')) {
        llong digit = str[i] - '0';

        has_digit = true;
        if (value < (limit + digit)/10) {
            return -ERANGE;
        }
        value = value*10 - digit;
        i += 1;
    }

    if (!has_digit) {
        return -EINVAL;
    }

    while ((i < str_len)
           && ((str[i] == ' ') || (str[i] == '\f') || (str[i] == '\n')
               || (str[i] == '\r') || (str[i] == '\t')
               || (str[i] == '\v'))) {
        i += 1;
    }
    if (i < str_len) {
        return -EINVAL;
    }

    if (negative) {
        *result = value;
    } else {
        *result = -value;
    }
    return 0;
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
    int32 i = 0;
    llong value = 0;
    llong limit = -MAXOF(value);
    bool negative = false;

    if ((str == NULL) || (str_len <= 0)) {
        return 0;
    }

    if ((i < str_len) && ((str[i] == '-') || (str[i] == '+'))) {
        negative = str[i] == '-';
        if (negative) {
            limit = MINOF(value);
        }
        i += 1;
    }

    (void)limit;

    while ((i < str_len) && (str[i] >= '0') && (str[i] <= '9')) {
        llong digit = str[i] - '0';

        if (DEBUGGING) {
            if (value < (limit + digit)/10) {
                TRAP("overflow");
            }
        }
        value = value*10 - digit;
        i += 1;
    }

    if (negative) {
        return value;
    }
    return -value;
}

// Like atoi2, but saturates on overflow instead of trapping.
llong
atoi2sat(char *str, int32 str_len) {
    int32 i = 0;
    llong value = 0;
    llong limit = -MAXOF(value);
    bool negative = false;

    if ((str == NULL) || (str_len <= 0)) {
        return 0;
    }

    if ((i < str_len) && ((str[i] == '-') || (str[i] == '+'))) {
        negative = str[i] == '-';
        if (negative) {
            limit = MINOF(value);
        }
        i += 1;
    }

    while ((i < str_len) && (str[i] >= '0') && (str[i] <= '9')) {
        llong digit = str[i] - '0';

        if (value < (limit + digit)/10) {
            if (negative) {
                return MINOF(value);
            } else {
                return MAXOF(value);
            }
        }
        value = value*10 - digit;
        i += 1;
    }

    if (negative) {
        return value;
    }
    return -value;
}

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
    (void)atoi2sat;
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

    ASSERT_ZERO(parse_integer(STRLIT("0"), &result));
    ASSERT_EQUAL(result, 0);
    ASSERT_ZERO(parse_integer(STRLIT("  +123  "), &result));
    ASSERT_EQUAL(result, 123);
    ASSERT_ZERO(parse_integer(STRLIT("-9223372036854775808"), &result));
    ASSERT_EQUAL(result, LLONG_MIN);
    ASSERT_EQUAL(parse_integer(STRLIT(""), &result), -EINVAL);
    ASSERT_EQUAL(parse_integer(STRLIT("12x"), &result), -EINVAL);
    ASSERT_EQUAL(parse_integer(STRLIT("9223372036854775808"), &result),
                 -ERANGE);
    ASSERT_EQUAL(parse_integer(STRLIT("-9223372036854775809"), &result),
                 -ERANGE);

    ASSERT_EQUAL(atoi2("-123x", 4), -123);
    ASSERT_EQUAL(atoi2("99", 1), 9);
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
    ASSERT_EQUAL(atoi2sat("99", 1), 9);
    ASSERT_EQUAL(atoi2sat("42", 0), 0);
    ASSERT_EQUAL(atoi2sat(STRLIT("9223372036854775807")), LLONG_MAX);
    ASSERT_EQUAL(atoi2sat(STRLIT("-9223372036854775808")), LLONG_MIN);
    ASSERT_EQUAL(atoi2sat(STRLIT("9223372036854775808")), LLONG_MAX);
    ASSERT_EQUAL(atoi2sat(STRLIT("-9223372036854775809")), LLONG_MIN);
    ASSERT_EQUAL(atoi2sat(STRLIT("999999999999999999999999999999")),
                 LLONG_MAX);
    ASSERT_EQUAL(atoi2sat(STRLIT("-999999999999999999999999999999")),
                 LLONG_MIN);

    {
        int32 n;
        ASSERT_ZERO(util_string_int32(&n, "12345"));
        ASSERT_EQUAL(n, 12345);
        ASSERT_ZERO(util_string_int32(&n, "-54321"));
        ASSERT_EQUAL(n, -54321);
        ASSERT_EQUAL(util_string_int32(&n, "2147483648"), -1);
        ASSERT_EQUAL(util_string_int32(&n, "notanumber"), -1);
    }

    ASSERT(util_is_integer(""));
    ASSERT(util_is_integer("0123456789"));
    ASSERT(!util_is_integer("-1"));
    ASSERT(!util_is_integer("12x"));

    exit(EXIT_SUCCESS);
}

#endif

#endif /* STRTONUM_C */
