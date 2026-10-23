// Copyright 2019 Ulf Adams
//
// The contents of this file may be used under the terms of the Apache License,
// Version 2.0.
//
//    (See accompanying file LICENSE-Apache or copy at
//     http://www.apache.org/licenses/LICENSE-2.0)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

#if !defined(RYU_S2D_C)
#define RYU_S2D_C

#include "cbase.h"

#include "ryu/ryu_parse.h"

#if defined(RYU_DEBUG)
#include <inttypes.h>
#endif

#include "ryu/common.h"
#include "ryu/d2s_intrinsics.h"

#if defined(RYU_OPTIMIZE_SIZE)
#include "ryu/d2s_small_table.h"
#else
#include "ryu/d2s_full_table.h"
#endif

#define DOUBLE_MANTISSA_BITS 52
#define DOUBLE_EXPONENT_BITS 11
#define DOUBLE_EXPONENT_BIAS 1023

#if defined(_MSC_VER)
#include <intrin.h>

static inline uint32
s2d_floor_log2(uint64 value) {
    unsigned long index;

    return _BitScanReverse64(&index, value) ? (uint32)index : 64;
}
#else
static inline uint32
s2d_floor_log2(uint64 value) {
    return 63 - (uint32)__builtin_clzll(value);
}
#endif

// The max function is already defined on Windows.
static inline int32
s2d_max32(int32 a, int32 b) {
    return a < b ? b : a;
}

// Round a 64-bit significand to an integer without out-of-range shifts.
// At shift == 64, a midpoint may still round up if any lower bit or the
// multiplication's discarded fraction is nonzero. Beyond 64 it rounds to 0.
static inline uint64
s2d_round_shift(uint64 value, int32 shift, bool trailing_zeros,
                bool *round_up) {
    uint64 truncated;
    uint64 last_removed_bit;

    assert(shift >= 0);
    assert(round_up);
    if (shift == 0) {
        *round_up = false;
        return value;
    }
    if (shift > 64) {
        *round_up = false;
        return 0;
    }
    if (shift == 64) {
        truncated = 0;
        last_removed_bit = value >> 63;
        trailing_zeros &= (value & ((1ull << 63) - 1)) == 0;
    } else {
        truncated = value >> shift;
        last_removed_bit = (value >> (shift - 1)) & 1;
        trailing_zeros &= (value & ((1ull << (shift - 1)) - 1)) == 0;
    }
    *round_up = (last_removed_bit != 0)
                && (!trailing_zeros || ((truncated & 1) != 0));
    return truncated + (uint64)*round_up;
}

static inline double
int64_bits_to_double(uint64 bits) {
    double result;

    memcpy(&result, &bits, SIZEOF(result));
    return result;
}

// Fixed-width exact arithmetic for the uncommon long-decimal parsing path.
// Every binary64 rounding midpoint has at most 768 significant decimal
// digits. Retaining 800 digits and a sticky bit therefore suffices even
// when the input contains arbitrarily many trailing digits.
#define RYU_DECIMAL_DIGITS 800
#define RYU_BIG_WORDS 128

typedef struct RyuBig {
    uint32 words[RYU_BIG_WORDS];
    int32 used;
} RyuBig;

static void
ryu_big_small(RyuBig *number, uint64 value) {
    *number = (RyuBig){0};
    number->words[0] = (uint32)value;
    number->words[1] = (uint32)(value >> 32);
    number->used = 1;
    if (number->words[1] != 0) {
        number->used = 2;
    }
    return;
}

static void
ryu_big_multiply(RyuBig *number, uint32 multiplier) {
    uint64 carry = 0;

    for (int32 i = 0; i < number->used; i += 1) {
        uint64 product = (uint64)number->words[i]*multiplier + carry;

        number->words[i] = (uint32)product;
        carry = product >> 32;
    }
    if (carry != 0) {
        assert(number->used < RYU_BIG_WORDS);
        number->words[number->used] = (uint32)carry;
        number->used += 1;
    }
    return;
}

static void
ryu_big_add_digit(RyuBig *number, uint32 digit) {
    uint64 carry = digit;

    for (int32 i = 0; i < number->used && carry != 0; i += 1) {
        uint64 sum = (uint64)number->words[i] + carry;

        number->words[i] = (uint32)sum;
        carry = sum >> 32;
    }
    if (carry != 0) {
        assert(number->used < RYU_BIG_WORDS);
        number->words[number->used] = (uint32)carry;
        number->used += 1;
    }
    return;
}

static void
ryu_big_shift(RyuBig *number, int32 shift) {
    int32 whole = shift/32;
    int32 bits = shift%32;
    uint32 carry = 0;

    if (shift == 0) {
        return;
    }
    assert(shift > 0 && number->used + whole + 1 < RYU_BIG_WORDS);
    for (int32 i = number->used - 1; i >= 0; i -= 1) {
        number->words[i + whole] = number->words[i];
    }
    for (int32 i = 0; i < whole; i += 1) {
        number->words[i] = 0;
    }
    number->used += whole;
    if (bits != 0) {
        for (int32 i = 0; i < number->used; i += 1) {
            uint64 shifted = ((uint64)number->words[i] << bits) | carry;

            number->words[i] = (uint32)shifted;
            carry = (uint32)(shifted >> 32);
        }
        if (carry != 0) {
            number->words[number->used] = carry;
            number->used += 1;
        }
    }
    return;
}

