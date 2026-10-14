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

#if !defined(RYU_F2S_C)
#define RYU_F2S_C

#include "cbase.h"
#include "ryu/ryu.h"

#include "ryu/common.h"
#include "ryu/digit_table.h"
#include "ryu/f2s_intrinsics.h"

#define FLOAT_MANTISSA_BITS 23
#define FLOAT_EXPONENT_BITS 8
#define FLOAT_BIAS 127

// A floating decimal representing m * 10^e.
typedef struct FloatingDecimal32 {
    uint32 mantissa;

    // Decimal exponent's range is -45 to 38 inclusive, and can fit in a short
    // if needed.
    int32 exponent;
} FloatingDecimal32;

static inline FloatingDecimal32
f2s_to_decimal(uint32 ieee_mantissa, uint32 ieee_exponent) {
    int32 e2;
    uint32 m2;
    bool even;
    bool accept_bounds;
    uint32 mv;
    uint32 mp;
    uint32 mm_shift;
    uint32 mm;
    uint32 vr;
    uint32 vp;
    uint32 vm;
    int32 e10;
    bool vm_is_trailing_zeros = false;
    bool vr_is_trailing_zeros = false;
    uint8 last_removed_digit = 0;
    uint32 q;
    int32 i;
    int32 j;
    int32 k;
    int32 l;
    int32 removed = 0;
    uint32 output;
    int32 exp;
    FloatingDecimal32 fd;

    if (ieee_exponent == 0) {
        // We subtract 2 so that the bounds computation has 2 additional bits.
        e2 = 1 - FLOAT_BIAS - FLOAT_MANTISSA_BITS - 2;
        m2 = ieee_mantissa;
    } else {
        e2 = (int32)ieee_exponent - FLOAT_BIAS - FLOAT_MANTISSA_BITS - 2;
        m2 = (1u << FLOAT_MANTISSA_BITS) | ieee_mantissa;
    }

    even = (m2 & 1) == 0;
    accept_bounds = even;

#if defined(RYU_DEBUG)
    fmt_printf("-> %u * 2^%d\n", m2, e2 + 2);
#endif

    // Step 2: Determine the interval of valid decimal representations.
    mv = 4*m2;
    mp = 4*m2 + 2;
    // Implicit bool -> int conversion. True is 1, false is 0.
    mm_shift = ieee_mantissa != 0 || ieee_exponent <= 1;
    mm = 4*m2 - 1 - mm_shift;

    // Step 3: Convert to a decimal power base using 64-bit arithmetic.
    if (e2 >= 0) {
        q = log10_pow2(e2);
        e10 = (int32)q;
        k = FLOAT_POW5_INV_BITCOUNT + pow5_bits((int32)q) - 1;
        i = -e2 + (int32)q + k;
        vr = mul_pow5_inv_div_pow2(mv, q, i);
        vp = mul_pow5_inv_div_pow2(mp, q, i);
        vm = mul_pow5_inv_div_pow2(mm, q, i);
#if defined(RYU_DEBUG)
        fmt_printf("%u * 2^%d / 10^%u\n", mv, e2, q);
        fmt_printf("V+=%u\nV =%u\nV-=%u\n", vp, vr, vm);
#endif
        if (q != 0 && (vp - 1) / 10 <= vm / 10) {
            // We need to know one removed digit even if we are not going to
            // loop below. We could use q = X - 1 above, except that would
            // require 33 bits for the result, and we've found that 32-bit
            // arithmetic is faster even on 64-bit machines.
            l = FLOAT_POW5_INV_BITCOUNT + pow5_bits((int32)(q - 1)) - 1;
            last_removed_digit =
                (uint8)(mul_pow5_inv_div_pow2(mv, q - 1,
                                              -e2 + (int32)q - 1 + l)
                        % 10);
        }
        if (q <= 9) {
            // The largest power of 5 that fits in 24 bits is 5^10, but
            // q <= 9 seems to be safe as well. Only one of mp, mv, and mm can
            // be a multiple of 5, if any.
            if (mv % 5 == 0) {
                vr_is_trailing_zeros = multiple_of_power_of5_32(mv, q);
            } else if (accept_bounds) {
                vm_is_trailing_zeros = multiple_of_power_of5_32(mm, q);
            } else {
                vp -= multiple_of_power_of5_32(mp, q);
            }
        }
    } else {
        q = log10_pow5(-e2);
        e10 = (int32)q + e2;
        i = -e2 - (int32)q;
        k = pow5_bits(i) - FLOAT_POW5_BITCOUNT;
        j = (int32)q - k;
        vr = mul_pow5_div_pow2(mv, (uint32)i, j);
        vp = mul_pow5_div_pow2(mp, (uint32)i, j);
        vm = mul_pow5_div_pow2(mm, (uint32)i, j);
#if defined(RYU_DEBUG)
        fmt_printf("%u * 5^%d / 10^%u\n", mv, -e2, q);
        fmt_printf("%u %d %d %d\n", q, i, k, j);
        fmt_printf("V+=%u\nV =%u\nV-=%u\n", vp, vr, vm);
#endif
        if (q != 0 && (vp - 1) / 10 <= vm / 10) {
            j = (int32)q - 1 - (pow5_bits(i + 1) - FLOAT_POW5_BITCOUNT);
            last_removed_digit =
                (uint8)(mul_pow5_div_pow2(mv, (uint32)(i + 1), j) % 10);
        }
        if (q <= 1) {
            // {vr,vp,vm} is trailing zeros if {mv,mp,mm} has at least q
            // trailing 0 bits. mv = 4*m2, so it always has at least two
            // trailing 0 bits.
            vr_is_trailing_zeros = true;
            if (accept_bounds) {
                // mm = mv - 1 - mm_shift, so it has 1 trailing 0 bit iff
                // mm_shift == 1.
                vm_is_trailing_zeros = mm_shift == 1;
            } else {
                // mp = mv + 2, so it always has at least one trailing 0 bit.
                vp -= 1;
            }
        } else if (q < 31) {
            // TODO(ulfjack): Use a tighter bound here.
            vr_is_trailing_zeros = multiple_of_power_of2_32(mv, q - 1);
#if defined(RYU_DEBUG)
            fmt_printf("vr is trailing zeros=%s\n",
                       vr_is_trailing_zeros ? "true" : "false");
#endif
        }
    }
#if defined(RYU_DEBUG)
    fmt_printf("e10=%d\n", e10);
    fmt_printf("V+=%u\nV =%u\nV-=%u\n", vp, vr, vm);
    fmt_printf("vm is trailing zeros=%s\n",
               vm_is_trailing_zeros ? "true" : "false");
    fmt_printf("vr is trailing zeros=%s\n",
               vr_is_trailing_zeros ? "true" : "false");
#endif

    // Step 4: Find the shortest decimal representation in the interval of
    // valid representations.
    if (vm_is_trailing_zeros || vr_is_trailing_zeros) {
        // General case, which happens rarely (~4.0%).
        while (vp / 10 > vm / 10) {
#if defined(__clang__)
            // https://bugs.llvm.org/show_bug.cgi?id=23106
            // The compiler does not realize that vm % 10 can be computed from
            // vm / 10 as vm - (vm / 10) * 10.
            vm_is_trailing_zeros &= vm - (vm / 10)*10 == 0;
#else
            vm_is_trailing_zeros &= vm % 10 == 0;
#endif
            vr_is_trailing_zeros &= last_removed_digit == 0;
            last_removed_digit = (uint8)(vr % 10);
            vr /= 10;
            vp /= 10;
            vm /= 10;
            removed += 1;
        }
#if defined(RYU_DEBUG)
        fmt_printf("V+=%u\nV =%u\nV-=%u\n", vp, vr, vm);
        fmt_printf("d-10=%s\n", vm_is_trailing_zeros ? "true" : "false");
#endif
        if (vm_is_trailing_zeros) {
            while (vm % 10 == 0) {
                vr_is_trailing_zeros &= last_removed_digit == 0;
                last_removed_digit = (uint8)(vr % 10);
                vr /= 10;
                vp /= 10;
                vm /= 10;
                removed += 1;
            }
        }
#if defined(RYU_DEBUG)
        fmt_printf("%u %d\n", vr, last_removed_digit);
        fmt_printf("vr is trailing zeros=%s\n",
                   vr_is_trailing_zeros ? "true" : "false");
#endif
        if (vr_is_trailing_zeros && last_removed_digit == 5 && vr % 2 == 0) {
            // Round even if the exact number is .....50..0.
            last_removed_digit = 4;
        }

        // We need to take vr + 1 if vr is outside bounds or we need to round
        // up.
        output = vr + ((vr == vm && (!accept_bounds || !vm_is_trailing_zeros))
                       || last_removed_digit >= 5);
    } else {
        // Specialized for the common case (~96.0%). Percentages below are
        // relative to this. Loop iterations below (approximately):
        // 0: 13.6%, 1: 70.7%, 2: 14.1%, 3: 1.39%, 4: 0.14%, 5+: 0.01%
        while (vp / 10 > vm / 10) {
            last_removed_digit = (uint8)(vr % 10);
            vr /= 10;
            vp /= 10;
            vm /= 10;
            removed += 1;
        }
#if defined(RYU_DEBUG)
        fmt_printf("%u %d\n", vr, last_removed_digit);
        fmt_printf("vr is trailing zeros=%s\n",
                   vr_is_trailing_zeros ? "true" : "false");
#endif
        // We need to take vr + 1 if vr is outside bounds or we need to round
        // up.
        output = vr + (vr == vm || last_removed_digit >= 5);
    }
    exp = e10 + removed;

#if defined(RYU_DEBUG)
    fmt_printf("V+=%u\nV =%u\nV-=%u\n", vp, vr, vm);
    fmt_printf("O=%u\n", output);
    fmt_printf("EXP=%d\n", exp);
#endif

    fd.exponent = exp;
    fd.mantissa = output;
    return fd;
}

