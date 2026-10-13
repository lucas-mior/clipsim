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
// -DRYU_OPTIMIZE_SIZE Use smaller lookup tables. Instead of storing every
//     required power of 5, only store every 26th entry, and compute
//     intermediate values with a multiplication. This reduces the lookup table
//     size by about 10x (only one case, and only double) at the cost of some
//     performance. Currently requires MSVC intrinsics.

#if !defined(RYU_D2S_C)
#define RYU_D2S_C

#include "cbase.h"
#include "ryu/ryu.h"

#if defined(RYU_DEBUG)
#include <inttypes.h>
#endif

#include "ryu/common.h"
#include "ryu/digit_table.h"
#include "ryu/d2s_intrinsics.h"

// Include either the small or the full lookup tables depending on the mode.
#if defined(RYU_OPTIMIZE_SIZE)
#include "ryu/d2s_small_table.h"
#else
#include "ryu/d2s_full_table.h"
#endif

#define DOUBLE_MANTISSA_BITS 52
#define DOUBLE_EXPONENT_BITS 11
#define DOUBLE_BIAS 1023

// A floating decimal representing m * 10^e.
typedef struct FloatingDecimal64 {
    uint64 mantissa;

    // Decimal exponent's range is -324 to 308 inclusive, and can fit in a
    // short if needed.
    int32 exponent;
} FloatingDecimal64;

static inline uint32
decimal_length17(uint64 value) {
    // This is slightly faster than a loop.
    // The average output length is 16.38 digits, so we check high-to-low.
    // Function precondition: value is not an 18, 19, or 20-digit number.
    // 17 digits are sufficient for round-tripping.
    assert(value < 100000000000000000L);

    if (value >= 10000000000000000L) {
        return 17;
    }
    if (value >= 1000000000000000L) {
        return 16;
    }
    if (value >= 100000000000000L) {
        return 15;
    }
    if (value >= 10000000000000L) {
        return 14;
    }
    if (value >= 1000000000000L) {
        return 13;
    }
    if (value >= 100000000000L) {
        return 12;
    }
    if (value >= 10000000000L) {
        return 11;
    }
    if (value >= 1000000000L) {
        return 10;
    }
    if (value >= 100000000L) {
        return 9;
    }
    if (value >= 10000000L) {
        return 8;
    }
    if (value >= 1000000L) {
        return 7;
    }
    if (value >= 100000L) {
        return 6;
    }
    if (value >= 10000L) {
        return 5;
    }
    if (value >= 1000L) {
        return 4;
    }
    if (value >= 100L) {
        return 3;
    }
    if (value >= 10L) {
        return 2;
    }

    return 1;
}