static int32
ryu_big_compare(RyuBig *lhs, RyuBig *rhs) {
    if (lhs->used != rhs->used) {
        if (lhs->used > rhs->used) {
            return 1;
        }
        return -1;
    }
    for (int32 i = lhs->used - 1; i >= 0; i -= 1) {
        if (lhs->words[i] != rhs->words[i]) {
            if (lhs->words[i] > rhs->words[i]) {
                return 1;
            }
            return -1;
        }
    }
    return 0;
}

// Compare decimal prefix * 10^exponent with the exact midpoint separating
// 'lower_bits' and the following binary64 value (including infinity).
static int32
ryu_decimal_midpoint(RyuBig *prefix, int64 exponent, bool sticky,
                     uint64 lower_bits) {
    RyuBig lhs = *prefix;
    RyuBig rhs;
    uint64 mantissa = lower_bits & ((1ull << 52) - 1);
    int32 biased = (int32)((lower_bits >> 52) & 0x7ff);
    int32 binary_exponent;
    int64 rhs_shift;
    int64 lhs_shift;
    int32 comparison;

    if (biased == 0) {
        binary_exponent = -1074;
    } else {
        mantissa |= 1ull << 52;
        binary_exponent = biased - 1075;
    }
    ryu_big_small(&rhs, 2*mantissa + 1);
    rhs_shift = binary_exponent - 1;
    lhs_shift = exponent;

    // Cancel powers of ten against powers of two before comparing integers.
    if (exponent >= 0) {
        for (int64 i = 0; i < exponent; i += 1) {
            ryu_big_multiply(&lhs, 5);
        }
    } else {
        for (int64 i = 0; i < -exponent; i += 1) {
            ryu_big_multiply(&rhs, 5);
        }
        rhs_shift -= exponent;
        lhs_shift = 0;
    }
    if (lhs_shift > rhs_shift) {
        ryu_big_shift(&lhs, (int32)(lhs_shift - rhs_shift));
    } else if (rhs_shift > lhs_shift) {
        ryu_big_shift(&rhs, (int32)(rhs_shift - lhs_shift));
    }
    comparison = ryu_big_compare(&lhs, &rhs);
    if (comparison == 0 && sticky) {
        comparison = 1;
    }
    return comparison;
}

// For a long significand, the first 17 nonzero significant digits bound
// the input between two consecutive 17-digit decimal values. If those
// endpoints round to the same binary64 value, the entire interval does too.
// No large-integer calculation is needed in that case.
static bool
ryu_decimal_interval(const char *buffer, int32 parsed_len, int64 order,
                     bool negative, double *result) {
    uint64 prefix = 0;
    int32 digits = 0;
    int64 exponent = order - 16;
    double lower;
    double upper;
    char text[40];
    int32 len;
    int32 status;
    uint64 lower_bits;
    uint64 upper_bits;

    for (int32 i = 0; i < parsed_len; i += 1) {
        char c = buffer[i];

        if ((c == 'e') || (c == 'E')) {
            break;
        }
        if ((c < '0') || (c > '9')) {
            continue;
        }
        if (digits == 0 && c == '0') {
            continue;
        }
        prefix = prefix*10 + (uint64)(c - '0');
        digits += 1;
        if (digits == 17) {
            break;
        }
    }
    assert(digits == 17);

    // Construct only a short, bounded decimal for the existing Ryu path.
    // The lower endpoint is inclusive; the upper endpoint is exclusive.
    for (int32 bound = 0; bound < 2; bound += 1) {
        uint64 mantissa = prefix + (uint64)bound;
        int64 e10 = exponent;

        if (mantissa == 100000000000000000ull) {
            mantissa /= 10;
            e10 += 1;
        }
        for (int32 i = 16; i >= 0; i -= 1) {
            text[i] = (char)('0' + mantissa%10);
            mantissa /= 10;
        }
        len = 17;
        text[len] = 'e';
        len += 1;
        if (e10 < 0) {
            text[len] = '-';
            len += 1;
            e10 = -e10;
        }
        {
            char exponent_digits[16];
            int32 count = 0;

            do {
                exponent_digits[count] = (char)('0' + e10%10);
                count += 1;
                e10 /= 10;
            } while (e10 != 0);
            for (int32 i = count - 1; i >= 0; i -= 1) {
                text[len] = exponent_digits[i];
                len += 1;
            }
        }
        if (bound == 0) {
            status = s2d_n(text, len, &lower);
        } else {
            status = s2d_n(text, len, &upper);
        }
        if (status <= 0 && status != -FLOAT_UNDERFLOW) {
            return false;
        }
    }
    memcpy(&lower_bits, &lower, SIZEOF(lower_bits));
    memcpy(&upper_bits, &upper, SIZEOF(upper_bits));
    if (lower_bits != upper_bits) {
        return false;
    }
    if (negative) {
        lower_bits |= 1ull << 63;
    }
    *result = int64_bits_to_double(lower_bits);
    return true;
}