static inline int32
f2s_to_chars(FloatingDecimal32 value, bool sign, char *result) {
    // Step 5: Print the decimal representation.
    int32 index = 0;
    uint32 output;
    int32 output_len;
    int32 i = 0;
    uint32 c;
    uint32 c0;
    uint32 c1;
    int32 exp;

    if (sign) {
        result[index] = '-';
        index += 1;
    }

    output = value.mantissa;
    output_len = (int32)decimal_length9(output);

#if defined(RYU_DEBUG)
    fmt_printf("DIGITS=%u\n", value.mantissa);
    fmt_printf("OLEN=%u\n", output_len);
    fmt_printf("EXP=%u\n", value.exponent + output_len);
#endif

    // Print the decimal digits.
    // The following code is equivalent to:
    // for (uint32 i = 0; i < output_len - 1; i += 1) {
    //     uint32 c = output % 10; output /= 10;
    //     result[index + output_len - i] = (char)('0' + c);
    // }
    // result[index] = '0' + output % 10;
    while (output >= 10000) {
#if defined(__clang__)
        // https://bugs.llvm.org/show_bug.cgi?id=38217
        c = output - 10000*(output / 10000);
#else
        c = output % 10000;
#endif
        output /= 10000;
        c0 = (c % 100) << 1;
        c1 = (c / 100) << 1;
        memcpy(result + index + output_len - i - 1, ryu_digit_table + c0, 2);
        memcpy(result + index + output_len - i - 3, ryu_digit_table + c1, 2);
        i += 4;
    }

    if (output >= 100) {
        c = (output % 100) << 1;
        output /= 100;
        memcpy(result + index + output_len - i - 1, ryu_digit_table + c, 2);
        i += 2;
    }

    if (output >= 10) {
        c = output << 1;
        // We can't use memcpy here: the decimal dot goes between these two
        // digits.
        result[index + output_len - i] = ryu_digit_table[c + 1];
        result[index] = ryu_digit_table[c];
    } else {
        result[index] = (char)('0' + output);
    }

    // Print decimal point if needed.
    if (output_len > 1) {
        result[index + 1] = '.';
        index += output_len + 1;
    } else {
        index += 1;
    }

    // Print the exponent.
    result[index] = 'E';
    index += 1;
    exp = value.exponent + output_len - 1;
    if (exp < 0) {
        result[index] = '-';
        index += 1;
        exp = -exp;
    }

    if (exp >= 10) {
        memcpy(result + index, ryu_digit_table + 2*exp, 2);
        index += 2;
    } else {
        result[index] = (char)('0' + exp);
        index += 1;
    }

    return index;
}