static inline FloatingDecimal64
d2s_to_decimal(uint64 ieee_mantissa, uint32 ieee_exponent) {
    int32 e2;
    uint64 m2;
    bool even;
    bool accept_bounds;
    uint64 mv;
    uint32 mm_shift;
    uint64 vr;
    uint64 vp;
    uint64 vm;
    int32 e10;
    bool vm_is_trailing_zeros = false;
    bool vr_is_trailing_zeros = false;
    uint32 q;
    int32 i;
    int32 j;
    int32 k;
    uint32 mv_mod5;
    int32 removed = 0;
    uint8 last_removed_digit = 0;
    uint64 output;
    uint64 vp_div10;
    uint64 vm_div10;
    uint32 vm_mod10;
    uint64 vr_div10;
    uint32 vr_mod10;
    bool round_up;
    uint64 vp_div100;
    uint64 vm_div100;
    uint64 vr_div100;
    uint32 vr_mod100;
    int32 exp;
    FloatingDecimal64 fd;
#if defined(RYU_OPTIMIZE_SIZE)
    uint64 pow5[2];
#endif

    if (ieee_exponent == 0) {
        // We subtract 2 so that the bounds computation has 2 additional bits.
        e2 = 1 - DOUBLE_BIAS - DOUBLE_MANTISSA_BITS - 2;
        m2 = ieee_mantissa;
    } else {
        e2 = (int32)ieee_exponent - DOUBLE_BIAS - DOUBLE_MANTISSA_BITS - 2;
        m2 = (1ull << DOUBLE_MANTISSA_BITS) | ieee_mantissa;
    }

    even = (m2 & 1) == 0;
    accept_bounds = even;

#if defined(RYU_DEBUG)
    printf("-> %" PRIu64 " * 2^%d\n", m2, e2 + 2);
#endif

    // Step 2: Determine the interval of valid decimal representations.
    mv = 4*m2;
    // Implicit bool -> int conversion. True is 1, false is 0.
    mm_shift = ieee_mantissa != 0 || ieee_exponent <= 1;
    // We would compute mp and mm like this:
    // uint64 mp = 4*m2 + 2;
    // uint64 mm = mv - 1 - mm_shift;

    // Step 3: Convert to a decimal power base using 128-bit arithmetic.
    if (e2 >= 0) {
        // I tried special-casing q == 0, but there was no effect on
        // performance. This expression is slightly faster than
        // max(0, log10_pow2(e2) - 1).
        q = log10_pow2(e2) - (uint32)(e2 > 3);
        e10 = (int32)q;
        k = DOUBLE_POW5_INV_BITCOUNT + pow5_bits((int32)q) - 1;
        i = -e2 + (int32)q + k;
#if defined(RYU_OPTIMIZE_SIZE)
        double_compute_inv_pow5(q, pow5);
        vr = mul_shift_all64(m2, pow5, i, &vp, &vm, mm_shift);
#else
        vr = mul_shift_all64(m2, double_pow5_inv_split[q], i, &vp, &vm,
                             mm_shift);
#endif
#if defined(RYU_DEBUG)
        printf("%" PRIu64 " * 2^%d / 10^%u\n", mv, e2, q);
        printf("V+=%" PRIu64 "\nV =%" PRIu64 "\nV-=%" PRIu64 "\n", vp, vr, vm);
#endif
        if (q <= 21) {
            // This should use q <= 22, but I think 21 is also safe. Smaller
            // values may still be safe, but it's more difficult to reason
            // about them. Only one of mp, mv, and mm can be a multiple of 5,
            // if any.
            mv_mod5 = ((uint32)mv) - 5*((uint32)div5(mv));
            if (mv_mod5 == 0) {
                vr_is_trailing_zeros = multiple_of_power_of5(mv, q);
            } else if (accept_bounds) {
                // Same as min(e2 + (~mm & 1), pow5_factor(mm)) >= q
                // <=> e2 + (~mm & 1) >= q && pow5_factor(mm) >= q
                // <=> true && pow5_factor(mm) >= q, since e2 >= q.
                vm_is_trailing_zeros =
                    multiple_of_power_of5(mv - 1 - mm_shift, q);
            } else {
                // Same as min(e2 + 1, pow5_factor(mp)) >= q.
                vp -= multiple_of_power_of5(mv + 2, q);
            }
        }
    } else {
        // This expression is slightly faster than max(0, log10_pow5(-e2) - 1).
        q = log10_pow5(-e2) - (uint32)(-e2 > 1);
        e10 = (int32)q + e2;
        i = -e2 - (int32)q;
        k = pow5_bits(i) - DOUBLE_POW5_BITCOUNT;
        j = (int32)q - k;
#if defined(RYU_OPTIMIZE_SIZE)
        double_compute_pow5((uint32)i, pow5);
        vr = mul_shift_all64(m2, pow5, j, &vp, &vm, mm_shift);
#else
        vr = mul_shift_all64(m2, double_pow5_split[i], j, &vp, &vm, mm_shift);
#endif
#if defined(RYU_DEBUG)
        printf("%" PRIu64 " * 5^%d / 10^%u\n", mv, -e2, q);
        printf("%u %d %d %d\n", q, i, k, j);
        printf("V+=%" PRIu64 "\nV =%" PRIu64 "\nV-=%" PRIu64 "\n", vp, vr, vm);
#endif
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
        } else if (q < 63) {
            // TODO(ulfjack): Use a tighter bound here.
            // We want to know if the full product has at least q trailing
            // zeros. We need to compute min(p2(mv), p5(mv) - e2) >= q
            // <=> p2(mv) >= q && p5(mv) - e2 >= q
            // <=> p2(mv) >= q (because -e2 >= q)
            vr_is_trailing_zeros = multiple_of_power_of2(mv, q);
#if defined(RYU_DEBUG)
            printf("vr is trailing zeros=%s\n",
                   vr_is_trailing_zeros ? "true" : "false");
#endif
        }
    }