static int32
ryu_decimal_extended(const char *buffer, int32 parsed_len,
                     int64 decimal_exponent, int64 significant_digits,
                     bool negative, double *result) {
    int64 order = decimal_exponent + significant_digits - 1;
    uint64 bits = 0;
    int32 kept = 0;
    bool started = false;
    bool sticky = false;
    RyuBig prefix;
    char approximate[40];
    int32 approximate_len = 0;
    int64 approximate_exponent;
    double estimate;

    if (order >= 309) {
        bits = 0x7ff0000000000000ull;
        goto finished;
    }
    if (order <= -325) {
        goto finished;
    }
    if (ryu_decimal_interval(buffer, parsed_len, order, negative, result)) {
        uint64 rounded_bits;

        memcpy(&rounded_bits, result, SIZEOF(rounded_bits));
        if ((rounded_bits & 0x7ff0000000000000ull) == 0) {
            return -FLOAT_UNDERFLOW;
        }
        return parsed_len;
    }

    ryu_big_small(&prefix, 0);
    for (int32 i = 0; i < parsed_len; i += 1) {
        char c = buffer[i];

        if ((c == 'e') || (c == 'E')) {
            break;
        }
        if ((c < '0') || (c > '9')) {
            continue;
        }
        if (!started && c == '0') {
            continue;
        }
        started = true;
        if (kept < RYU_DECIMAL_DIGITS) {
            ryu_big_multiply(&prefix, 10);
            ryu_big_add_digit(&prefix, (uint32)(c - '0'));
            if (kept < 17) {
                approximate[approximate_len] = c;
                approximate_len += 1;
            }
            kept += 1;
        } else if (c != '0') {
            sticky = true;
        }
    }
    decimal_exponent += significant_digits - kept;

    // Use Ryu's existing short parser only to locate the nearby binary64
    // candidate. Exact midpoint comparisons below determine final rounding.
    approximate_exponent = order - 16;
    approximate[approximate_len] = 'e';
    approximate_len += 1;
    if (approximate_exponent < 0) {
        approximate[approximate_len] = '-';
        approximate_len += 1;
        approximate_exponent = -approximate_exponent;
    }
    {
        char digits[16];
        int32 count = 0;

        do {
            digits[count] = (char)('0' + approximate_exponent%10);
            count += 1;
            approximate_exponent /= 10;
        } while (approximate_exponent != 0);
        for (int32 i = count - 1; i >= 0; i -= 1) {
            approximate[approximate_len] = digits[i];
            approximate_len += 1;
        }
    }
    ASSERT_GE(abs(s2d_n(approximate, approximate_len, &estimate)),
              FLOAT_UNDERFLOW);
    memcpy(&bits, &estimate, SIZEOF(bits));
    if (bits == 0x7ff0000000000000ull) {
        bits -= 1;
    }

    // The first 17 significant digits already locate the correct value
    // within one ULP. Correct it using exact round-to-nearest, ties-to-even.
    for (;;) {
        int32 comparison = ryu_decimal_midpoint(&prefix,
                                                decimal_exponent, sticky,
                                                bits);

        if (comparison > 0 || (comparison == 0 && (bits & 1) != 0)) {
            bits += 1;
            if (bits == 0x7ff0000000000000ull) {
                break;
            }
            continue;
        }
        if (bits != 0) {
            comparison = ryu_decimal_midpoint(&prefix,
                                              decimal_exponent, sticky,
                                              bits - 1);
            if (comparison < 0 || (comparison == 0 && (bits & 1) != 0)) {
                bits -= 1;
                continue;
            }
        }
        break;
    }

finished:
    if (negative) {
        bits |= 1ull << 63;
    }
    *result = int64_bits_to_double(bits);
    if ((bits & 0x7ff0000000000000ull) == 0) {
        return -FLOAT_UNDERFLOW;
    }
    return parsed_len;
}

typedef struct RyuDecimalParts {
    int32 m10digits;
    int64 significant_digits;
    int64 parsed_exponent;
    int32 dot_index;
    int32 e_index;
    uint64 m10;
    bool signed_m;
    bool signed_e;
    bool extended;
} RyuDecimalParts;