int
f2s_buffered_n(float f, char *result) {
    // Step 1: Decode the floating-point number, and unify normalized and
    // subnormal cases.
    uint32 bits;
    bool ieee_sign;
    uint32 ieee_mantissa;
    uint32 ieee_exponent;
    FloatingDecimal32 value;

    bits = float_to_bits(f);

#if defined(RYU_DEBUG)
    fmt_printf("IN=");
    for (int32 bit = 31; bit >= 0; bit -= 1) {
        fmt_printf("%u", (bits >> bit) & 1);
    }
    fmt_printf("\n");
#endif

    // Decode bits into sign, mantissa, and exponent.
    ieee_sign = ((bits >> (FLOAT_MANTISSA_BITS + FLOAT_EXPONENT_BITS))
                 & 1) != 0;
    ieee_mantissa = bits & ((1u << FLOAT_MANTISSA_BITS) - 1);
    ieee_exponent =
        (bits >> FLOAT_MANTISSA_BITS) & ((1u << FLOAT_EXPONENT_BITS) - 1);

    // Case distinction; exit early for the easy cases.
    if (ieee_exponent == ((1u << FLOAT_EXPONENT_BITS) - 1u)
        || (ieee_exponent == 0 && ieee_mantissa == 0)) {
        return copy_special_str(result, ieee_sign, ieee_exponent,
                                ieee_mantissa);
    }

    value = f2s_to_decimal(ieee_mantissa, ieee_exponent);
    return f2s_to_chars(value, ieee_sign, result);
}

void
f2s_buffered(float f, char *result) {
    int index;

    index = f2s_buffered_n(f, result);

    // Terminate the string.
    result[index] = '\0';
    return;
}

char *
f2s(float f) {
    char *result;

    result = (char *)malloc(16);
    f2s_buffered(f, result);
    return result;
}

#endif /* RYU_F2S_C */