#if defined(RYU_DEBUG)
    printf("e10=%d\n", e10);
    printf("V+=%" PRIu64 "\nV =%" PRIu64 "\nV-=%" PRIu64 "\n", vp, vr, vm);
    printf("vm is trailing zeros=%s\n",
           vm_is_trailing_zeros ? "true" : "false");
    printf("vr is trailing zeros=%s\n",
           vr_is_trailing_zeros ? "true" : "false");
#endif

    // Step 4: Find the shortest decimal representation in the interval of
    // valid representations. On average, we remove ~2 digits.
    if (vm_is_trailing_zeros || vr_is_trailing_zeros) {
        // General case, which happens rarely (~0.7%).
        for (;;) {
            vp_div10 = div10(vp);
            vm_div10 = div10(vm);
            if (vp_div10 <= vm_div10) {
                break;
            }
            vm_mod10 = ((uint32)vm) - 10*((uint32)vm_div10);
            vr_div10 = div10(vr);
            vr_mod10 = ((uint32)vr) - 10*((uint32)vr_div10);
            vm_is_trailing_zeros &= vm_mod10 == 0;
            vr_is_trailing_zeros &= last_removed_digit == 0;
            last_removed_digit = (uint8)vr_mod10;
            vr = vr_div10;
            vp = vp_div10;
            vm = vm_div10;
            removed += 1;
        }
#if defined(RYU_DEBUG)
        printf("V+=%" PRIu64 "\nV =%" PRIu64 "\nV-=%" PRIu64 "\n", vp, vr, vm);
        printf("d-10=%s\n", vm_is_trailing_zeros ? "true" : "false");
#endif
        if (vm_is_trailing_zeros) {
            for (;;) {
                vm_div10 = div10(vm);
                vm_mod10 = ((uint32)vm) - 10*((uint32)vm_div10);
                if (vm_mod10 != 0) {
                    break;
                }
                vp_div10 = div10(vp);
                vr_div10 = div10(vr);
                vr_mod10 = ((uint32)vr) - 10*((uint32)vr_div10);
                vr_is_trailing_zeros &= last_removed_digit == 0;
                last_removed_digit = (uint8)vr_mod10;
                vr = vr_div10;
                vp = vp_div10;
                vm = vm_div10;
                removed += 1;
            }
        }
#if defined(RYU_DEBUG)
        printf("%" PRIu64 " %d\n", vr, last_removed_digit);
        printf("vr is trailing zeros=%s\n",
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
        // Specialized for the common case (~99.3%). Percentages below are
        // relative to this.
        round_up = false;
        vp_div100 = div100(vp);
        vm_div100 = div100(vm);
        if (vp_div100 > vm_div100) {
            // Optimization: remove two digits at a time (~86.2%).
            vr_div100 = div100(vr);
            vr_mod100 = ((uint32)vr) - 100*((uint32)vr_div100);
            round_up = vr_mod100 >= 50;
            vr = vr_div100;
            vp = vp_div100;
            vm = vm_div100;
            removed += 2;
        }
        // Loop iterations below (approximately), without optimization above:
        // 0: 0.03%, 1: 13.8%, 2: 70.6%, 3: 14.0%, 4: 1.40%, 5: 0.14%,
        // 6+: 0.02%
        // Loop iterations below (approximately), with optimization above:
        // 0: 70.6%, 1: 27.8%, 2: 1.40%, 3: 0.14%, 4+: 0.02%
        for (;;) {
            vp_div10 = div10(vp);
            vm_div10 = div10(vm);
            if (vp_div10 <= vm_div10) {
                break;
            }
            vr_div10 = div10(vr);
            vr_mod10 = ((uint32)vr) - 10*((uint32)vr_div10);
            round_up = vr_mod10 >= 5;
            vr = vr_div10;
            vp = vp_div10;
            vm = vm_div10;
            removed += 1;
        }
#if defined(RYU_DEBUG)
        printf("%" PRIu64 " roundUp=%s\n", vr, round_up ? "true" : "false");
        printf("vr is trailing zeros=%s\n",
               vr_is_trailing_zeros ? "true" : "false");
#endif
        // We need to take vr + 1 if vr is outside bounds or we need to round
        // up.
        output = vr + (vr == vm || round_up);
    }
    exp = e10 + removed;

#if defined(RYU_DEBUG)
    printf("V+=%" PRIu64 "\nV =%" PRIu64 "\nV-=%" PRIu64 "\n", vp, vr, vm);
    printf("O=%" PRIu64 "\n", output);
    printf("EXP=%d\n", exp);
#endif

    fd.exponent = exp;
    fd.mantissa = output;
    return fd;
}

