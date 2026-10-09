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

int32
s2d_n(const char *buffer, int32 len, double *result) {
    int32 m10digits = 0;
    int32 e10digits = 0;
    int32 dot_index = -1;
    int32 e_index = -1;
    uint64 m10 = 0;
    int32 e10 = 0;
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

    if ((buffer[i] == '-') || (buffer[i] == '+')) {
        signed_m = buffer[i] == '-';
        i += 1;
    }

    for (; i < len; i += 1) {
        char c = buffer[i];

        if ((c >= '0') && (c <= '9')) {
            has_mantissa_digit = true;
            if (m10digits >= 17) {
                return -INPUT_TOO_LONG;
            }
            m10 = 10*m10 + (uint64)(c - '0');
            if (m10 != 0) {
                m10digits += 1;
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
                if (e10digits > 3) {
                    return -INPUT_TOO_LONG;
                }
                e10 = 10*e10 + (c - '0');
                if (e10 != 0) {
                    e10digits += 1;
                }
            }
        }
    }

    parsed_len = i;

    if (e_index < 0) {
        e_index = parsed_len;
    }
    if (dot_index < 0) {
        dot_index = e_index;
    }
    if (signed_e) {
        e10 = -e10;
    }
    e10 -= dot_index < e_index ? e_index - dot_index - 1 : 0;
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
s2d(const char *buffer, double *result) {
    size_t len = strlen(buffer);

    if (len > INT32_MAX) {
        return -INPUT_TOO_LONG;
    }
    return s2d_n(buffer, (int32)len, result);
}
