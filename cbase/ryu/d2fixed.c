// Copyright 2018 Ulf Adams
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

// Runtime compiler options:
// -DRYU_DEBUG Generate verbose debugging output to stdout.
//
// -DRYU_ONLY_64_BIT_OPS Avoid using uint128 or 64-bit intrinsics. Slower,
//     depending on your compiler.
//
// -DRYU_AVOID_UINT128 Avoid using uint128. Slower, depending on your compiler.

#if !defined(RYU_D2FIXED_C)
#define RYU_D2FIXED_C

#include "cbase.h"
#include "ryu/ryu.h"

#if defined(RYU_DEBUG)
#include <inttypes.h>
#endif

#include "ryu/common.h"
#include "ryu/digit_table.h"
#include "ryu/d2fixed_full_table.h"
#include "ryu/d2s_intrinsics.h"

#define DOUBLE_MANTISSA_BITS 52
#define DOUBLE_EXPONENT_BITS 11
#define DOUBLE_BIAS 1023

#define POW10_ADDITIONAL_BITS 120

#if defined(HAS_UINT128)
static inline uint128
umul256(uint128 a, uint64 b_hi, uint64 b_lo, uint128 *product_hi) {
    uint64 a_lo;
    uint64 a_hi;
    uint128 b00;
    uint128 b01;
    uint128 b10;
    uint128 b11;
    uint64 b00_lo;
    uint64 b00_hi;
    uint128 mid1;
    uint64 mid1_lo;
    uint64 mid1_hi;
    uint128 mid2;
    uint64 mid2_lo;
    uint64 mid2_hi;
    uint128 p_hi;
    uint128 p_lo;

    a_lo = (uint64)a;
    a_hi = (uint64)(a >> 64);

    b00 = (uint128)a_lo*b_lo;
    b01 = (uint128)a_lo*b_hi;
    b10 = (uint128)a_hi*b_lo;
    b11 = (uint128)a_hi*b_hi;

    b00_lo = (uint64)b00;
    b00_hi = (uint64)(b00 >> 64);

    mid1 = b10 + b00_hi;
    mid1_lo = (uint64)mid1;
    mid1_hi = (uint64)(mid1 >> 64);

    mid2 = b01 + mid1_lo;
    mid2_lo = (uint64)mid2;
    mid2_hi = (uint64)(mid2 >> 64);

    p_hi = b11 + mid1_hi + mid2_hi;
    p_lo = ((uint128)mid2_lo << 64) | b00_lo;

    *product_hi = p_hi;
    return p_lo;
}

// Returns the high 128 bits of the 256-bit product of a and b.
static inline uint128
umul256_hi(uint128 a, uint64 b_hi, uint64 b_lo) {
    uint128 hi;

    // Reuse the umul256 implementation. Optimizers will likely eliminate the
    // instructions used to compute the low part of the product.
    umul256(a, b_hi, b_lo, &hi);
    return hi;
}

// Unfortunately, gcc/clang do not automatically turn a 128-bit integer
// division into a multiplication, so we have to do it manually.
static inline uint32
uint128_mod1e9(uint128 value) {
    uint64 multiplied;
    uint32 shifted;

    // After multiplying, we're going to shift right by 29, then truncate to
    // uint32. This means that we need only 29 + 32 = 61 bits, so we can
    // truncate to uint64 before shifting.
    multiplied = (uint64)umul256_hi(value, 0x89705F4136B4A597u,
                                    0x31680A88F8953031u);

    // For uint32 truncation, see the mod1e9() comment in d2s_intrinsics.h.
    shifted = (uint32)(multiplied >> 29);

    return (uint32)value - 1000000000*shifted;
}