static inline int32
d2s_to_chars(FloatingDecimal64 value, bool sign, char *result) {
    // Step 5: Print the decimal representation.
    int32 index = 0;
    uint64 output;
    int32 output_len;
    int32 i = 0;
    uint64 q;
    uint32 output2;
    uint32 c;
    uint32 d;
    uint32 c0;
    uint32 c1;
    uint32 d0;
    uint32 d1;
    int32 exp;
    int32 digit;

    if (sign) {
        result[index] = '-';
        index += 1;
    }

    output = value.mantissa;
    output_len = (int32)decimal_length17(output);

#if defined(RYU_DEBUG)
    printf("DIGITS=%" PRIu64 "\n", value.mantissa);
    printf("OLEN=%u\n", output_len);
    printf("EXP=%u\n", value.exponent + output_len);
#endif

    // Print the decimal digits.
    // The following code is equivalent to:
    // for (uint32 i = 0; i < output_len - 1; i += 1) {
    //     uint32 c = output % 10; output /= 10;
    //     result[index + output_len - i] = (char)('0' + c);
    // }
    // result[index] = '0' + output % 10;

    // We prefer 32-bit operations, even on 64-bit platforms.
    // We have at most 17 digits, and uint32 can store 9 digits.
    // If output doesn't fit into uint32, we cut off 8 digits, so the rest will
    // fit into uint32.
    if ((output >> 32) != 0) {
        // Expensive 64-bit division.
        q = div1e8(output);
        output2 = ((uint32)output) - 100000000*((uint32)q);
        output = q;

        c = output2 % 10000;
        output2 /= 10000;
        d = output2 % 10000;
        c0 = (c % 100) << 1;
        c1 = (c / 100) << 1;
        d0 = (d % 100) << 1;
        d1 = (d / 100) << 1;
        memcpy(result + index + output_len - 1, ryu_digit_table + c0, 2);
        memcpy(result + index + output_len - 3, ryu_digit_table + c1, 2);
        memcpy(result + index + output_len - 5, ryu_digit_table + d0, 2);
        memcpy(result + index + output_len - 7, ryu_digit_table + d1, 2);
        i += 8;
    }

    output2 = (uint32)output;
    while (output2 >= 10000) {
#if defined(__clang__)
        // https://bugs.llvm.org/show_bug.cgi?id=38217
        c = output2 - 10000*(output2 / 10000);
#else
        c = output2 % 10000;
#endif
        output2 /= 10000;
        c0 = (c % 100) << 1;
        c1 = (c / 100) << 1;
        memcpy(result + index + output_len - i - 1, ryu_digit_table + c0, 2);
        memcpy(result + index + output_len - i - 3, ryu_digit_table + c1, 2);
        i += 4;
    }

    if (output2 >= 100) {
        c = (output2 % 100) << 1;
        output2 /= 100;
        memcpy(result + index + output_len - i - 1, ryu_digit_table + c, 2);
        i += 2;
    }

    if (output2 >= 10) {
        c = output2 << 1;
        // We can't use memcpy here: the decimal dot goes between these two
        // digits.
        result[index + output_len - i] = ryu_digit_table[c + 1];
        result[index] = ryu_digit_table[c];
    } else {
        result[index] = (char)('0' + output2);
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

    if (exp >= 100) {
        digit = exp % 10;
        memcpy(result + index, ryu_digit_table + 2*(exp / 10), 2);
        result[index + 2] = (char)('0' + digit);
        index += 3;
    } else if (exp >= 10) {
        memcpy(result + index, ryu_digit_table + 2*exp, 2);
        index += 2;
    } else {
        result[index] = (char)('0' + exp);
        index += 1;
    }

    return index;
}

static inline bool
d2s_small_int(uint64 ieee_mantissa, uint32 ieee_exponent,
              FloatingDecimal64 *value) {
    uint64 m2 = (1ull << DOUBLE_MANTISSA_BITS) | ieee_mantissa;
    int32 e2 = (int32)ieee_exponent - DOUBLE_BIAS - DOUBLE_MANTISSA_BITS;
    uint64 mask;
    uint64 fraction;

    if (e2 > 0) {
        // f = m2*2^e2 >= 2^53 is an integer. Ignore this case for now.
        return false;
    }

    if (e2 < -52) {
        // f < 1.
        return false;
    }

    // Since 2^52 <= m2 < 2^53 and 0 <= -e2 <= 52:
    // 1 <= f = m2/2^-e2 < 2^53. Test if the lower -e2 bits of the
    // significand are 0, i.e. whether the fraction is 0.
    mask = (1ull << -e2) - 1;
    fraction = m2 & mask;
    if (fraction != 0) {
        return false;
    }

    // f is an integer in the range [1, 2^53).
    // Note: mantissa might contain trailing (decimal) 0's.
    // Note: since 2^53 < 10^16, there is no need to adjust decimal_length17().
    value->mantissa = m2 >> -e2;
    value->exponent = 0;
    return true;
}

int
d2s_buffered_n(double f, char *result) {
    // Step 1: Decode the floating-point number, and unify normalized and
    // subnormal cases.
    uint64 bits;
    bool ieee_sign;
    uint64 ieee_mantissa;
    uint32 ieee_exponent;
    FloatingDecimal64 value;
    bool is_small_int;
    uint64 q;
    uint32 r;

    bits = double_to_bits(f);

#if defined(RYU_DEBUG)
    printf("IN=");
    for (int32 bit = 63; bit >= 0; bit -= 1) {
        printf("%d", (int)((bits >> bit) & 1));
    }
    printf("\n");
#endif

    // Decode bits into sign, mantissa, and exponent.
    ieee_sign = ((bits >> (DOUBLE_MANTISSA_BITS + DOUBLE_EXPONENT_BITS))
                 & 1) != 0;
    ieee_mantissa = bits & ((1ull << DOUBLE_MANTISSA_BITS) - 1);
    ieee_exponent =
        (uint32)((bits >> DOUBLE_MANTISSA_BITS)
                 & ((1u << DOUBLE_EXPONENT_BITS) - 1));

    // Case distinction; exit early for the easy cases.
    if (ieee_exponent == ((1u << DOUBLE_EXPONENT_BITS) - 1u)
        || (ieee_exponent == 0 && ieee_mantissa == 0)) {
        return copy_special_str(result, ieee_sign, ieee_exponent,
                                ieee_mantissa);
    }

    is_small_int = d2s_small_int(ieee_mantissa, ieee_exponent, &value);
    if (is_small_int) {
        // For small integers in the range [1, 2^53), value.mantissa might
        // contain trailing decimal zeros. For scientific notation we need to
        // move these zeros into the exponent. This is not needed for
        // fixed-point notation, so it might be beneficial to trim trailing
        // zeros in d2s_to_chars only if needed once fixed-point notation output
        // is implemented.
        for (;;) {
            q = div10(value.mantissa);
            r = ((uint32)value.mantissa) - 10*((uint32)q);
            if (r != 0) {
                break;
            }
            value.mantissa = q;
            value.exponent += 1;
        }
    } else {
        value = d2s_to_decimal(ieee_mantissa, ieee_exponent);
    }

    return d2s_to_chars(value, ieee_sign, result);
}

void
d2s_buffered(double f, char *result) {
    int index;

    index = d2s_buffered_n(f, result);

    // Terminate the string.
    result[index] = '\0';
    return;
}

char *
d2s(double f) {
    char *result;

    result = (char *)malloc(RYU_D2S_BUFFER_SIZE);
    d2s_buffered(f, result);
    return result;
}

#endif /* RYU_D2S_C */
