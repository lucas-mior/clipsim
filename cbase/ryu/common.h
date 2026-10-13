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

#if !defined(RYU_COMMON_H)
#define RYU_COMMON_H

#include "cbase.h"

#if defined(_M_IX86) || defined(_M_ARM)
#define RYU_32_BIT_PLATFORM
#endif

// Returns the number of decimal digits in v, which must not contain more than
// 9 digits.
static inline uint32
decimal_length9(uint32 v) {
    // Function precondition: v is not a 10-digit number.
    // (f2s: 9 digits are sufficient for round-tripping.)
    // (d2fixed: We print 9-digit blocks.)
    assert(v < 1000000000);
    if (v >= 100000000) {
        return 9;
    }
    if (v >= 10000000) {
        return 8;
    }
    if (v >= 1000000) {
        return 7;
    }
    if (v >= 100000) {
        return 6;
    }
    if (v >= 10000) {
        return 5;
    }
    if (v >= 1000) {
        return 4;
    }
    if (v >= 100) {
        return 3;
    }
    if (v >= 10) {
        return 2;
    }
    return 1;
}

// Returns e == 0 ? 1 : [log_2(5^e)]; requires 0 <= e <= 3528.
static inline int32
log2_pow5(int32 e) {
    // This approximation works up to the point that the multiplication
    // overflows at e = 3529. If the multiplication were done in 64 bits, it
    // would fail at 5^4004, which is just greater than 2^9297.
    assert(e >= 0);
    assert(e <= 3528);
    return (int32)((((uint32)e)*1217359) >> 19);
}

// Returns e == 0 ? 1 : ceil(log_2(5^e)); requires 0 <= e <= 3528.
static inline int32
pow5_bits(int32 e) {
    // This approximation works up to the point that the multiplication
    // overflows at e = 3529. If the multiplication were done in 64 bits, it
    // would fail at 5^4004, which is just greater than 2^9297.
    assert(e >= 0);
    assert(e <= 3528);
    return (int32)(((((uint32)e)*1217359) >> 19) + 1);
}

// Returns e == 0 ? 1 : ceil(log_2(5^e)); requires 0 <= e <= 3528.
static inline int32
ceil_log2_pow5(int32 e) {
    return log2_pow5(e) + 1;
}

// Returns floor(log_10(2^e)); requires 0 <= e <= 1650.
static inline uint32
log10_pow2(int32 e) {
    // The first value this approximation fails for is 2^1651, which is just
    // greater than 10^297.
    assert(e >= 0);
    assert(e <= 1650);
    return (((uint32)e)*78913) >> 18;
}

// Returns floor(log_10(5^e)); requires 0 <= e <= 2620.
static inline uint32
log10_pow5(int32 e) {
    // The first value this approximation fails for is 5^2621, which is just
    // greater than 10^1832.
    assert(e >= 0);
    assert(e <= 2620);
    return (((uint32)e)*732923) >> 20;
}

static inline int32
copy_special_str(char *result, bool sign, bool exponent, bool mantissa) {
    int32 sign_chars = 0;

    if (mantissa) {
        memcpy(result, "NaN", 3);
        return 3;
    }
    if (sign) {
        result[0] = '-';
        sign_chars = 1;
    }
    if (exponent) {
        memcpy(result + sign_chars, "Infinity", 8);
        return sign_chars + 8;
    }
    memcpy(result + sign_chars, "0E0", 3);
    return sign_chars + 3;
}

static inline uint32
float_to_bits(float f) {
    uint32 bits = 0;

    memcpy(&bits, &f, SIZEOF(bits));
    return bits;
}

static inline uint64
double_to_bits(double d) {
    uint64 bits = 0;

    memcpy(&bits, &d, SIZEOF(bits));
    return bits;
}

#endif /* RYU_COMMON_H */