// Best case: use 128-bit type.
static inline uint32
mul_shift_mod1e9(uint64 m, uint64 const *mul, int32 j) {
    uint128 b0;
    uint128 b1;
    uint128 b2;
    uint128 mid;
    uint128 s1;

    b0 = (uint128)m*mul[0]; // 0
    b1 = (uint128)m*mul[1]; // 64
    b2 = (uint128)m*mul[2]; // 128
    mid = b1 + (uint64)(b0 >> 64); // 64
    s1 = b2 + (uint64)(mid >> 64); // 128

#if defined(RYU_DEBUG)
    if ((j < 128) || (j > 180)) {
        fmt_printf("%d\n", j);
    }
#endif

    assert(j >= 128);
    assert(j <= 180);

    // j: [128, 256)
    return uint128_mod1e9(s1 >> (j - 128));
}
#else /* HAS_UINT128 */
#if defined(HAS_64_BIT_INTRINSICS)
// Returns the low 64 bits of the high 128 bits of the 256-bit product of a
// and b.
static inline uint64
umul256_hi128_lo64(uint64 a_hi, uint64 a_lo,
                   uint64 b_hi, uint64 b_lo) {
    uint64 b00_hi;
    uint64 b00_lo;
    uint64 b01_hi;
    uint64 b01_lo;
    uint64 b10_hi;
    uint64 b10_lo;
    uint64 b11_hi;
    uint64 b11_lo;
    uint64 temp1_lo;
    uint64 temp1_hi;
    uint64 temp2_lo;
    uint64 temp2_hi;

    b00_lo = umul128(a_lo, b_lo, &b00_hi);
    b01_lo = umul128(a_lo, b_hi, &b01_hi);
    b10_lo = umul128(a_hi, b_lo, &b10_hi);
    b11_lo = umul128(a_hi, b_hi, &b11_hi);
    (void)b00_lo;
    (void)b11_hi;

    temp1_lo = b10_lo + b00_hi;
    temp1_hi = b10_hi + (temp1_lo < b10_lo);
    temp2_lo = b01_lo + temp1_lo;
    temp2_hi = b01_hi + (temp2_lo < b01_lo);
    return b11_lo + temp1_hi + temp2_hi;
}

static inline uint32
uint128_mod1e9(uint64 v_hi, uint64 v_lo) {
    uint64 multiplied;
    uint32 shifted;

    // After multiplying, we're going to shift right by 29, then truncate to
    // uint32. This means that we need only 29 + 32 = 61 bits, so we can
    // truncate to uint64 before shifting.
    multiplied = umul256_hi128_lo64(v_hi, v_lo, 0x89705F4136B4A597u,
                                    0x31680A88F8953031u);

    // For uint32 truncation, see the mod1e9() comment in d2s_intrinsics.h.
    shifted = (uint32)(multiplied >> 29);

    return (uint32)v_lo - 1000000000*shifted;
}
#endif /* HAS_64_BIT_INTRINSICS */

static inline uint32
mul_shift_mod1e9(uint64 m, uint64 const *mul, int32 j) {
    uint64 high0;
    uint64 low0;
    uint64 high1;
    uint64 low1;
    uint64 high2;
    uint64 low2;
    uint64 s0_high;
    uint32 c1;
    uint64 s1_low;
    uint32 c2;
    uint64 s1_high;

    low0 = umul128(m, mul[0], &high0); // 0
    low1 = umul128(m, mul[1], &high1); // 64
    low2 = umul128(m, mul[2], &high2); // 128
    s0_high = low1 + high0; // 64
    c1 = s0_high < low1;
    s1_low = low2 + high1 + c1; // 128
    c2 = s1_low < low2; // high1 + c1 can't overflow, compare against low2.
    s1_high = high2 + c2; // 192
    (void)low0;

#if defined(RYU_DEBUG)
    if ((j < 128) || (j > 180)) {
        fmt_printf("%d\n", j);
    }
#endif

    assert(j >= 128);
    assert(j <= 180);

#if defined(HAS_64_BIT_INTRINSICS)
    {
        uint32 dist;
        uint64 shifted_high;
        uint64 shifted_low;

        dist = (uint32)(j - 128); // dist: [0, 52]
        shifted_high = s1_high >> dist;
        shifted_low = shift_right128(s1_low, s1_high, dist);
        return uint128_mod1e9(shifted_high, shifted_low);
    }
#else /* HAS_64_BIT_INTRINSICS */
    if (j < 160) {
        uint64 r0;
        uint64 r1;
        uint64 r2;

        // j: [128, 160)
        r0 = mod1e9(s1_high);
        r1 = mod1e9((r0 << 32) | (s1_low >> 32));
        r2 = (r1 << 32) | (s1_low & 0xffffffff);
        return mod1e9(r2 >> (j - 128));
    } else {
        uint64 r0;
        uint64 r1;

        // j: [160, 192)
        r0 = mod1e9(s1_high);
        r1 = (r0 << 32) | (s1_low >> 32);
        return mod1e9(r1 >> (j - 160));
    }
#endif /* HAS_64_BIT_INTRINSICS */
}
#endif /* HAS_UINT128 */