static int32
ryu_decimal_finish(const char *buffer, int32 parsed_len,
                   RyuDecimalParts parts, double *result) {
    int32 m10digits = parts.m10digits;
    int64 significant_digits = parts.significant_digits;
    int64 parsed_exponent = parts.parsed_exponent;
    int32 dot_index = parts.dot_index;
    int32 e_index = parts.e_index;
    uint64 m10 = parts.m10;
    bool signed_m = parts.signed_m;
    bool signed_e = parts.signed_e;
    bool extended = parts.extended;
    int32 e10;
    int32 e2;
    uint64 m2;
    bool trailing_zeros;
    uint32 ieee_e2;
    int32 shift;
    bool round_up;
    uint64 ieee_m2;
    uint64 ieee;

    if (e_index < 0) {
        e_index = parsed_len;
    }
    if (dot_index < 0) {
        dot_index = e_index;
    }
    if (signed_e) {
        parsed_exponent = -parsed_exponent;
    }
    parsed_exponent -= dot_index < e_index ? e_index - dot_index - 1 : 0;
    if (extended && m10 != 0) {
        return ryu_decimal_extended(buffer, parsed_len, parsed_exponent,
                                    significant_digits, signed_m, result);
    }
    if (parsed_exponent > 1000000000ll) {
        e10 = 1000000000;
    } else if (parsed_exponent < -1000000000ll) {
        e10 = -1000000000;
    } else {
        e10 = (int32)parsed_exponent;
    }
    if (m10 == 0) {
        *result = signed_m ? -0.0 : 0.0;
        return parsed_len;
    }

#if defined(RYU_DEBUG)
    fmt_printf("Input=%.*s\n", parsed_len, buffer);
    fmt_printf("m10digits = %d\n", m10digits);
    fmt_printf("m10 * 10^e10 = %" PRIu64 " * 10^%d\n", m10, e10);
#endif

    if ((m10digits + e10 <= -324) || (m10 == 0)) {
        // Number is less than 1e-324, which should be rounded down to 0.
        ieee =
            ((uint64)signed_m)
            << (DOUBLE_EXPONENT_BITS + DOUBLE_MANTISSA_BITS);
        *result = int64_bits_to_double(ieee);
        return -FLOAT_UNDERFLOW;
    }
    if (m10digits + e10 >= 310) {
        // Number is larger than 1e+309, which should round to +/-Infinity.
        ieee =
            (((uint64)signed_m)
             << (DOUBLE_EXPONENT_BITS + DOUBLE_MANTISSA_BITS))
            | (0x7ffull << DOUBLE_MANTISSA_BITS);
        *result = int64_bits_to_double(ieee);
        return parsed_len;
    }

    // Convert to binary float m2 * 2^e2, while retaining information about
    // whether the conversion was exact (trailing_zeros).
    if (e10 >= 0) {
        int32 j;

        // The length of m * 10^e in bits is:
        //   log2(m10 * 10^e10)
        //       = log2(m10) + e10 + e10 * log2(5)
        //
        // We want to compute the DOUBLE_MANTISSA_BITS + 1 top-most bits
        // (+1 for the implicit leading one in IEEE format). We therefore
        // choose a binary output exponent of:
        //   log2(m10 * 10^e10) - (DOUBLE_MANTISSA_BITS + 1).
        //
        // We use floor(log2(5^e10)) so that we get at least this many bits;
        // better to have an additional bit than to not have enough bits.
        e2 = (int32)s2d_floor_log2(m10) + e10 + log2_pow5(e10)
             - (DOUBLE_MANTISSA_BITS + 1);

        // We now compute:
        //   [m10 * 10^e10 / 2^e2]
        //       = [m10 * 5^e10 / 2^(e2-e10)].
        j = e2 - e10 - ceil_log2_pow5(e10) + DOUBLE_POW5_BITCOUNT;
        assert(j >= 0);
#if defined(RYU_OPTIMIZE_SIZE)
        {
            uint64 pow5[2];

            double_compute_pow5((uint32)e10, pow5);
            m2 = mul_shift64(m10, pow5, j);
        }
#else
        assert(e10 < DOUBLE_POW5_TABLE_SIZE);
        m2 = mul_shift64(m10, double_pow5_split[e10], j);
#endif
        // We also compute if the result is exact, i.e.:
        //   [m10 * 10^e10 / 2^e2] == m10 * 10^e10 / 2^e2.
        // This can only be the case if 2^e2 divides m10 * 10^e10, which
        // in turn requires that the largest power of 2 that divides
        // m10 + e10 is greater than e2. If e2 is less than e10, the
        // result must be exact. Otherwise use multiple_of_power_of2.
        trailing_zeros = e2 < e10
                         || ((e2 - e10 < 64)
                             && multiple_of_power_of2(m10, (uint32)(e2 - e10)));
    } else {
        int32 j;

        e2 = (int32)s2d_floor_log2(m10) + e10 - ceil_log2_pow5(-e10)
             - (DOUBLE_MANTISSA_BITS + 1);
        j = e2 - e10 + ceil_log2_pow5(-e10) - 1
            + DOUBLE_POW5_INV_BITCOUNT;
#if defined(RYU_OPTIMIZE_SIZE)
        {
            uint64 pow5[2];

            double_compute_inv_pow5((uint32)(-e10), pow5);
            m2 = mul_shift64(m10, pow5, j);
        }
#else
        assert(-e10 < DOUBLE_POW5_INV_TABLE_SIZE);
        m2 = mul_shift64(m10, double_pow5_inv_split[-e10], j);
#endif
        trailing_zeros = multiple_of_power_of5(m10, (uint32)(-e10));
    }

#if defined(RYU_DEBUG)
    fmt_printf("m2 * 2^e2 = %" PRIu64 " * 2^%d\n", m2, e2);
#endif

    // Compute the final IEEE exponent.
    ieee_e2 = (uint32)s2d_max32(0, e2 + DOUBLE_EXPONENT_BIAS
                                   + (int32)s2d_floor_log2(m2));

    if (ieee_e2 > 0x7fe) {
        // Final IEEE exponent is larger than representable: +/-Infinity.
        ieee =
            (((uint64)signed_m)
             << (DOUBLE_EXPONENT_BITS + DOUBLE_MANTISSA_BITS))
            | (0x7ffull << DOUBLE_MANTISSA_BITS);
        *result = int64_bits_to_double(ieee);
        return parsed_len;
    }

    // We need to figure out how much we need to shift m2. The tricky part is
    // that the final IEEE exponent must be taken into account, so we need to
    // reverse the bias and also special-case the value zero.
    shift = (ieee_e2 == 0 ? 1 : (int32)ieee_e2) - e2
            - DOUBLE_EXPONENT_BIAS - DOUBLE_MANTISSA_BITS;
    assert(shift >= 0);
#if defined(RYU_DEBUG)
    fmt_printf("ieee_e2 = %d\n", ieee_e2);
    fmt_printf("shift = %d\n", shift);
#endif

    // Round to nearest, ties to even. The helper also handles shifts that
    // reach or exceed the significand width without undefined behavior.
    ieee_m2 = s2d_round_shift(m2, shift, trailing_zeros, &round_up);

#if defined(RYU_DEBUG)
    fmt_printf("round_up = %d\n", round_up);
    fmt_printf("ieee_m2 = %" PRIu64 "\n", ieee_m2);
#endif
    assert(ieee_m2 <= (1ull << (DOUBLE_MANTISSA_BITS + 1)));
    ieee_m2 &= (1ull << DOUBLE_MANTISSA_BITS) - 1;
    if (ieee_m2 == 0 && round_up) {
        // IEEE represents +/-Infinity so no overflow check is needed here.
        ieee_e2 += 1;
    }

    ieee = (((((uint64)signed_m) << DOUBLE_EXPONENT_BITS) | (uint64)ieee_e2)
            << DOUBLE_MANTISSA_BITS)
           | ieee_m2;
    *result = int64_bits_to_double(ieee);
    if (ieee_e2 > 0x7fe) {
        return parsed_len;
    }
    // Rounding may carry the largest subnormal into the smallest normal.
    // Report underflow based on the final exponent, not the tentative one.
    if (ieee_e2 == 0) {
        return -FLOAT_UNDERFLOW;
    }
    return parsed_len;
}

