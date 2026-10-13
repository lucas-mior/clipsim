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

#if !defined(RYU_S2F_C)
#define RYU_S2F_C

#include "cbase.h"
#include "ryu/ryu_parse.h"

#if defined(RYU_DEBUG)
#include <inttypes.h>
#endif

#include "ryu/common.h"
#include "ryu/f2s_intrinsics.h"

#define FLOAT_MANTISSA_BITS 23
#define FLOAT_EXPONENT_BITS 8
#define FLOAT_EXPONENT_BIAS 127

#if defined(_MSC_VER)
#include <intrin.h>

static inline uint32
s2f_floor_log2(uint32 value) {
    unsigned long index;

    return _BitScanReverse(&index, value) ? (uint32)index : 32;
}

#else

static inline uint32
s2f_floor_log2(uint32 value) {
    return (uint32)(31 - __builtin_clz(value));
}

#endif

static inline float
int32_bits_to_float(uint32 bits) {
    float result;

    memcpy(&result, &bits, SIZEOF(result));
    return result;
}

// The binary32 counterpart of s2d_round_shift: no shift may reach 32.
static inline uint32
s2f_round_shift(uint32 value, int32 shift, bool trailing_zeros,
                bool *round_up) {
    uint32 truncated;
    uint32 last_removed_bit;

    assert(shift >= 0);
    assert(round_up);
    if (shift == 0) {
        *round_up = false;
        return value;
    }
    if (shift > 32) {
        *round_up = false;
        return 0;
    }
    if (shift == 32) {
        truncated = 0;
        last_removed_bit = value >> 31;
        trailing_zeros &= (value & ((1u << 31) - 1)) == 0;
    } else {
        truncated = value >> shift;
        last_removed_bit = (value >> (shift - 1)) & 1;
        trailing_zeros &= (value & ((1u << (shift - 1)) - 1)) == 0;
    }
    *round_up = (last_removed_bit != 0)
                && (!trailing_zeros || ((truncated & 1) != 0));
    return truncated + (uint32)*round_up;
}