// Convert `digits` to a sequence of decimal digits. Append the digits to the
// result. The caller has to guarantee that:
//   10^(olength-1) <= digits < 10^olength
// e.g., by passing `olength` as `decimal_length9(digits)`.
static inline void
append_n_digits(uint32 olength, uint32 digits, char *result) {
    uint32 i = 0;
    uint32 c;
    uint32 c0;
    uint32 c1;

#if defined(RYU_DEBUG)
    fmt_printf("DIGITS=%u\n", digits);
#endif

    while (digits >= 10000) {
#if defined(__clang__) // https://bugs.llvm.org/show_bug.cgi?id=38217
        c = digits - 10000*(digits / 10000);
#else
        c = digits % 10000;
#endif
        digits /= 10000;
        c0 = (c % 100) << 1;
        c1 = (c / 100) << 1;
        memcpy(result + olength - i - 2, ryu_digit_table + c0, 2);
        memcpy(result + olength - i - 4, ryu_digit_table + c1, 2);
        i += 4;
    }
    if (digits >= 100) {
        c = (digits % 100) << 1;
        digits /= 100;
        memcpy(result + olength - i - 2, ryu_digit_table + c, 2);
        i += 2;
    }
    if (digits >= 10) {
        c = digits << 1;
        memcpy(result + olength - i - 2, ryu_digit_table + c, 2);
    } else {
        result[0] = (char)('0' + digits);
    }
    return;
}

// Convert `digits` to a sequence of decimal digits. Print the first digit,
// followed by a decimal dot '.' followed by the remaining digits. The caller
// has to guarantee that:
//   10^(olength-1) <= digits < 10^olength
// e.g., by passing `olength` as `decimal_length9(digits)`.
static inline void
append_d_digits(uint32 olength, uint32 digits, char *result) {
    uint32 i = 0;
    uint32 c;
    uint32 c0;
    uint32 c1;

#if defined(RYU_DEBUG)
    fmt_printf("DIGITS=%u\n", digits);
#endif

    while (digits >= 10000) {
#if defined(__clang__) // https://bugs.llvm.org/show_bug.cgi?id=38217
        c = digits - 10000*(digits / 10000);
#else
        c = digits % 10000;
#endif
        digits /= 10000;
        c0 = (c % 100) << 1;
        c1 = (c / 100) << 1;
        memcpy(result + olength + 1 - i - 2, ryu_digit_table + c0, 2);
        memcpy(result + olength + 1 - i - 4, ryu_digit_table + c1, 2);
        i += 4;
    }
    if (digits >= 100) {
        c = (digits % 100) << 1;
        digits /= 100;
        memcpy(result + olength + 1 - i - 2, ryu_digit_table + c, 2);
    }
    if (digits >= 10) {
        c = digits << 1;
        result[2] = ryu_digit_table[c + 1];
        result[1] = '.';
        result[0] = ryu_digit_table[c];
    } else {
        result[1] = '.';
        result[0] = (char)('0' + digits);
    }
    return;
}

// Convert `digits` to decimal and write the last `count` decimal digits to
// result. If `digits` contains additional digits, then those are silently
// ignored.
static inline void
append_c_digits(uint32 count, uint32 digits, char *result) {
    uint32 i = 0;

#if defined(RYU_DEBUG)
    fmt_printf("DIGITS=%u\n", digits);
#endif

    // Copy pairs of digits from ryu_digit_table.
    for (; i < count - 1; i += 2) {
        uint32 c;

        c = (digits % 100) << 1;
        digits /= 100;
        memcpy(result + count - i - 2, ryu_digit_table + c, 2);
    }

    // Generate the last digit if count is odd.
    if (i < count) {
        char c;

        c = (char)('0' + (digits % 10));
        result[count - i - 1] = c;
    }
    return;
}