static int32
ryu_parse_special_double(const char *buffer, int32 len,
                         int32 index, bool negative, double *result) {
    char c0;
    char c1;
    char c2;
    bool is_inf;
    bool is_nan;

    if ((index + 2) >= len) {
        return 0;
    }

    c0 = buffer[index];
    c1 = buffer[index + 1];
    c2 = buffer[index + 2];
    is_inf = ((c0 == 'i') || (c0 == 'I'))
             && ((c1 == 'n') || (c1 == 'N'))
             && ((c2 == 'f') || (c2 == 'F'));
    is_nan = ((c0 == 'n') || (c0 == 'N'))
             && ((c1 == 'a') || (c1 == 'A'))
             && ((c2 == 'n') || (c2 == 'N'));

    if (is_inf) {
        int32 special_len = index + 3;
        uint64 ieee = 0x7ffull << DOUBLE_MANTISSA_BITS;

        if ((index + 8) <= len) {
            char c3 = buffer[index + 3];
            char c4 = buffer[index + 4];
            char c5 = buffer[index + 5];
            char c6 = buffer[index + 6];
            char c7 = buffer[index + 7];
            bool is_infinity = ((c3 == 'i') || (c3 == 'I'))
                               && ((c4 == 'n') || (c4 == 'N'))
                               && ((c5 == 'i') || (c5 == 'I'))
                               && ((c6 == 't') || (c6 == 'T'))
                               && ((c7 == 'y') || (c7 == 'Y'));

            if (is_infinity) {
                special_len = index + 8;
            }
        }
        if (negative) {
            ieee |= 1ull << 63;
        }
        *result = int64_bits_to_double(ieee);
        return special_len;
    }

    if (is_nan) {
        uint64 ieee = (0x7ffull << DOUBLE_MANTISSA_BITS)
                      | (1ull << (DOUBLE_MANTISSA_BITS - 1));

        if (negative) {
            ieee |= 1ull << 63;
        }
        *result = int64_bits_to_double(ieee);
        return index + 3;
    }

    return 0;
}

