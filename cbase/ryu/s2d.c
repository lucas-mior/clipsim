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

#include "cbase.h"

#include "ryu/ryu_parse.h"

#ifdef RYU_DEBUG
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

static inline uint32 floor_log2(const uint64 value) {
  long index;
  return _BitScanReverse64(&index, value) ? index : 64;
}

#else

static inline uint32 floor_log2(const uint64 value) {
  return 63 - __builtin_clzll(value);
}

#endif

// The max function is already defined on Windows.
static inline int32 max32(int32 a, int32 b) {
  return a < b ? b : a;
}

static inline double int64Bits2Double(uint64 bits) {
  double f;
  memcpy(&f, &bits, sizeof(double));
  return f;
}

// Fixed-width exact arithmetic for the uncommon long-decimal parsing path.
// Every binary64 rounding midpoint has at most 768 significant decimal
// digits. Retaining 800 digits and a sticky bit therefore suffices even
// when the input contains arbitrarily many trailing digits.
#define RYU_DECIMAL_DIGITS 800
#define RYU_BIG_WORDS 128

typedef struct {
    uint32 words[RYU_BIG_WORDS];
    int32 used;
} RyuBig;

static void
ryu_big_small(RyuBig *number, uint64 value) {
    memset(number, 0, SIZEOF(*number));
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
            uint64 shifted = ((uint64)number->words[i] << bits) |carry;

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
    *result = int64Bits2Double(lower_bits);
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
    int32 status;

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
    status = s2d_n(approximate, approximate_len, &estimate);
    assert(status > 0 || status == -FLOAT_UNDERFLOW);
    memcpy(&bits, &estimate, SIZEOF(bits));
    if (bits == 0x7ff0000000000000ull) {
        bits -= 1;
    }

    // The first 17 significant digits already locate the correct value
    // within one ULP. Correct it using exact round-to-nearest, ties-to-even.
    for (;;) {
        int32 comparison = ryu_decimal_midpoint(&prefix,
                                                decimal_exponent, sticky, bits);

        if (comparison > 0 || (comparison == 0 && (bits &1) != 0)) {
            bits += 1;
            if (bits == 0x7ff0000000000000ull) {
                break;
            }
            continue;
        }
        if (bits != 0) {
            comparison = ryu_decimal_midpoint(&prefix,
                                              decimal_exponent,
                                              sticky, bits - 1);
            if (comparison < 0 || (comparison == 0 && (bits &1) != 0)) {
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
    *result = int64Bits2Double(bits);
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

#ifdef RYU_DEBUG
  printf("Input=%.*s\n", parsed_len, buffer);
  printf("m10digits = %d\n", m10digits);
  printf("e10digits = %d\n", e10digits);
  printf("m10 * 10^e10 = %" PRIu64 " * 10^%d\n", m10, e10);
#endif

  if ((m10digits + e10 <= -324) || (m10 == 0)) {
    // Number is less than 1e-324, which should be rounded down to 0; return +/-0.0.
    uint64 ieee =
        ((uint64)signed_m) << (DOUBLE_EXPONENT_BITS + DOUBLE_MANTISSA_BITS);
    *result = int64Bits2Double(ieee);
    return -FLOAT_UNDERFLOW;
  }
  if (m10digits + e10 >= 310) {
    // Number is larger than 1e+309, which should be rounded to +/-Infinity.
    uint64 ieee =
        (((uint64)signed_m)
         << (DOUBLE_EXPONENT_BITS + DOUBLE_MANTISSA_BITS))
        |(0x7ffull << DOUBLE_MANTISSA_BITS);
    *result = int64Bits2Double(ieee);
    return parsed_len;
  }

  // Convert to binary float m2 * 2^e2, while retaining information about whether the conversion
  // was exact (trailingZeros).
  int32 e2;
  uint64 m2;
  bool trailingZeros;
  if (e10 >= 0) {
    // The length of m * 10^e in bits is:
    //   log2(m10 * 10^e10) = log2(m10) + e10 log2(10) = log2(m10) + e10 + e10 * log2(5)
    //
    // We want to compute the DOUBLE_MANTISSA_BITS + 1 top-most bits (+1 for the implicit leading
    // one in IEEE format). We therefore choose a binary output exponent of
    //   log2(m10 * 10^e10) - (DOUBLE_MANTISSA_BITS + 1).
    //
    // We use floor(log2(5^e10)) so that we get at least this many bits; better to
    // have an additional bit than to not have enough bits.
    e2 = floor_log2(m10) + e10 + log2pow5(e10) - (DOUBLE_MANTISSA_BITS + 1);

    // We now compute [m10 * 10^e10 / 2^e2] = [m10 * 5^e10 / 2^(e2-e10)].
    // To that end, we use the DOUBLE_POW5_SPLIT table.
    int j = e2 - e10 - ceil_log2pow5(e10) + DOUBLE_POW5_BITCOUNT;
    assert(j >= 0);
#if defined(RYU_OPTIMIZE_SIZE)
    uint64 pow5[2];
    double_computePow5(e10, pow5);
    m2 = mulShift64(m10, pow5, j);
#else
    assert(e10 < DOUBLE_POW5_TABLE_SIZE);
    m2 = mulShift64(m10, DOUBLE_POW5_SPLIT[e10], j);
#endif
    // We also compute if the result is exact, i.e.,
    //   [m10 * 10^e10 / 2^e2] == m10 * 10^e10 / 2^e2.
    // This can only be the case if 2^e2 divides m10 * 10^e10, which in turn requires that the
    // largest power of 2 that divides m10 + e10 is greater than e2. If e2 is less than e10, then
    // the result must be exact. Otherwise we use the existing multipleOfPowerOf2 function.
    trailingZeros = e2 < e10 || (e2 - e10 < 64 && multipleOfPowerOf2(m10, e2 - e10));
  } else {
    e2 = floor_log2(m10) + e10 - ceil_log2pow5(-e10) - (DOUBLE_MANTISSA_BITS + 1);
    int j = e2 - e10 + ceil_log2pow5(-e10) - 1 + DOUBLE_POW5_INV_BITCOUNT;
#if defined(RYU_OPTIMIZE_SIZE)
    uint64 pow5[2];
    double_computeInvPow5(-e10, pow5);
    m2 = mulShift64(m10, pow5, j);
#else
    assert(-e10 < DOUBLE_POW5_INV_TABLE_SIZE);
    m2 = mulShift64(m10, DOUBLE_POW5_INV_SPLIT[-e10], j);
#endif
    trailingZeros = multipleOfPowerOf5(m10, -e10);
  }

#ifdef RYU_DEBUG
  printf("m2 * 2^e2 = %" PRIu64 " * 2^%d\n", m2, e2);
#endif

  // Compute the final IEEE exponent.
  uint32 ieee_e2 = (uint32) max32(0, e2 + DOUBLE_EXPONENT_BIAS + floor_log2(m2));
  bool underflow = ieee_e2 == 0;

  if (ieee_e2 > 0x7fe) {
    // Final IEEE exponent is larger than the maximum representable; return +/-Infinity.
    uint64 ieee =
        (((uint64)signed_m)
         << (DOUBLE_EXPONENT_BITS + DOUBLE_MANTISSA_BITS))
        |(0x7ffull << DOUBLE_MANTISSA_BITS);
    *result = int64Bits2Double(ieee);
    return parsed_len;
  }

  // We need to figure out how much we need to shift m2. The tricky part is that we need to take
  // the final IEEE exponent into account, so we need to reverse the bias and also special-case
  // the value 0.
  int32 shift = (ieee_e2 == 0 ? 1 : ieee_e2) - e2 - DOUBLE_EXPONENT_BIAS - DOUBLE_MANTISSA_BITS;
  assert(shift >= 0);
#ifdef RYU_DEBUG
  printf("ieee_e2 = %d\n", ieee_e2);
  printf("shift = %d\n", shift);
#endif

  // We need to round up if the exact value is more than 0.5 above the value we computed. That's
  // equivalent to checking if the last removed bit was 1 and either the value was not just
  // trailing zeros or the result would otherwise be odd.
  //
  // We need to update trailingZeros given that we have the exact output exponent ieee_e2 now.
  trailingZeros &= (m2 & ((1ull << (shift - 1)) - 1)) == 0;
  uint64 lastRemovedBit = (m2 >> (shift - 1)) & 1;
  bool roundUp = (lastRemovedBit != 0) && (!trailingZeros || (((m2 >> shift) & 1) != 0));

#ifdef RYU_DEBUG
  printf("roundUp = %d\n", roundUp);
  printf("ieee_m2 = %" PRIu64 "\n", (m2 >> shift) + roundUp);
#endif
  uint64 ieee_m2 = (m2 >> shift) + roundUp;
  assert(ieee_m2 <= (1ull << (DOUBLE_MANTISSA_BITS + 1)));
  ieee_m2 &= (1ull << DOUBLE_MANTISSA_BITS) - 1;
  if (ieee_m2 == 0 && roundUp) {
    // Due to how the IEEE represents +/-Infinity, we don't need to check for overflow here.
    ieee_e2++;
  }

  uint64 ieee =
      (((((uint64)signed_m) << DOUBLE_EXPONENT_BITS) |(uint64)ieee_e2)
       << DOUBLE_MANTISSA_BITS)
      |ieee_m2;
  *result = int64Bits2Double(ieee);
  if (ieee_e2 > 0x7fe) {
    return parsed_len;
  }
  if (underflow) {
    return -FLOAT_UNDERFLOW;
  }
  return parsed_len;
}

int32
s2d_n(const char *buffer, int32 len, double *result) {
    int32 m10digits = 0;
    int64 significant_digits = 0;
    int64 parsed_exponent = 0;
    bool extended = false;
    int32 dot_index = -1;
    int32 e_index = -1;
    uint64 m10 = 0;
    bool signed_m = false;
    bool signed_e = false;
    bool has_mantissa_digit = false;
    int32 i = 0;
    int32 parsed_len;

    if (len <= 0) {
        return -INPUT_TOO_SHORT;
    }

    // %a/%A values are binary already, so parse them without Ryu decimal
    // conversion. Keep only the high bits plus a sticky bit for exact
    // round-to-nearest-even conversion to IEEE-754 binary64.
    {
        int32 hex_i = 0;
        bool hex_negative = false;

        if ((buffer[hex_i] == '-') || (buffer[hex_i] == '+')) {
            hex_negative = buffer[hex_i] == '-';
            hex_i += 1;
        }

        if ((hex_i + 2) < len) {
            char c0 = buffer[hex_i];
            char c1 = buffer[hex_i + 1];
            char c2 = buffer[hex_i + 2];
            bool is_inf = ((c0 == 'i') || (c0 == 'I'))
                          && ((c1 == 'n') || (c1 == 'N'))
                          && ((c2 == 'f') || (c2 == 'F'));
            bool is_nan = ((c0 == 'n') || (c0 == 'N'))
                          && ((c1 == 'a') || (c1 == 'A'))
                          && ((c2 == 'n') || (c2 == 'N'));

            if (is_inf) {
                int32 special_len = hex_i + 3;
                uint64 ieee = 0x7ffull << DOUBLE_MANTISSA_BITS;

                if ((hex_i + 8) <= len) {
                    char c3 = buffer[hex_i + 3];
                    char c4 = buffer[hex_i + 4];
                    char c5 = buffer[hex_i + 5];
                    char c6 = buffer[hex_i + 6];
                    char c7 = buffer[hex_i + 7];
                    bool is_infinity = ((c3 == 'i') || (c3 == 'I'))
                                       && ((c4 == 'n') || (c4 == 'N'))
                                       && ((c5 == 'i') || (c5 == 'I'))
                                       && ((c6 == 't') || (c6 == 'T'))
                                       && ((c7 == 'y') || (c7 == 'Y'));

                    if (is_infinity) {
                        special_len = hex_i + 8;
                    }
                }
                if (hex_negative) {
                    ieee |= 1ull << 63;
                }
                *result = int64Bits2Double(ieee);
                return special_len;
            }
            if (is_nan) {
                uint64 ieee = (0x7ffull << DOUBLE_MANTISSA_BITS)
                              |(1ull << (DOUBLE_MANTISSA_BITS - 1));

                if (hex_negative) {
                    ieee |= 1ull << 63;
                }
                *result = int64Bits2Double(ieee);
                return hex_i + 3;
            }
        }

        if ((hex_i + 2) <= len && buffer[hex_i] == '0'
            && ((buffer[hex_i + 1] == 'x')
                || (buffer[hex_i + 1] == 'X'))) {
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

            hex_i += 2;
            for (; hex_i < len; hex_i += 1) {
                char c = buffer[hex_i];
                int32 digit;

                if ((c >= '0') && (c <= '9')) {
                    digit = c - '0';
                } else if ((c >= 'a') && (c <= 'f')) {
                    digit = c - 'a' + 10;
                } else if ((c >= 'A') && (c <= 'F')) {
                    digit = c - 'A' + 10;
                } else {
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
                    int32 highest_bit;

                    if (digit == 0) {
                        continue;
                    }
                    started = true;
                    if (digit >= 8) {
                        highest_bit = 3;
                    } else if (digit >= 4) {
                        highest_bit = 2;
                    } else if (digit >= 2) {
                        highest_bit = 1;
                    } else {
                        highest_bit = 0;
                    }

                    for (int32 bit = highest_bit; bit >= 0; bit -= 1) {
                        uint64 value = (uint64)((digit >> bit) & 1);

                        bit_len += 1;
                        prefix = (prefix << 1) |value;
                        prefix_bits += 1;
                    }
                    continue;
                }

                for (int32 bit = 3; bit >= 0; bit -= 1) {
                    uint64 value = (uint64)((digit >> bit) & 1);

                    bit_len += 1;
                    if (prefix_bits < 64) {
                        prefix = (prefix << 1) |value;
                        prefix_bits += 1;
                    } else if (value != 0) {
                        sticky = true;
                    }
                }
            }

            if (!has_hex_digit || (hex_i >= len)
                || ((buffer[hex_i] != 'p') && (buffer[hex_i] != 'P'))) {
                hex_i = 0;
            } else {
                exponent_index = hex_i;
                exponent_digit_index = exponent_index + 1;
                if ((exponent_digit_index < len)
                    && ((buffer[exponent_digit_index] == '-')
                        || (buffer[exponent_digit_index] == '+'))) {
                    exponent_negative =
                        buffer[exponent_digit_index] == '-';
                    exponent_digit_index += 1;
                }

                if ((exponent_digit_index >= len)
                    || (buffer[exponent_digit_index] < '0')
                    || (buffer[exponent_digit_index] > '9')) {
                    hex_i = 0;
                } else {
                    int64 exponent_limit = INT64_MAX/2;

                    hex_i = exponent_digit_index;
                    for (; hex_i < len; hex_i += 1) {
                        char c = buffer[hex_i];
                        int32 digit;

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
                    parsed_hex_len = hex_i;
                    if (exponent_negative) {
                        exponent = -exponent;
                    }
                    scale = exponent - 4*fractional_hex_digits;

                    if (!started) {
                        uint64 ieee = 0;

                        if (hex_negative) {
                            ieee = 1ull << 63;
                        }
                        *result = int64Bits2Double(ieee);
                        return parsed_hex_len;
                    }

                    top_exponent = scale + bit_len - 1;
                    if (top_exponent > 1023) {
                        uint64 ieee = 0x7ffull << DOUBLE_MANTISSA_BITS;

                        if (hex_negative) {
                            ieee |= 1ull << 63;
                        }
                        *result = int64Bits2Double(ieee);
                        return parsed_hex_len;
                    }

                    if (top_exponent >= -1022) {
                        uint64 significand;

                        // Normal numbers retain 53 significand bits.

                        if (bit_len <= 53) {
                            significand = prefix << (53 - bit_len);
                        } else {
                            int32 discarded_prefix_bits = prefix_bits - 53;
                            int32 lower_prefix_bits =
                                discarded_prefix_bits - 1;
                            uint64 guard;
                            bool lower_nonzero = sticky;

                            significand = prefix >> discarded_prefix_bits;
                            guard = (prefix >> lower_prefix_bits) &1;
                            if (lower_prefix_bits > 0) {
                                uint64 lower_mask =
                                    (1ull << lower_prefix_bits) - 1;

                                if ((prefix & lower_mask) != 0) {
                                    lower_nonzero = true;
                                }
                            }
                            if ((guard != 0)
                                && (lower_nonzero
                                    || ((significand &1) != 0))) {
                                significand += 1;
                            }
                            if (significand == (1ull << 53)) {
                                significand >>= 1;
                                top_exponent += 1;
                                if (top_exponent > 1023) {
                                    uint64 ieee =
                                        0x7ffull << DOUBLE_MANTISSA_BITS;

                                    if (hex_negative) {
                                        ieee |= 1ull << 63;
                                    }
                                    *result = int64Bits2Double(ieee);
                                    return parsed_hex_len;
                                }
                            }
                        }

                        {
                            uint64 exponent_bits =
                                (uint64)(top_exponent
                                         + DOUBLE_EXPONENT_BIAS);
                            uint64 mantissa_mask =
                                (1ull << DOUBLE_MANTISSA_BITS) - 1;
                            uint64 ieee =
                                (exponent_bits << DOUBLE_MANTISSA_BITS)
                                |(significand & mantissa_mask);

                            if (hex_negative) {
                                ieee |= 1ull << 63;
                            }
                            *result = int64Bits2Double(ieee);
                            return parsed_hex_len;
                        }
                    } else {
                        int64 right_shift = -1074 - scale;
                        uint64 significand = 0;

                        // Subnormals are integer multiples of 2^-1074.

                        if (right_shift <= 0) {
                            significand = prefix << -right_shift;
                        } else {
                            int64 kept_bits = bit_len - right_shift;

                            if (kept_bits > 0) {
                                int32 kept = (int32)kept_bits;
                                int32 discarded_prefix_bits =
                                    prefix_bits - kept;
                                int32 lower_prefix_bits =
                                    discarded_prefix_bits - 1;
                                uint64 guard;
                                bool lower_nonzero = sticky;

                                significand =
                                    prefix >> discarded_prefix_bits;
                                guard =
                                    (prefix >> lower_prefix_bits) &1;
                                if (lower_prefix_bits > 0) {
                                    uint64 lower_mask =
                                        (1ull << lower_prefix_bits) - 1;

                                    if ((prefix & lower_mask) != 0) {
                                        lower_nonzero = true;
                                    }
                                }
                                if ((guard != 0)
                                    && (lower_nonzero
                                        || ((significand &1) != 0))) {
                                    significand += 1;
                                }
                            } else if (kept_bits == 0) {
                                int32 lower_prefix_bits = prefix_bits - 1;
                                bool lower_nonzero = sticky;

                                if (lower_prefix_bits > 0) {
                                    uint64 lower_mask =
                                        (1ull << lower_prefix_bits) - 1;

                                    if ((prefix & lower_mask) != 0) {
                                        lower_nonzero = true;
                                    }
                                }
                                if (lower_nonzero) {
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
                            if (hex_negative) {
                                ieee |= 1ull << 63;
                            }
                            *result = int64Bits2Double(ieee);
                            return -FLOAT_UNDERFLOW;
                        }
                    }
                }
            }
        }

    }

    if ((i < len) && ((buffer[i] == '-') || (buffer[i] == '+'))) {
        signed_m = buffer[i] == '-';
        i += 1;
    }

    for (; i < len; i += 1) {
        char c = buffer[i];

        if ((c >= '0') && (c <= '9')) {
            has_mantissa_digit = true;
            if (significant_digits != 0 || c != '0') {
                significant_digits += 1;
            }
            if (significant_digits > 17) {
                extended = true;
            } else {
                m10 = 10*m10 + (uint64)(c - '0');
                if (m10 != 0) {
                    m10digits += 1;
                }
            }
            continue;
        }
        if ((c == '.') && (dot_index < 0)) {
            dot_index = i;
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
            e_index = exponent_index;
            i = exponent_index + 1;
            if ((buffer[i] == '-') || (buffer[i] == '+')) {
                signed_e = buffer[i] == '-';
                i += 1;
            }
            for (; i < len; i += 1) {
                char c = buffer[i];

                if ((c < '0') || (c > '9')) {
                    break;
                }
                if (parsed_exponent < 1000000000000ll) {
                    parsed_exponent = 10*parsed_exponent + (c - '0');
                    if (parsed_exponent > 1000000000000ll) {
                        parsed_exponent = 1000000000000ll;
                    }
                }
            }
        }
    }

    parsed_len = i;
    RyuDecimalParts parts = {
        .m10digits = m10digits,
        .significant_digits = significant_digits,
        .parsed_exponent = parsed_exponent,
        .dot_index = dot_index,
        .e_index = e_index,
        .m10 = m10,
        .signed_m = signed_m,
        .signed_e = signed_e,
        .extended = extended,
    };
    return ryu_decimal_finish(buffer, parsed_len, parts, result);
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
    bool has_digit = false;

    if (*cursor == '\0') {
        return -INPUT_TOO_SHORT;
    }
    if (*cursor == '-' || *cursor == '+') {
        parts.signed_m = *cursor == '-';
        cursor += 1;
    }

    if ((*cursor == 'i') || (*cursor == 'I') || (*cursor == 'n')
        || (*cursor == 'N')
        || (*cursor == '0' && (cursor[1] == 'x' || cursor[1] == 'X'))) {
        // Determine only the token length. The next byte may be the start
        // of another TSV cell, so strlen() would scan the entire file tail.
        const char *end = cursor;

        while ((*end >= '0' && *end <= '9')
               || (*end >= 'a' && *end <= 'z')
               || (*end >= 'A' && *end <= 'Z')
               || *end == '.' || *end == '+' || *end == '-') {
            end += 1;
        }
        if (end - buffer > INT32_MAX) {
            return -INPUT_TOO_LONG;
        }
        return s2d_n(buffer, (int32)(end - buffer), result);
    }

    for (;;) {
        char c = *cursor;

        if (c >= '0' && c <= '9') {
            has_digit = true;
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
            cursor += 1;
            continue;
        }
        if (c == '.' && parts.dot_index < 0) {
            parts.dot_index = (int32)(cursor - buffer);
            cursor += 1;
            continue;
        }
        break;
    }

    if (!has_digit) {
        return -MALFORMED_INPUT;
    }

    if (*cursor == 'e' || *cursor == 'E') {
        const char *digits = cursor + 1;

        if (*digits == '-' || *digits == '+') {
            digits += 1;
        }
        if (*digits >= '0' && *digits <= '9') {
            parts.e_index = (int32)(cursor - buffer);
            cursor += 1;
            if (*cursor == '-' || *cursor == '+') {
                parts.signed_e = *cursor == '-';
                cursor += 1;
            }
            while (*cursor >= '0' && *cursor <= '9') {
                int32 digit = *cursor - '0';

                if (parts.parsed_exponent < 1000000000000ll) {
                    parts.parsed_exponent =
                        10*parts.parsed_exponent + digit;
                    if (parts.parsed_exponent > 1000000000000ll) {
                        parts.parsed_exponent = 1000000000000ll;
                    }
                }
                cursor += 1;
            }
        }
    }
    if (cursor - buffer > INT32_MAX) {
        return -INPUT_TOO_LONG;
    }
    return ryu_decimal_finish(buffer, (int32)(cursor - buffer), parts, result);
}

int32
s2d(const char *buffer, double *result) {
    size_t len = strlen(buffer);

    if (len > INT32_MAX) {
        return -INPUT_TOO_LONG;
    }
    return s2d_n(buffer, (int32)len, result);
}