enum Status
s2f_n(const char *buffer, int32 len, float *result) {
    int32 m10_digits;
    int32 e10_digits;
    int32 dot_index;
    int32 e_index;
    uint32 m10;
    int32 e10;
    bool signed_m;
    bool signed_e;
    int32 i;
    int32 e2;
    uint32 m2;
    bool trailing_zeros;
    int32 j;
    uint32 ieee_e2;
    int32 shift;
    bool round_up;
    uint32 ieee_m2;
    uint32 ieee;

    if (len == 0) {
        return INPUT_TOO_SHORT;
    }

    m10_digits = 0;
    e10_digits = 0;
    dot_index = len;
    e_index = len;
    m10 = 0;
    e10 = 0;
    signed_m = false;
    signed_e = false;
    i = 0;

    if (buffer[i] == '-') {
        signed_m = true;
        i += 1;
    }

    for (; i < len; i += 1) {
        char c;

        c = buffer[i];
        if (c == '.') {
            if (dot_index != len) {
                return MALFORMED_INPUT;
            }
            dot_index = i;
            continue;
        }
        if ((c < '0') || (c > '9')) {
            break;
        }
        if (m10_digits >= 9) {
            return INPUT_TOO_LONG;
        }
        m10 = 10*m10 + (uint32)(c - '0');
        if (m10 != 0) {
            m10_digits += 1;
        }
    }

    if ((i < len) && ((buffer[i] == 'e') || (buffer[i] == 'E'))) {
        e_index = i;
        i += 1;
        if ((i < len) && ((buffer[i] == '-') || (buffer[i] == '+'))) {
            signed_e = buffer[i] == '-';
            i += 1;
        }
        for (; i < len; i += 1) {
            char c;

            c = buffer[i];
            if ((c < '0') || (c > '9')) {
                return MALFORMED_INPUT;
            }
            if (e10_digits > 3) {
                // TODO: Be more lenient. Return +/-Infinity or +/-0 instead.
                return INPUT_TOO_LONG;
            }
            e10 = 10*e10 + (c - '0');
            if (e10 != 0) {
                e10_digits += 1;
            }
        }
    }

    if (i < len) {
        return MALFORMED_INPUT;
    }
    if (signed_e) {
        e10 = -e10;
    }
    e10 -= dot_index < e_index ? e_index - dot_index - 1 : 0;
    if (m10 == 0) {
        *result = signed_m ? -0.0f : 0.0f;
        return SUCCESS;
    }

#if defined(RYU_DEBUG)
    printf("Input=%s\n", buffer);
    printf("m10_digits = %d\n", m10_digits);
    printf("e10_digits = %d\n", e10_digits);
    printf("m10 * 10^e10 = %u * 10^%d\n", m10, e10);
#endif

    if ((m10_digits + e10 <= -46) || (m10 == 0)) {
        // Number is less than 1e-46 and should be rounded down to 0. Return
        // +/-0.0.
        ieee = ((uint32)signed_m)
               << (FLOAT_EXPONENT_BITS + FLOAT_MANTISSA_BITS);
        *result = int32_bits_to_float(ieee);
        return SUCCESS;
    }
    if (m10_digits + e10 >= 40) {
        // Number is larger than 1e+39 and should be rounded to +/-Infinity.
        ieee = (((uint32)signed_m)
                << (FLOAT_EXPONENT_BITS + FLOAT_MANTISSA_BITS))
               |(0xffu << FLOAT_MANTISSA_BITS);
        *result = int32_bits_to_float(ieee);
        return SUCCESS;
    }

    // Convert to binary float m2 * 2^e2, while retaining information about
    // whether the conversion was exact (trailing_zeros).
    if (e10 >= 0) {
        // The length of m * 10^e in bits is:
        //   log2(m10 * 10^e10) = log2(m10) + e10 log2(10)
        //                       = log2(m10) + e10 + e10 * log2(5)
        //
        // We want to compute the FLOAT_MANTISSA_BITS + 1 top-most bits (+1 for
        // the implicit leading one in IEEE format). We therefore choose a
        // binary output exponent of:
        //   log2(m10 * 10^e10) - (FLOAT_MANTISSA_BITS + 1).
        //
        // We use floor(log2(5^e10)) so that we get at least this many bits;
        // better to have an additional bit than to not have enough bits.
        e2 = (int32)s2f_floor_log2(m10) + e10 + log2_pow5(e10)
             - (FLOAT_MANTISSA_BITS + 1);

        // We now compute [m10 * 10^e10 / 2^e2]
        //     = [m10 * 5^e10 / 2^(e2-e10)].
        // To that end, we use the FLOAT_POW5_SPLIT table.
        j = e2 - e10 - ceil_log2_pow5(e10) + FLOAT_POW5_BITCOUNT;
        assert(j >= 0);
        m2 = mul_pow5_div_pow2(m10, (uint32)e10, j);

        // We also compute if the result is exact:
        //   [m10 * 10^e10 / 2^e2] == m10 * 10^e10 / 2^e2.
        // This can only be the case if 2^e2 divides m10 * 10^e10, which in
        // turn requires that the largest power of 2 that divides m10 + e10 is
        // greater than e2. If e2 is less than e10, the result must be exact.
        // Otherwise we use the existing multiple_of_power_of2_32 function.
        trailing_zeros = e2 < e10
                         || ((e2 - e10 < 32)
                             && multiple_of_power_of2_32(m10,
                                                         (uint32)(e2 - e10)));
    } else {
        e2 = (int32)s2f_floor_log2(m10) + e10 - ceil_log2_pow5(-e10)
             - (FLOAT_MANTISSA_BITS + 1);

        // We now compute [m10 * 10^e10 / 2^e2]
        //     = [m10 / (5^(-e10) 2^(e2-e10))].
        j = e2 - e10 + ceil_log2_pow5(-e10) - 1
            + FLOAT_POW5_INV_BITCOUNT;
        m2 = mul_pow5_inv_div_pow2(m10, (uint32)(-e10), j);

        // We also compute if the result is exact:
        //   [m10 / (5^(-e10) 2^(e2-e10))]
        //       == m10 / (5^(-e10) 2^(e2-e10))
        //
        // If e2-e10 >= 0, we need to check whether
        // (5^(-e10) 2^(e2-e10)) divides m10, which is the case iff pow5(m10)
        // >= -e10 AND pow2(m10) >= e2-e10.
        //
        // If e2-e10 < 0, we have actually computed
        // [m10 * 2^(e10 e2) / 5^(-e10)] above, and we need to check whether
        // 5^(-e10) divides (m10 * 2^(e10-e2)), which is the case iff
        // pow5(m10 * 2^(e10-e2)) = pow5(m10) >= -e10.
        trailing_zeros = (e2 < e10
                          || ((e2 - e10 < 32)
                              && multiple_of_power_of2_32(m10,
                                                          (uint32)(e2 - e10))))
                         && multiple_of_power_of5_32(m10, (uint32)(-e10));
    }

#if defined(RYU_DEBUG)
    printf("m2 * 2^e2 = %u * 2^%d\n", m2, e2);
#endif

    // Compute the final IEEE exponent.
    // Keep the exponent arithmetic signed: unsigned promotion would turn
    // negative subnormal exponents into large values (false infinities).
    ieee_e2 = (uint32)MAX(0, e2 + FLOAT_EXPONENT_BIAS
                            + (int32)s2f_floor_log2(m2));

    if (ieee_e2 > 0xfe) {
        // Final IEEE exponent is larger than the maximum representable. Return
        // +/-Infinity.
        ieee = (((uint32)signed_m)
                << (FLOAT_EXPONENT_BITS + FLOAT_MANTISSA_BITS))
               |(0xffu << FLOAT_MANTISSA_BITS);
        *result = int32_bits_to_float(ieee);
        return SUCCESS;
    }

    // We need to figure out how much we need to shift m2. The tricky part is
    // that we need to take the final IEEE exponent into account, so we need to
    // reverse the bias and also special-case the value 0.
    shift = (ieee_e2 == 0 ? 1 : (int32)ieee_e2) - e2
            - FLOAT_EXPONENT_BIAS - FLOAT_MANTISSA_BITS;
    assert(shift >= 0);

#if defined(RYU_DEBUG)
    printf("ieee_e2 = %d\n", ieee_e2);
    printf("shift = %d\n", shift);
#endif

    // Round to nearest, ties to even, including full-width shifts.
    ieee_m2 = s2f_round_shift(m2, shift, trailing_zeros, &round_up);

#if defined(RYU_DEBUG)
    printf("round_up = %d\n", round_up);
    printf("ieee_m2 = %u\n", ieee_m2);
#endif

    assert(ieee_m2 <= (1u << (FLOAT_MANTISSA_BITS + 1)));
    ieee_m2 &= (1u << FLOAT_MANTISSA_BITS) - 1;
    if ((ieee_m2 == 0) && round_up) {
        // Rounding up may overflow the mantissa. In this case we move a
        // trailing zero of the mantissa into the exponent. Due to how the IEEE
        // represents +/-Infinity, we don't need to check for overflow here.
        ieee_e2 += 1;
    }

    ieee = (((((uint32)signed_m) << FLOAT_EXPONENT_BITS) | (uint32)ieee_e2)
            << FLOAT_MANTISSA_BITS)
           |ieee_m2;
    *result = int32_bits_to_float(ieee);
    return SUCCESS;
}

enum Status
s2f(const char *buffer, float *result) {
    int32 len;

    len = (int32)strlen(buffer);
    return s2f_n(buffer, len, result);
}

#endif /* RYU_S2F_C */
