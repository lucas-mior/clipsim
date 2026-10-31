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

#if !defined(RYU_GENERIC_128_C)
#define RYU_GENERIC_128_C

#include "cbase.h"

#include "ryu/ryu_generic_128.h"
#include "ryu/generic_128.h"

#define GENERIC_ONE ((uint128)1)

#define RYU_FLOAT_MANTISSA_BITS 23
#define RYU_FLOAT_EXPONENT_BITS 8

#define RYU_DOUBLE_MANTISSA_BITS 52
#define RYU_DOUBLE_EXPONENT_BITS 11

#define RYU_LONG_DOUBLE_MANTISSA_BITS 64
#define RYU_LONG_DOUBLE_EXPONENT_BITS 15

#if defined(RYU_DEBUG)
static char *
ryu_s(uint128 value) {
    int32 len = (int32)generic_decimal_length(value);
    char *buffer = (char *)malloc((len + 1)*SIZEOF(char));

    for (int32 i = 0; i < len; i += 1) {
        uint32 c = (uint32)(value % 10);

        value /= 10;
        buffer[len - 1 - i] = (char)('0' + c);
    }
    buffer[len] = 0;
    return buffer;
}

static char *
generic_bool_string(bool value) {
    return value ? "true" : "false";
}
#endif

struct floating_decimal_128
float_to_fd128(float f) {
    uint32 bits = 0;

    memcpy(&bits, &f, SIZEOF(float));
    return generic_binary_to_decimal(bits, RYU_FLOAT_MANTISSA_BITS,
                                     RYU_FLOAT_EXPONENT_BITS, false);
}

struct floating_decimal_128
double_to_fd128(double d) {
    uint64 bits = 0;

    memcpy(&bits, &d, SIZEOF(double));
    return generic_binary_to_decimal(bits, RYU_DOUBLE_MANTISSA_BITS,
                                     RYU_DOUBLE_EXPONENT_BITS, false);
}

struct floating_decimal_128
long_double_to_fd128(long double d) {
    uint128 bits = 0;

    memcpy(&bits, &d, SIZEOF(long double));
#if defined(RYU_DEBUG)
    // For some odd reason, this ends up with noise in the top 48 bits. We can
    // clear out those bits with the following line; this is not required, the
    // conversion routine should ignore those bits, but the debug output can be
    // confusing if they aren't 0s.
    bits &= (GENERIC_ONE << 80) - 1;
#endif
    return generic_binary_to_decimal(bits, RYU_LONG_DOUBLE_MANTISSA_BITS,
                                     RYU_LONG_DOUBLE_EXPONENT_BITS, true);
}

static struct floating_decimal_128
generic_zero_decimal(bool ieee_sign) {
    struct floating_decimal_128 fd;

    fd.mantissa = 0;
    fd.exponent = 0;
    fd.sign = ieee_sign;
    return fd;
}

static struct floating_decimal_128
generic_exceptional_decimal(uint128 ieee_mantissa, uint32 mantissa_bits,
                            bool explicit_leading_bit, bool ieee_sign) {
    struct floating_decimal_128 fd;

    fd.mantissa = explicit_leading_bit
                  ? ieee_mantissa & ((GENERIC_ONE << (mantissa_bits - 1)) - 1)
                  : ieee_mantissa;
    fd.exponent = FD128_EXCEPTIONAL_EXPONENT;
    fd.sign = ieee_sign;
    return fd;
}

static void
generic_normalize_binary(uint128 ieee_mantissa, uint32 ieee_exponent,
                         uint32 mantissa_bits, uint32 bias,
                         bool explicit_leading_bit, int32 *e2,
                         uint128 *m2) {
    // We subtract 2 in all cases so that the bounds computation has 2
    // additional bits.
    if (explicit_leading_bit) {
        // mantissa_bits includes the explicit leading bit, so we need to
        // correct for that here.
        if (ieee_exponent == 0) {
            *e2 = 1 - (int32)bias - (int32)mantissa_bits + 1 - 2;
        } else {
            *e2 = (int32)ieee_exponent - (int32)bias
                  - (int32)mantissa_bits + 1 - 2;
        }
        *m2 = ieee_mantissa;
    } else {
        if (ieee_exponent == 0) {
            *e2 = 1 - (int32)bias - (int32)mantissa_bits - 2;
            *m2 = ieee_mantissa;
        } else {
            *e2 = (int32)ieee_exponent - (int32)bias
                  - (int32)mantissa_bits - 2;
            *m2 = (GENERIC_ONE << mantissa_bits) | ieee_mantissa;
        }
    }
    return;
}