static int32
ryu_hex_digit(char c) {
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

static int32
ryu_hex_highest_bit(int32 digit) {
    assert(digit > 0);
    if (digit >= 8) {
        return 3;
    }
    if (digit >= 4) {
        return 2;
    }
    if (digit >= 2) {
        return 1;
    }
    return 0;
}

static bool
ryu_hex_lower_nonzero(uint64 prefix, int32 lower_prefix_bits, bool sticky) {
    bool lower_nonzero = sticky;

    if (lower_prefix_bits > 0) {
        uint64 lower_mask = (1ull << lower_prefix_bits) - 1;

        if ((prefix & lower_mask) != 0) {
            lower_nonzero = true;
        }
    }
    return lower_nonzero;
}

static uint64
ryu_hex_round(uint64 prefix, int32 prefix_bits, int32 kept_bits,
              bool sticky) {
    int32 discarded_prefix_bits = prefix_bits - kept_bits;
    int32 lower_prefix_bits = discarded_prefix_bits - 1;
    uint64 significand = prefix >> discarded_prefix_bits;
    uint64 guard = (prefix >> lower_prefix_bits) & 1;
    bool lower_nonzero;

    lower_nonzero = ryu_hex_lower_nonzero(prefix, lower_prefix_bits, sticky);
    if ((guard != 0) && (lower_nonzero || ((significand & 1) != 0))) {
        significand += 1;
    }
    return significand;
}

static int32
ryu_parse_hex_double(const char *buffer, int32 len, int32 index,
                     bool negative, double *result) {
    uint64 prefix = 0;
    int32 prefix_bits = 0;
    int64 bit_len = 0;
    int64 fractional_hex_digits = 0;
    int64 exponent = 0;
    int64 scale;
    int64 top_exponent;
    bool sticky = false;
    bool started = false;
    bool dot_seen = false;
    bool has_hex_digit = false;
    bool exponent_negative = false;
    int32 exponent_index;
    int32 exponent_digit_index;
    int32 parsed_hex_len;

    if ((index + 2) > len || buffer[index] != '0'
        || ((buffer[index + 1] != 'x') && (buffer[index + 1] != 'X'))) {
        return 0;
    }

    index += 2;
    for (; index < len; index += 1) {
        char c = buffer[index];
        int32 digit = ryu_hex_digit(c);

        if (digit < 0) {
            if ((c == '.') && !dot_seen) {
                dot_seen = true;
                continue;
            }
            break;
        }

        has_hex_digit = true;
        if (dot_seen) {
            fractional_hex_digits += 1;
        }

        if (!started) {
            if (digit == 0) {
                continue;
            }
            started = true;
            for (int32 bit = ryu_hex_highest_bit(digit);
                 bit >= 0;
                 bit -= 1) {
                uint64 value = (uint64)((digit >> bit) & 1);

                bit_len += 1;
                prefix = (prefix << 1) | value;
                prefix_bits += 1;
            }
            continue;
        }

        for (int32 bit = 3; bit >= 0; bit -= 1) {
            uint64 value = (uint64)((digit >> bit) & 1);

            bit_len += 1;
            if (prefix_bits < 64) {
                prefix = (prefix << 1) | value;
                prefix_bits += 1;
            } else if (value != 0) {
                sticky = true;
            }
        }
    }

    if (!has_hex_digit || (index >= len)
        || ((buffer[index] != 'p') && (buffer[index] != 'P'))) {
        return 0;
    }

    exponent_index = index;
    exponent_digit_index = exponent_index + 1;
    if ((exponent_digit_index < len)
        && ((buffer[exponent_digit_index] == '-')
            || (buffer[exponent_digit_index] == '+'))) {
        exponent_negative = buffer[exponent_digit_index] == '-';
        exponent_digit_index += 1;
    }

    if ((exponent_digit_index >= len)
        || (buffer[exponent_digit_index] < '0')
        || (buffer[exponent_digit_index] > '9')) {
        return 0;
    }

    index = exponent_digit_index;
    for (; index < len; index += 1) {
        char c = buffer[index];
        int32 digit;
        int64 exponent_limit = INT64_MAX/2;

        if ((c < '0') || (c > '9')) {
            break;
        }
        digit = c - '0';
        if (exponent > (exponent_limit - digit)/10) {
            exponent = exponent_limit;
        } else {
            exponent = 10*exponent + digit;
        }
    }

    parsed_hex_len = index;
    if (exponent_negative) {
        exponent = -exponent;
    }
    scale = exponent - 4*fractional_hex_digits;

    if (!started) {
        uint64 ieee = 0;

        if (negative) {
            ieee = 1ull << 63;
        }
        *result = int64_bits_to_double(ieee);
        return parsed_hex_len;
    }

    top_exponent = scale + bit_len - 1;
    if (top_exponent > 1023) {
        uint64 ieee = 0x7ffull << DOUBLE_MANTISSA_BITS;

        if (negative) {
            ieee |= 1ull << 63;
        }
        *result = int64_bits_to_double(ieee);
        return parsed_hex_len;
    }

    if (top_exponent >= -1022) {
        uint64 significand;

        // Normal numbers retain 53 significand bits.

        if (bit_len <= 53) {
            significand = prefix << (53 - bit_len);
        } else {
            significand = ryu_hex_round(prefix, prefix_bits, 53, sticky);
            if (significand == (1ull << 53)) {
                significand >>= 1;
                top_exponent += 1;
                if (top_exponent > 1023) {
                    uint64 ieee = 0x7ffull << DOUBLE_MANTISSA_BITS;

                    if (negative) {
                        ieee |= 1ull << 63;
                    }
                    *result = int64_bits_to_double(ieee);
                    return parsed_hex_len;
                }
            }
        }

        {
            uint64 exponent_bits = (uint64)(top_exponent
                                            + DOUBLE_EXPONENT_BIAS);
            uint64 mantissa_mask = (1ull << DOUBLE_MANTISSA_BITS) - 1;
            uint64 ieee = (exponent_bits << DOUBLE_MANTISSA_BITS)
                          | (significand & mantissa_mask);

            if (negative) {
                ieee |= 1ull << 63;
            }
            *result = int64_bits_to_double(ieee);
            return parsed_hex_len;
        }
    }

    {
        int64 right_shift = -1074 - scale;
        uint64 significand = 0;

        // Subnormals are integer multiples of 2^-1074.

        if (right_shift <= 0) {
            significand = prefix << -right_shift;
        } else {
            int64 kept_bits = bit_len - right_shift;

            if (kept_bits > 0) {
                assert(kept_bits <= INT32_MAX);
                significand = ryu_hex_round(prefix, prefix_bits,
                                            (int32)kept_bits, sticky);
            } else if (kept_bits == 0) {
                int32 lower_prefix_bits = prefix_bits - 1;

                if (ryu_hex_lower_nonzero(prefix, lower_prefix_bits,
                                          sticky)) {
                    significand = 1;
                }
            }
        }

        {
            uint64 ieee;

            if (significand == (1ull << 52)) {
                ieee = 1ull << DOUBLE_MANTISSA_BITS;
            } else {
                ieee = significand;
            }
            if (negative) {
                ieee |= 1ull << 63;
            }
            *result = int64_bits_to_double(ieee);
            if (significand == (1ull << DOUBLE_MANTISSA_BITS)) {
                return parsed_hex_len;
            }
            return -FLOAT_UNDERFLOW;
        }
    }
}

static int32
ryu_parse_nondecimal_double(const char *buffer, int32 len,
                            double *result) {
    int32 index = 0;
    bool negative = false;
    int32 status;

    if ((buffer[index] == '-') || (buffer[index] == '+')) {
        negative = buffer[index] == '-';
        index += 1;
    }

    status = ryu_parse_special_double(buffer, len, index, negative, result);
    if (status != 0) {
        return status;
    }
    return ryu_parse_hex_double(buffer, len, index, negative, result);
}

int32
s2d_n(const char *buffer, int32 len, double *result) {
    RyuDecimalParts parts = {
        .dot_index = -1,
        .e_index = -1,
    };
    bool has_mantissa_digit = false;
    int32 i = 0;
    int32 nondecimal_status;
    int32 parsed_len;

    if (len <= 0) {
        return -INPUT_TOO_SHORT;
    }

    // %a/%A values are binary already, so parse them without Ryu decimal
    // conversion. Keep only the high bits plus a sticky bit for exact
    // round-to-nearest-even conversion to IEEE-754 binary64.
    nondecimal_status = ryu_parse_nondecimal_double(buffer, len, result);
    if (nondecimal_status != 0) {
        return nondecimal_status;
    }

    if ((i < len) && ((buffer[i] == '-') || (buffer[i] == '+'))) {
        parts.signed_m = buffer[i] == '-';
        i += 1;
    }

    for (; i < len; i += 1) {
        char c = buffer[i];

        if ((c >= '0') && (c <= '9')) {
            has_mantissa_digit = true;
            if (parts.significant_digits != 0 || c != '0') {
                parts.significant_digits += 1;
            }
            if (parts.significant_digits > 17) {
                parts.extended = true;
            } else {
                parts.m10 = 10*parts.m10 + (uint64)(c - '0');
                if (parts.m10 != 0) {
                    parts.m10digits += 1;
                }
            }
            continue;
        }
        if ((c == '.') && (parts.dot_index < 0)) {
            parts.dot_index = i;
            continue;
        }
        break;
    }

    if (!has_mantissa_digit) {
        return -MALFORMED_INPUT;
    }

    if ((i < len) && ((buffer[i] == 'e') || (buffer[i] == 'E'))) {
        int32 exponent_index = i;
        int32 exponent_digit_index = i + 1;

        if ((exponent_digit_index < len)
            && ((buffer[exponent_digit_index] == '-')
                || (buffer[exponent_digit_index] == '+'))) {
            exponent_digit_index += 1;
        }

        if ((exponent_digit_index < len)
            && (buffer[exponent_digit_index] >= '0')
            && (buffer[exponent_digit_index] <= '9')) {
            parts.e_index = exponent_index;
            i = exponent_index + 1;
            if ((buffer[i] == '-') || (buffer[i] == '+')) {
                parts.signed_e = buffer[i] == '-';
                i += 1;
            }
            for (; i < len; i += 1) {
                char c = buffer[i];

                if ((c < '0') || (c > '9')) {
                    break;
                }
                if (parts.parsed_exponent < 1000000000000ll) {
                    parts.parsed_exponent =
                        10*parts.parsed_exponent + (c - '0');
                    if (parts.parsed_exponent > 1000000000000ll) {
                        parts.parsed_exponent = 1000000000000ll;
                    }
                }
            }
        }
    }

    parsed_len = i;
    return ryu_decimal_finish(buffer, parsed_len, parts, result);
}

static bool
ryu_s2d_is_digit(char c) {
    return (c >= '0') && (c <= '9');
}

static bool
ryu_s2d_is_alpha(char c) {
    return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'));
}

static bool
ryu_s2d_is_token_char(char c) {
    return ryu_s2d_is_digit(c) || ryu_s2d_is_alpha(c) || (c == '.')
           || (c == '+') || (c == '-');
}

static bool
ryu_fast_nondecimal_start(const char *cursor) {
    return (cursor[0] == 'i') || (cursor[0] == 'I') || (cursor[0] == 'n')
           || (cursor[0] == 'N')
           || ((cursor[0] == '0')
               && ((cursor[1] == 'x') || (cursor[1] == 'X')));
}

static int32
ryu_fast_nondecimal_token_len(const char *buffer, const char *cursor) {
    const char *end;

    if (!ryu_fast_nondecimal_start(cursor)) {
        return 0;
    }

    // Determine only the token length. The next byte may be the start of
    // another TSV cell, so strlen() would scan the entire file tail.
    end = cursor;
    while (ryu_s2d_is_token_char(*end)) {
        end += 1;
    }
    if (end - buffer > INT32_MAX) {
        return -INPUT_TOO_LONG;
    }
    return (int32)(end - buffer);
}

static bool
ryu_fast_scan_mantissa(RyuDecimalParts *parts, const char *buffer,
                       const char **cursor) {
    bool has_digit = false;

    for (;;) {
        char c = **cursor;

        if (ryu_s2d_is_digit(c)) {
            has_digit = true;
            if ((parts->significant_digits != 0) || (c != '0')) {
                parts->significant_digits += 1;
            }
            if (parts->significant_digits > 17) {
                parts->extended = true;
            } else {
                parts->m10 = 10*parts->m10 + (uint64)(c - '0');
                if (parts->m10 != 0) {
                    parts->m10digits += 1;
                }
            }
            *cursor += 1;
            continue;
        }
        if ((c == '.') && (parts->dot_index < 0)) {
            parts->dot_index = (int32)(*cursor - buffer);
            *cursor += 1;
            continue;
        }
        break;
    }
    return has_digit;
}

static void
ryu_fast_scan_exponent(RyuDecimalParts *parts, const char *buffer,
                       const char **cursor) {
    const char *digits;

    if ((**cursor != 'e') && (**cursor != 'E')) {
        return;
    }

    digits = *cursor + 1;
    if ((*digits == '-') || (*digits == '+')) {
        digits += 1;
    }
    if (!ryu_s2d_is_digit(*digits)) {
        return;
    }

    parts->e_index = (int32)(*cursor - buffer);
    *cursor += 1;
    if ((**cursor == '-') || (**cursor == '+')) {
        parts->signed_e = **cursor == '-';
        *cursor += 1;
    }
    while (ryu_s2d_is_digit(**cursor)) {
        int32 digit = **cursor - '0';

        if (parts->parsed_exponent < 1000000000000ll) {
            parts->parsed_exponent = 10*parts->parsed_exponent + digit;
            if (parts->parsed_exponent > 1000000000000ll) {
                parts->parsed_exponent = 1000000000000ll;
            }
        }
        *cursor += 1;
    }
    return;
}

// Parses a nul-terminated floating-point prefix without length checks in
// the ordinary decimal scanner. Exotic spellings use the bounded parser.
int32
s2d_fast(const char *buffer, double *result) {
    const char *cursor = buffer;
    RyuDecimalParts parts = {
        .dot_index = -1,
        .e_index = -1,
    };
    int32 token_len;

    if (*cursor == '\0') {
        return -INPUT_TOO_SHORT;
    }
    if ((*cursor == '-') || (*cursor == '+')) {
        parts.signed_m = *cursor == '-';
        cursor += 1;
    }

    token_len = ryu_fast_nondecimal_token_len(buffer, cursor);
    if (token_len != 0) {
        if (token_len < 0) {
            return token_len;
        }
        return s2d_n(buffer, token_len, result);
    }

    if (!ryu_fast_scan_mantissa(&parts, buffer, &cursor)) {
        return -MALFORMED_INPUT;
    }
    ryu_fast_scan_exponent(&parts, buffer, &cursor);

    if (cursor - buffer > INT32_MAX) {
        return -INPUT_TOO_LONG;
    }
    return ryu_decimal_finish(buffer, (int32)(cursor - buffer), parts, result);
}

int32
s2d(const char *buffer, double *result) {
    return s2d_n(buffer, strlen32((char *)buffer), result);
}
#endif /* RYU_S2D_C */