// Convert `digits` to decimal and write the last 9 decimal digits to result.
// If `digits` contains additional digits, then those are silently ignored.
static inline void
append_nine_digits(uint32 digits, char *result) {
    uint32 c;
    uint32 c0;
    uint32 c1;

#if defined(RYU_DEBUG)
    fmt_printf("DIGITS=%u\n", digits);
#endif

    if (digits == 0) {
        memset(result, '0', 9);
        return;
    }

    for (uint32 i = 0; i < 5; i += 4) {
#if defined(__clang__) // https://bugs.llvm.org/show_bug.cgi?id=38217
        c = digits - 10000*(digits / 10000);
#else
        c = digits % 10000;
#endif
        digits /= 10000;
        c0 = (c % 100) << 1;
        c1 = (c / 100) << 1;
        memcpy(result + 7 - i, ryu_digit_table + c0, 2);
        memcpy(result + 5 - i, ryu_digit_table + c1, 2);
    }
    result[0] = (char)('0' + digits);
    return;
}

static inline uint32
index_for_exponent(uint32 e) {
    return (e + 15) / 16;
}

static inline uint32
pow10_bits_for_index(uint32 idx) {
    return 16*idx + POW10_ADDITIONAL_BITS;
}

static inline uint32
length_for_index(uint32 idx) {
    // +1 for ceil, +16 for mantissa, +8 to round up when dividing by 9.
    return (log10_pow2(16*(int32)idx) + 1 + 16 + 8) / 9;
}

static inline int
copy_special_str_printf(char *result, bool sign, uint64 mantissa) {
#if defined(_MSC_VER)
    // TODO: Check that -nan is expected output on Windows.
    if (sign) {
        result[0] = '-';
    }
    if (mantissa) {
        if (mantissa < (1ull << (DOUBLE_MANTISSA_BITS - 1))) {
            memcpy(result + sign, "nan(snan)", 9);
            return sign + 9;
        }
        memcpy(result + sign, "nan", 3);
        return sign + 3;
    }
#else
    if (mantissa) {
        memcpy(result, "nan", 3);
        return 3;
    }
    if (sign) {
        result[0] = '-';
    }
#endif
    memcpy(result + sign, "Infinity", 8);
    return sign + 8;
}