static void
generic_scaled_values_positive(uint128 m2, int32 e2, uint32 mm_shift,
                               uint128 *vr, uint128 *vp, uint128 *vm,
                               int32 *e10) {
    uint32 q = generic_log10_pow2(e2) - (e2 > 3);
    int32 k = FLOAT_128_POW5_INV_BITCOUNT + generic_pow5_bits(q) - 1;
    int32 i = -e2 + (int32)q + k;
    uint64 pow5[4];

    *e10 = (int32)q;
    generic_compute_inv_pow5(q, pow5);
    *vr = generic_mul_shift(4*m2, pow5, i);
    *vp = generic_mul_shift(4*m2 + 2, pow5, i);
    *vm = generic_mul_shift(4*m2 - 1 - mm_shift, pow5, i);
#if defined(RYU_DEBUG)
    fmt_printf("%s * 2^%d / 10^%u\n", ryu_s(4*m2), e2, q);
    fmt_printf("V+=%s\nV =%s\nV-=%s\n", ryu_s(*vp), ryu_s(*vr), ryu_s(*vm));
#endif
    return;
}

static void
generic_update_trailing_positive(uint128 mv, uint32 q, uint32 mm_shift,
                                 bool accept_bounds, uint128 *vp,
                                 bool *vr_trailing_zeros,
                                 bool *vm_trailing_zeros) {
    // floor(log_5(2^128)) = 55, this is very conservative.
    if (q > 55) {
        return;
    }

    // Only one of mp, mv, and mm can be a multiple of 5, if any.
    if (mv % 5 == 0) {
        *vr_trailing_zeros = generic_multiple_of_power_of5(mv, q - 1);
    } else if (accept_bounds) {
        // Same as min(e2 + (~mm & 1), generic_pow5_factor(mm)) >= q
        // <=> e2 + (~mm & 1) >= q && generic_pow5_factor(mm) >= q
        // <=> true && generic_pow5_factor(mm) >= q, since e2 >= q.
        *vm_trailing_zeros =
            generic_multiple_of_power_of5(mv - 1 - mm_shift, q);
    } else {
        // Same as min(e2 + 1, generic_pow5_factor(mp)) >= q.
        *vp -= generic_multiple_of_power_of5(mv + 2, q);
    }
    return;
}

static void
generic_scaled_values_negative(uint128 m2, int32 e2, uint32 mm_shift,
                               uint128 *vr, uint128 *vp, uint128 *vm,
                               int32 *e10, uint32 *q_result) {
    uint32 q = generic_log10_pow5(-e2) - (-e2 > 1);
    int32 i = -e2 - (int32)q;
    int32 k = generic_pow5_bits(i) - FLOAT_128_POW5_BITCOUNT;
    int32 j = (int32)q - k;
    uint64 pow5[4];

    *q_result = q;
    *e10 = (int32)q + e2;
    generic_compute_pow5(i, pow5);
    *vr = generic_mul_shift(4*m2, pow5, j);
    *vp = generic_mul_shift(4*m2 + 2, pow5, j);
    *vm = generic_mul_shift(4*m2 - 1 - mm_shift, pow5, j);
#if defined(RYU_DEBUG)
    fmt_printf("%s * 5^%d / 10^%u\n", ryu_s(4*m2), -e2, q);
    fmt_printf("%u %d %d %d\n", q, i, k, j);
    fmt_printf("V+=%s\nV =%s\nV-=%s\n", ryu_s(*vp), ryu_s(*vr), ryu_s(*vm));
#endif
    return;
}

static void
generic_update_trailing_negative(uint128 mv, uint32 q, uint32 mm_shift,
                                 bool accept_bounds, uint128 *vp,
                                 bool *vr_trailing_zeros,
                                 bool *vm_trailing_zeros) {
    if (q <= 1) {
        // {vr,vp,vm} is trailing zeros if {mv,mp,mm} has at least q trailing
        // 0 bits. mv = 4 m2, so it always has at least two trailing 0 bits.
        *vr_trailing_zeros = true;
        if (accept_bounds) {
            // mm = mv - 1 - mm_shift, so it has 1 trailing 0 bit iff
            // mm_shift == 1.
            *vm_trailing_zeros = mm_shift == 1;
        } else {
            // mp = mv + 2, so it always has at least one trailing 0 bit.
            *vp -= 1;
        }
    } else if (q < 127) { // TODO(ulfjack): Use a tighter bound here.
        // We need to compute:
        // min(ntz(mv), generic_pow5_factor(mv) - e2) >= q - 1
        // <=> ntz(mv) >= q - 1
        //     && generic_pow5_factor(mv) - e2 >= q - 1
        // <=> ntz(mv) >= q - 1    (e2 is negative and -e2 >= q)
        // <=> (mv & ((1 << (q - 1)) - 1)) == 0
        // We also need to make sure that the left shift does not overflow.
        *vr_trailing_zeros = generic_multiple_of_power_of2(mv, q - 1);
#if defined(RYU_DEBUG)
        fmt_printf("vr is trailing zeros=%s\n",
                   generic_bool_string(*vr_trailing_zeros));
#endif
    }
    return;
}

