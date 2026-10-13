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
#if !defined(RYU_F2S_INTRINSICS_H)
#define RYU_F2S_INTRINSICS_H

#include "cbase.h"

// Defines RYU_32_BIT_PLATFORM if applicable.
#include "ryu/common.h"

#if defined(RYU_FLOAT_FULL_TABLE)

#include "ryu/f2s_full_table.h"

#else

#if defined(RYU_OPTIMIZE_SIZE)
#include "ryu/d2s_small_table.h"
#else
#include "ryu/d2s_full_table.h"
#endif
#define FLOAT_POW5_INV_BITCOUNT (DOUBLE_POW5_INV_BITCOUNT - 64)
#define FLOAT_POW5_BITCOUNT (DOUBLE_POW5_BITCOUNT - 64)

#endif /* defined(RYU_FLOAT_FULL_TABLE) */

static inline uint32
pow5_factor_32(uint32 value) {
    uint32 count = 0;

    for (;;) {
        uint32 q;
        uint32 r;

        assert(value != 0);
        q = value/5;
        r = value%5;
        if (r != 0) {
            break;
        }
        value = q;
        count += 1;
    }
    return count;
}

// Returns true if value is divisible by 5^p.
static inline bool
multiple_of_power_of5_32(uint32 value, uint32 p) {
    return pow5_factor_32(value) >= p;
}

// Returns true if value is divisible by 2^p.
static inline bool
multiple_of_power_of2_32(uint32 value, uint32 p) {
    // __builtin_ctz doesn't appear to be faster here.
    return (value & ((1u << p) - 1)) == 0;
}

// It seems to be slightly faster to avoid uint128 here, although the generated
// code for uint128 looks slightly nicer.
static inline uint32
mul_shift32(uint32 m, uint64 factor, int32 shift) {
    // The casts here help MSVC to avoid calls to the __allmul library function.
    uint32 factor_lo = (uint32)factor;
    uint32 factor_hi = (uint32)(factor >> 32);
    uint64 bits0 = (uint64)m*factor_lo;
    uint64 bits1 = (uint64)m*factor_hi;

#if defined(RYU_32_BIT_PLATFORM)
    uint32 bits0_hi;
    uint32 bits1_lo;
    uint32 bits1_hi;
    int32 s;
#else
    uint64 sum;
    uint64 shifted_sum;
#endif

    assert(shift > 32);

#if defined(RYU_32_BIT_PLATFORM)
    // On 32-bit platforms we can avoid a 64-bit shift-right since we only need
    // the upper 32 bits of the result and the shift value is > 32.
    bits0_hi = (uint32)(bits0 >> 32);
    bits1_lo = (uint32)bits1;
    bits1_hi = (uint32)(bits1 >> 32);
    bits1_lo += bits0_hi;
    bits1_hi += bits1_lo < bits0_hi;
    if (shift >= 64) {
        // s2f can call this with a shift value >= 64, which we have to handle.
        // This could now be slower than the !defined(RYU_32_BIT_PLATFORM)
        // case.
        return (uint32)(bits1_hi >> (shift - 64));
    }

    s = shift - 32;
    return (bits1_hi << (32 - s)) | (bits1_lo >> s);
#else
    sum = (bits0 >> 32) + bits1;
    shifted_sum = sum >> (shift - 32);
    assert(shifted_sum <= UINT32_MAX);
    return (uint32)shifted_sum;
#endif /* defined(RYU_32_BIT_PLATFORM) */
}

static inline uint32
mul_pow5_inv_div_pow2(uint32 m, uint32 q, int32 j) {
#if defined(RYU_FLOAT_FULL_TABLE)
    return mul_shift32(m, float_pow5_inv_split[q], j);
#elif defined(RYU_OPTIMIZE_SIZE)
    // The inverse multipliers are defined as [2^x / 5^y] + 1; the upper
    // 64 bits from the double lookup table are the correct bits for [2^x /
    // 5^y], so we have to add 1 here. Note that we rely on the fact that the
    // added 1 that's already stored in the table never overflows into the upper
    // 64 bits.
    uint64 pow5[2];

    double_compute_inv_pow5(q, pow5);
    return mul_shift32(m, pow5[1] + 1, j);
#else
    return mul_shift32(m, double_pow5_inv_split[q][1] + 1, j);
#endif
}

static inline uint32
mul_pow5_div_pow2(uint32 m, uint32 i, int32 j) {
#if defined(RYU_FLOAT_FULL_TABLE)
    return mul_shift32(m, float_pow5_split[i], j);
#elif defined(RYU_OPTIMIZE_SIZE)
    uint64 pow5[2];

    double_compute_pow5(i, pow5);
    return mul_shift32(m, pow5[1], j);
#else
    return mul_shift32(m, double_pow5_split[i][1], j);
#endif
}

#endif /* RYU_F2S_INTRINSICS_H */