int
d2fixed_buffered_n(double d, uint32 precision, char *result) {
    uint64 bits;
    bool ieee_sign;
    uint64 ieee_mantissa;
    uint32 ieee_exponent;
    int32 e2;
    uint64 m2;
    int index;
    bool nonzero;
    int round_up;
    uint32 last_digit;

    bits = double_to_bits(d);
#if defined(RYU_DEBUG)
    fmt_printf("IN=");
    for (int32 bit = 63; bit >= 0; bit -= 1) {
        fmt_printf("%d", (int)((bits >> bit) & 1));
    }
    fmt_printf("\n");
#endif

    // Decode bits into sign, mantissa, and exponent.
    ieee_sign = ((bits >> (DOUBLE_MANTISSA_BITS + DOUBLE_EXPONENT_BITS))
                 & 1) != 0;
    ieee_mantissa = bits & ((1ull << DOUBLE_MANTISSA_BITS) - 1);
    ieee_exponent = (uint32)((bits >> DOUBLE_MANTISSA_BITS)
                             & ((1u << DOUBLE_EXPONENT_BITS) - 1));

    // Case distinction; exit early for the easy cases.
    if (ieee_exponent == ((1u << DOUBLE_EXPONENT_BITS) - 1u)) {
        return copy_special_str_printf(result, ieee_sign, ieee_mantissa);
    }
    if ((ieee_exponent == 0) && (ieee_mantissa == 0)) {
        index = 0;
        if (ieee_sign) {
            result[index] = '-';
            index += 1;
        }
        result[index] = '0';
        index += 1;
        if (precision > 0) {
            result[index] = '.';
            index += 1;
            memset(result + index, '0', precision);
            index += precision;
        }
        return index;
    }

    if (ieee_exponent == 0) {
        e2 = 1 - DOUBLE_BIAS - DOUBLE_MANTISSA_BITS;
        m2 = ieee_mantissa;
    } else {
        e2 = (int32)ieee_exponent - DOUBLE_BIAS - DOUBLE_MANTISSA_BITS;
        m2 = (1ull << DOUBLE_MANTISSA_BITS) | ieee_mantissa;
    }

#if defined(RYU_DEBUG)
    fmt_printf("-> %" PRIu64 " * 2^%d\n", m2, e2);
#endif

    index = 0;
    nonzero = false;
    if (ieee_sign) {
        result[index] = '-';
        index += 1;
    }

    if (e2 >= -52) {
        uint32 idx;
        uint32 p10bits;
        int32 len;

        idx = e2 < 0 ? 0 : index_for_exponent((uint32)e2);
        p10bits = pow10_bits_for_index(idx);
        len = (int32)length_for_index(idx);
#if defined(RYU_DEBUG)
        fmt_printf("idx=%u\n", idx);
        fmt_printf("len=%d\n", len);
#endif
        for (int32 i = len - 1; i >= 0; i -= 1) {
            int32 j;
            uint32 digits;

            j = (int32)p10bits - e2;
            // Temporary: j is usually around 128, and by shifting a bit, we
            // push it to 128 or above, which is a slightly faster code path in
            // mul_shift_mod1e9. Instead, we can just increase the multipliers.
            digits = mul_shift_mod1e9(m2 << 8,
                                      pow10_split[pow10_offset[idx] + i],
                                      j + 8);
            if (nonzero) {
                append_nine_digits(digits, result + index);
                index += 9;
            } else if (digits != 0) {
                uint32 olength;

                olength = decimal_length9(digits);
                append_n_digits(olength, digits, result + index);
                index += olength;
                nonzero = true;
            }
        }
    }

    if (!nonzero) {
        result[index] = '0';
        index += 1;
    }
    if (precision > 0) {
        result[index] = '.';
        index += 1;
    }

#if defined(RYU_DEBUG)
    fmt_printf("e2=%d\n", e2);
#endif

    if (e2 < 0) {
        int32 idx;
        uint32 blocks;
        uint32 i = 0;

        idx = -e2 / 16;
#if defined(RYU_DEBUG)
        fmt_printf("idx=%d\n", idx);
#endif
        blocks = precision / 9 + 1;

        // 0 = don't round up; 1 = round up unconditionally; 2 = round up if
        // odd.
        round_up = 0;
        if (blocks <= min_block_2[idx]) {
            i = blocks;
            memset(result + index, '0', precision);
            index += precision;
        } else if (i < min_block_2[idx]) {
            i = min_block_2[idx];
            memset(result + index, '0', 9*i);
            index += 9*i;
        }
        for (; i < blocks; i += 1) {
            uint32 digits;
            int32 j;
            uint32 p;

            j = POW10_ADDITIONAL_BITS_2 + (-e2 - 16*idx);
            p = pow10_offset_2[idx] + i - min_block_2[idx];
            if (p >= pow10_offset_2[idx + 1]) {
                uint32 fill;

                // If the remaining digits are all 0, then we might as well
                // use memset. No rounding required in this case.
                fill = precision - 9*i;
                memset(result + index, '0', fill);
                index += fill;
                break;
            }
            // Temporary: j is usually around 128, and by shifting a bit, we
            // push it to 128 or above, which is a slightly faster code path in
            // mul_shift_mod1e9. Instead, we can just increase the multipliers.
            digits = mul_shift_mod1e9(m2 << 8, pow10_split_2[p], j + 8);
#if defined(RYU_DEBUG)
            fmt_printf("digits=%u\n", digits);
#endif
            if (i < blocks - 1) {
                append_nine_digits(digits, result + index);
                index += 9;
            } else {
                uint32 maximum;

                maximum = precision - 9*i;
                last_digit = 0;
                for (uint32 k = 0; k < 9 - maximum; k += 1) {
                    last_digit = digits % 10;
                    digits /= 10;
                }
#if defined(RYU_DEBUG)
                fmt_printf("lastDigit=%u\n", last_digit);
#endif
                if (last_digit != 5) {
                    round_up = last_digit > 5;
                } else {
                    int32 required_twos;
                    bool trailing_zeros;

                    // Is m * 10^(additionalDigits + 1) / 2^(-e2) integer?
                    required_twos = -e2 - (int32)precision - 1;
                    trailing_zeros = required_twos <= 0;
                    if (!trailing_zeros && (required_twos < 60)) {
                        uint32 twos;

                        twos = (uint32)required_twos;
                        trailing_zeros = multiple_of_power_of2(m2, twos);
                    }
                    round_up = trailing_zeros ? 2 : 1;
#if defined(RYU_DEBUG)
                    fmt_printf("requiredTwos=%d\n", required_twos);
                    fmt_printf("trailingZeros=%s\n",
                               trailing_zeros ? "true" : "false");
#endif
                }
                if (maximum > 0) {
                    append_c_digits(maximum, digits, result + index);
                    index += maximum;
                }
                break;
            }
        }
#if defined(RYU_DEBUG)
        fmt_printf("roundUp=%d\n", round_up);
#endif
        if (round_up != 0) {
            int round_index;
            int dot_index = 0; // '.' can't be located at index 0.
            char c;

            round_index = index;
            for (;;) {
                round_index -= 1;
                if (round_index == -1) {
                    result[round_index + 1] = '1';
                    if (dot_index > 0) {
                        result[dot_index] = '0';
                        result[dot_index + 1] = '.';
                    }
                    result[index] = '0';
                    index += 1;
                    break;
                }
                c = result[round_index];
                if (c == '-') {
                    result[round_index + 1] = '1';
                    if (dot_index > 0) {
                        result[dot_index] = '0';
                        result[dot_index + 1] = '.';
                    }
                    result[index] = '0';
                    index += 1;
                    break;
                }
                if (c == '.') {
                    dot_index = round_index;
                    continue;
                } else if (c == '9') {
                    result[round_index] = '0';
                    round_up = 1;
                    continue;
                } else {
                    if ((round_up == 2) && (c % 2 == 0)) {
                        break;
                    }
                    result[round_index] = c + 1;
                    break;
                }
            }
        }
    } else {
        memset(result + index, '0', precision);
        index += precision;
    }
    return index;
}