static void
generic_remove_digits(uint128 *vr, uint128 *vp, uint128 *vm,
                      bool *vr_trailing_zeros, bool *vm_trailing_zeros,
                      uint32 *removed, uint8 *last_removed_digit) {
    while (*vp / 10 > *vm / 10) {
        *vm_trailing_zeros &= *vm % 10 == 0;
        *vr_trailing_zeros &= *last_removed_digit == 0;
        *last_removed_digit = (uint8)(*vr % 10);
        *vr /= 10;
        *vp /= 10;
        *vm /= 10;
        *removed += 1;
    }
    return;
}

static void
generic_remove_vm_trailing_digits(uint128 *vr, uint128 *vp, uint128 *vm,
                                  bool *vr_trailing_zeros, uint32 *removed,
                                  uint8 *last_removed_digit) {
    while (*vm % 10 == 0) {
        *vr_trailing_zeros &= *last_removed_digit == 0;
        *last_removed_digit = (uint8)(*vr % 10);
        *vr /= 10;
        *vp /= 10;
        *vm /= 10;
        *removed += 1;
    }
    return;
}

struct floating_decimal_128
generic_binary_to_decimal(__uint128_t bits, uint32 mantissa_bits,
                          uint32 exponent_bits, bool explicit_leading_bit) {
    struct floating_decimal_128 fd;
    uint32 bias;
    uint32 ieee_exponent;
    uint32 mm_shift;
    uint32 removed = 0;
    uint8 last_removed_digit = 0;
    int32 e2;
    int32 e10;
    int32 exp;
    uint128 ieee_mantissa;
    uint128 m2;
    uint128 mv;
    uint128 vr;
    uint128 vp;
    uint128 vm;
    uint128 output;
    bool ieee_sign;
    bool accept_bounds;
    bool vm_trailing_zeros = false;
    bool vr_trailing_zeros = false;

#if defined(RYU_DEBUG)
    fmt_printf("IN=");
    for (int32 bit = 127; bit >= 0; bit -= 1) {
        fmt_printf("%u", (uint32)((bits >> bit) & 1));
    }
    fmt_printf("\n");
#endif

    bias = (1u << (exponent_bits - 1)) - 1;
    ieee_sign = ((bits >> (mantissa_bits + exponent_bits)) & 1) != 0;
    ieee_mantissa = bits & ((GENERIC_ONE << mantissa_bits) - 1);
    ieee_exponent = (uint32)(
        (bits >> mantissa_bits) & ((GENERIC_ONE << exponent_bits) - 1));

    if (ieee_exponent == 0 && ieee_mantissa == 0) {
        return generic_zero_decimal(ieee_sign);
    }
    if (ieee_exponent == ((1u << exponent_bits) - 1u)) {
        return generic_exceptional_decimal(ieee_mantissa, mantissa_bits,
                                           explicit_leading_bit, ieee_sign);
    }

    generic_normalize_binary(ieee_mantissa, ieee_exponent, mantissa_bits, bias,
                             explicit_leading_bit, &e2, &m2);
    accept_bounds = (m2 & 1) == 0;

#if defined(RYU_DEBUG)
    fmt_printf("-> %s %s * 2^%d\n", ieee_sign ? "-" : "+", ryu_s(m2), e2 + 2);
#endif

    // Step 2: Determine the interval of legal decimal representations.
    mv = 4*m2;

    // Implicit bool -> int conversion. True is 1, false is 0.
    mm_shift = (ieee_mantissa != (explicit_leading_bit
                                   ? GENERIC_ONE << (mantissa_bits - 1)
                                   : 0))
               || (ieee_exponent == 0);

    // Step 3: Convert to a decimal power base using 128-bit arithmetic.
    if (e2 >= 0) {
        uint32 q = generic_log10_pow2(e2) - (e2 > 3);

        generic_scaled_values_positive(m2, e2, mm_shift, &vr, &vp, &vm, &e10);
        generic_update_trailing_positive(mv, q, mm_shift, accept_bounds, &vp,
                                         &vr_trailing_zeros,
                                         &vm_trailing_zeros);
    } else {
        uint32 q;

        generic_scaled_values_negative(m2, e2, mm_shift, &vr, &vp, &vm, &e10,
                                       &q);
        generic_update_trailing_negative(mv, q, mm_shift, accept_bounds, &vp,
                                         &vr_trailing_zeros,
                                         &vm_trailing_zeros);
    }
#if defined(RYU_DEBUG)
    fmt_printf("e10=%d\n", e10);
    fmt_printf("V+=%s\nV =%s\nV-=%s\n", ryu_s(vp), ryu_s(vr), ryu_s(vm));
    fmt_printf("vm is trailing zeros=%s\n",
               generic_bool_string(vm_trailing_zeros));
    fmt_printf("vr is trailing zeros=%s\n",
               generic_bool_string(vr_trailing_zeros));
#endif

    // Step 4: Find the shortest decimal representation in the interval of
    // legal representations.
    generic_remove_digits(&vr, &vp, &vm, &vr_trailing_zeros,
                          &vm_trailing_zeros, &removed,
                          &last_removed_digit);
#if defined(RYU_DEBUG)
    fmt_printf("V+=%s\nV =%s\nV-=%s\n", ryu_s(vp), ryu_s(vr), ryu_s(vm));
    fmt_printf("d-10=%s\n", generic_bool_string(vm_trailing_zeros));
#endif
    if (vm_trailing_zeros) {
        generic_remove_vm_trailing_digits(&vr, &vp, &vm, &vr_trailing_zeros,
                                          &removed, &last_removed_digit);
    }
#if defined(RYU_DEBUG)
    fmt_printf("%s %u\n", ryu_s(vr), last_removed_digit);
    fmt_printf("vr is trailing zeros=%s\n",
               generic_bool_string(vr_trailing_zeros));
#endif
    if (vr_trailing_zeros && (last_removed_digit == 5) && (vr % 2 == 0)) {
        // Round even if the exact numbers is .....50..0.
        last_removed_digit = 4;
    }

    // We need to take vr + 1 if vr is outside bounds or we need to round up.
    output = vr + ((vr == vm && (!accept_bounds || !vm_trailing_zeros))
                   || (last_removed_digit >= 5));
    exp = e10 + (int32)removed;

#if defined(RYU_DEBUG)
    fmt_printf("V+=%s\nV =%s\nV-=%s\n", ryu_s(vp), ryu_s(vr), ryu_s(vm));
    fmt_printf("O=%s\n", ryu_s(output));
    fmt_printf("EXP=%d\n", exp);
#endif

    fd.mantissa = output;
    fd.exponent = exp;
    fd.sign = ieee_sign;
    return fd;
}

static inline int32
generic_copy_special_str(char *result, struct floating_decimal_128 fd) {
    if (fd.mantissa) {
        memcpy(result, "NaN", 3);
        return 3;
    }
    if (fd.sign) {
        result[0] = '-';
    }
    memcpy(result + fd.sign, "Infinity", 8);
    return fd.sign + 8;
}

int
generic_to_chars(struct floating_decimal_128 value, char *result) {
    int32 index = 0;
    int32 exp;
    uint32 olength;
    uint32 elength;
    uint128 output;

    if (value.exponent == FD128_EXCEPTIONAL_EXPONENT) {
        return generic_copy_special_str(result, value);
    }

    // Step 5: Print the decimal representation.
    if (value.sign) {
        result[index] = '-';
        index += 1;
    }

    output = value.mantissa;
    olength = generic_decimal_length(output);

#if defined(RYU_DEBUG)
    fmt_printf("DIGITS=%s\n", ryu_s(value.mantissa));
    fmt_printf("OLEN=%u\n", olength);
    fmt_printf("EXP=%d\n", value.exponent + (int32)olength);
#endif

    for (uint32 i = 0; i < olength - 1; i += 1) {
        uint32 c = (uint32)(output % 10);

        output /= 10;
        result[index + olength - i] = (char)('0' + c);
    }
    // output should be < 10 by now.
    result[index] = (char)('0' + (uint32)(output % 10));

    // Print decimal point if needed.
    if (olength > 1) {
        result[index + 1] = '.';
        index += (int32)olength + 1;
    } else {
        index += 1;
    }

    // Print the exponent.
    result[index] = 'E';
    index += 1;
    exp = value.exponent + (int32)olength - 1;

    if (exp < 0) {
        result[index] = '-';
        index += 1;
        exp = -exp;
    }

    elength = generic_decimal_length((uint32)exp);

    for (uint32 i = 0; i < elength; i += 1) {
        uint32 c = (uint32)exp % 10;

        exp /= 10;
        result[index + elength - 1 - i] = (char)('0' + c);
    }
    index += (int32)elength;
    return index;
}

#endif /* RYU_GENERIC_128_C */