void
d2fixed_buffered(double d, uint32 precision, char *result) {
    int len;

    len = d2fixed_buffered_n(d, precision, result);
    result[len] = '\0';
    return;
}

char *
d2fixed(double d, uint32 precision) {
    char *buffer;
    int index;

    buffer = (char *)malloc(2000);
    index = d2fixed_buffered_n(d, precision, buffer);
    buffer[index] = '\0';
    return buffer;
}


int
d2exp_buffered_n(double d, uint32 precision, char *result) {
    uint64 bits;
    bool ieee_sign;
    uint64 ieee_mantissa;
    uint32 ieee_exponent;
    int32 e2;
    uint64 m2;
    bool print_decimal_point;
    int index;
    uint32 digits;
    uint32 printed_digits;
    uint32 available_digits;
    uint32 maximum;
    uint32 last_digit;
    int round_up;
    int32 exp;

    bits = double_to_bits(d);
#if defined(RYU_DEBUG)
    fmt_printf("IN=");
    for (int32 bit = 63; bit >= 0; bit -= 1) {
        fmt_printf("%d", (int)((bits >> bit) & 1));
    }
    fmt_printf("\n");
#endif

    // Decode bits into sign, mantissa, and exponent.
    ieee_sign = ((bits >> (DOUBLE_MANTISSA_BITS + DOUBLE_EXPONENT_BITS)) & 1)
                != 0;
    ieee_mantissa = bits & ((1ull << DOUBLE_MANTISSA_BITS) - 1);
    ieee_exponent = (uint32)((bits >> DOUBLE_MANTISSA_BITS)
                             & ((1u << DOUBLE_EXPONENT_BITS) - 1));

    // Case distinction; exit early for the easy cases.
    if (ieee_exponent == ((1u << DOUBLE_EXPONENT_BITS) - 1u)) {
        return copy_special_str_printf(result, ieee_sign, ieee_mantissa);
    }
    if ((ieee_exponent == 0) && (ieee_mantissa == 0)) {
        index = 0;
        if (ieee_sign) {
            result[index] = '-';
            index += 1;
        }
        result[index] = '0';
        index += 1;
        if (precision > 0) {
            result[index] = '.';
            index += 1;
            memset(result + index, '0', precision);
            index += precision;
        }
        memcpy(result + index, "e+00", 4);
        index += 4;
        return index;
    }

    if (ieee_exponent == 0) {
        e2 = 1 - DOUBLE_BIAS - DOUBLE_MANTISSA_BITS;
        m2 = ieee_mantissa;
    } else {
        e2 = (int32)ieee_exponent - DOUBLE_BIAS - DOUBLE_MANTISSA_BITS;
        m2 = (1ull << DOUBLE_MANTISSA_BITS) | ieee_mantissa;
    }

#if defined(RYU_DEBUG)
    fmt_printf("-> %" PRIu64 " * 2^%d\n", m2, e2);
#endif

    print_decimal_point = precision > 0;
    precision += 1;
    index = 0;
    if (ieee_sign) {
        result[index] = '-';
        index += 1;
    }
    digits = 0;
    printed_digits = 0;
    available_digits = 0;
    exp = 0;
    if (e2 >= -52) {
        uint32 idx;
        uint32 p10bits;
        int32 len;

        idx = e2 < 0 ? 0 : index_for_exponent((uint32)e2);
        p10bits = pow10_bits_for_index(idx);
        len = (int32)length_for_index(idx);
#if defined(RYU_DEBUG)
        fmt_printf("idx=%u\n", idx);
        fmt_printf("len=%d\n", len);
#endif
        for (int32 i = len - 1; i >= 0; i -= 1) {
            int32 j;

            j = (int32)p10bits - e2;
            // Temporary: j is usually around 128, and by shifting a bit, we
            // push it to 128 or above, which is a slightly faster code path in
            // mul_shift_mod1e9. Instead, we can just increase the multipliers.
            digits = mul_shift_mod1e9(m2 << 8,
                                      pow10_split[pow10_offset[idx] + i],
                                      j + 8);
            if (printed_digits != 0) {
                if (printed_digits + 9 > precision) {
                    available_digits = 9;
                    break;
                }
                append_nine_digits(digits, result + index);
                index += 9;
                printed_digits += 9;
            } else if (digits != 0) {
                available_digits = decimal_length9(digits);
                exp = i*9 + (int32)available_digits - 1;
                if (available_digits > precision) {
                    break;
                }
                if (print_decimal_point) {
                    append_d_digits(available_digits, digits, result + index);
                    index += available_digits + 1; // +1 for decimal point.
                } else {
                    result[index] = (char)('0' + digits);
                    index += 1;
                }
                printed_digits = available_digits;
                available_digits = 0;
            }
        }
    }

    if ((e2 < 0) && (available_digits == 0)) {
        int32 idx;

        idx = -e2 / 16;
#if defined(RYU_DEBUG)
        fmt_printf("idx=%d, e2=%d, min=%d\n", idx, e2, min_block_2[idx]);
#endif
        for (int32 i = min_block_2[idx]; i < 200; i += 1) {
            int32 j;
            uint32 p;

            j = POW10_ADDITIONAL_BITS_2 + (-e2 - 16*idx);
            p = pow10_offset_2[idx] + (uint32)i - min_block_2[idx];
            // Temporary: j is usually around 128, and by shifting a bit, we
            // push it to 128 or above, which is a slightly faster code path in
            // mul_shift_mod1e9. Instead, we can just increase the multipliers.
            if (p >= pow10_offset_2[idx + 1]) {
                digits = 0;
            } else {
                digits = mul_shift_mod1e9(m2 << 8, pow10_split_2[p], j + 8);
            }
#if defined(RYU_DEBUG)
            fmt_printf("exact=%" PRIu64 " * (%" PRIu64 " + %" PRIu64
                       " << 64) >> %d\n",
                       m2, pow10_split_2[p][0], pow10_split_2[p][1], j);
            fmt_printf("digits=%u\n", digits);
#endif
            if (printed_digits != 0) {
                if (printed_digits + 9 > precision) {
                    available_digits = 9;
                    break;
                }
                append_nine_digits(digits, result + index);
                index += 9;
                printed_digits += 9;
            } else if (digits != 0) {
                available_digits = decimal_length9(digits);
                exp = -(i + 1)*9 + (int32)available_digits - 1;
                if (available_digits > precision) {
                    break;
                }
                if (print_decimal_point) {
                    append_d_digits(available_digits, digits, result + index);
                    index += available_digits + 1; // +1 for decimal point.
                } else {
                    result[index] = (char)('0' + digits);
                    index += 1;
                }
                printed_digits = available_digits;
                available_digits = 0;
            }
        }
    }

    maximum = precision - printed_digits;
#if defined(RYU_DEBUG)
    fmt_printf("availableDigits=%u\n", available_digits);
    fmt_printf("digits=%u\n", digits);
    fmt_printf("maximum=%u\n", maximum);
#endif
    if (available_digits == 0) {
        digits = 0;
    }
    last_digit = 0;
    if (available_digits > maximum) {
        uint32 remove_count;

        remove_count = available_digits - maximum;
        for (uint32 k = 0; k < remove_count; k += 1) {
            last_digit = digits % 10;
            digits /= 10;
        }
    }
#if defined(RYU_DEBUG)
    fmt_printf("lastDigit=%u\n", last_digit);
#endif
    // 0 = don't round up; 1 = round up unconditionally; 2 = round up if odd.
    if (last_digit != 5) {
        round_up = last_digit > 5;
    } else {
        int32 rexp;
        int32 required_twos;
        bool trailing_zeros;

        // Is m * 2^e2 * 10^(precision + 1 - exp) integer? Precision was
        // already increased by 1, so we don't need to write + 1 here.
        rexp = (int32)precision - exp;
        required_twos = -e2 - rexp;
        trailing_zeros = required_twos <= 0
                         || ((required_twos < 60)
                             && multiple_of_power_of2(m2,
                                                      (uint32)required_twos));
        if (rexp < 0) {
            int32 required_fives;

            required_fives = -rexp;
            trailing_zeros = trailing_zeros
                             && multiple_of_power_of5(m2,
                                                      (uint32)required_fives);
        }
        round_up = trailing_zeros ? 2 : 1;
#if defined(RYU_DEBUG)
        fmt_printf("requiredTwos=%d\n", required_twos);
        fmt_printf("trailingZeros=%s\n", trailing_zeros ? "true" : "false");
#endif
    }
    if (printed_digits != 0) {
        if (digits == 0) {
            memset(result + index, '0', maximum);
        } else {
            append_c_digits(maximum, digits, result + index);
        }
        index += maximum;
    } else {
        if (print_decimal_point) {
            append_d_digits(maximum, digits, result + index);
            index += maximum + 1; // +1 for decimal point.
        } else {
            result[index] = (char)('0' + digits);
            index += 1;
        }
    }
#if defined(RYU_DEBUG)
    fmt_printf("roundUp=%d\n", round_up);
#endif
    if (round_up != 0) {
        int round_index;
        char c;

        round_index = index;
        for (;;) {
            round_index -= 1;
            if (round_index == -1) {
                result[round_index + 1] = '1';
                exp += 1;
                break;
            }
            c = result[round_index];
            if (c == '-') {
                result[round_index + 1] = '1';
                exp += 1;
                break;
            }
            if (c == '.') {
                continue;
            } else if (c == '9') {
                result[round_index] = '0';
                round_up = 1;
                continue;
            } else {
                if ((round_up == 2) && (c % 2 == 0)) {
                    break;
                }
                result[round_index] = c + 1;
                break;
            }
        }
    }
    result[index] = 'e';
    index += 1;
    if (exp < 0) {
        result[index] = '-';
        index += 1;
        exp = -exp;
    } else {
        result[index] = '+';
        index += 1;
    }

    if (exp >= 100) {
        int32 c;

        c = exp % 100;
        result[index] = (char)('0' + exp/100);
        memcpy(result + index + 1, ryu_digit_table + 2*c, 2);
        index += 3;
    } else {
        memcpy(result + index, ryu_digit_table + 2*exp, 2);
        index += 2;
    }

    return index;
}

void
d2exp_buffered(double d, uint32 precision, char *result) {
    int len;

    len = d2exp_buffered_n(d, precision, result);
    result[len] = '\0';
    return;
}

char *
d2exp(double d, uint32 precision) {
    char *buffer;
    int index;

    buffer = (char *)malloc(2000);
    index = d2exp_buffered_n(d, precision, buffer);
    buffer[index] = '\0';
    return buffer;
}

#endif /* RYU_D2FIXED_C */
