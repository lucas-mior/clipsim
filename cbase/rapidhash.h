/*
 * rapidhash - Very fast, high quality, platform-independent hashing algorithm.
 * Copyright (C) 2024 Nicolas De Carli
 *
 * Based on 'wyhash', by Wang Yi <godspeed_china@yeah.net>
 *
 * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at:
 *   - rapidhash source repository: https://github.com/Nicoshev/rapidhash
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER)
#include <intrin.h>
#if defined(_M_X64) && !defined(_M_ARM64EC)
#pragma intrinsic(_umul128)
#endif
#endif

#if !defined(INTEGERS)
#define INTEGERS
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long ullong;

typedef long long llong;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
#endif

#define NOEXCEPT
#define RAPIDHASH_CONSTEXPR static const
#if !defined(RAPIDHASH_INLINE)
#if defined(__GNUC__) 
#define RAPIDHASH_INLINE static inline __attribute__((always_inline))
#else
#define RAPIDHASH_INLINE static inline
#endif
#endif

#if !defined(RAPIDHASH_PROTECTED)
  #define RAPIDHASH_FAST
#elif defined(RAPIDHASH_FAST)
  #error "cannot define both RAPIDHASH_PROTECTED and RAPIDHASH_FAST."
#endif

#if !defined(RAPIDHASH_COMPACT)
  #define RAPIDHASH_UNROLLED
#elif defined(RAPIDHASH_UNROLLED)
  #error "cannot define both RAPIDHASH_COMPACT and RAPIDHASH_UNROLLED."
#endif

#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
  #define LIKELY(x) __builtin_expect(x, 1)
  #define UNLIKELY(x) __builtin_expect(x, 0)
#else
  #define LIKELY(x) (x)
  #define UNLIKELY(x) (x)
#endif

#if !defined(RAPIDHASH_LITTLE_ENDIAN)
  #if defined(_WIN32) || defined(__LITTLE_ENDIAN__) \
      || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    #define RAPIDHASH_LITTLE_ENDIAN
  #elif defined(__BIG_ENDIAN__) \
      || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    #define RAPIDHASH_BIG_ENDIAN
  #else
#if defined(__GNUC__) || defined(__clang__)
    #warning "could not determine endianness! Falling back to little endian."
#endif
    #define RAPIDHASH_LITTLE_ENDIAN
  #endif
#endif

RAPIDHASH_CONSTEXPR uint64 rapid_seed = 0xbdd89aa982704029ull;
RAPIDHASH_CONSTEXPR uint64 rapid_secret[3] = {
    0x2d358dccaa6c78a5ull,
    0x8bb84b93962eacc9ull,
    0x4b33a62ed433d4a3ull
};

/*
 *  64*64 -> 128bit multiply function.
 *
 *  @param A  Address of 64-bit number.
 *  @param B  Address of 64-bit number.
 *
 *  Calculates 128-bit C = *A * *B.
 *
 *  When RAPIDHASH_FAST is defined:
 *  Overwrites A contents with C's low 64 bits.
 *  Overwrites B contents with C's high 64 bits.
 *
 *  When RAPIDHASH_PROTECTED is defined:
 *  Xors and overwrites A contents with C's low 64 bits.
 *  Xors and overwrites B contents with C's high 64 bits.
 */
RAPIDHASH_INLINE void
rapid_mum(uint64 *A, uint64 *B) {
#if defined(__SIZEOF_INT128__)
    __uint128_t r = *A;
    r *= *B;
  #if defined(RAPIDHASH_PROTECTED)
    *A ^= (uint64)r;
    *B ^= (uint64)(r >> 64);
  #else
    *A = (uint64)r;
    *B = (uint64)(r >> 64);
  #endif
#elif defined(_MSC_VER) && (defined(_WIN64) || defined(_M_HYBRID_CHPE_ARM64))
    uint64 a;
    uint64 b;
  #if defined(_M_X64)
    #if defined(RAPIDHASH_PROTECTED)
    a = _umul128(*A, *B, &b);
    *A ^= a;
    *B ^= b;
    #else
    *A = _umul128(*A, *B, B);
    #endif
  #else
    #if defined(RAPIDHASH_PROTECTED)
    b = __umulh(*A, *B);
    a = (*A)*(*B);
    *A ^= a;
    *B ^= b;
    #else
    {
        uint64 c = __umulh(*A, *B);
        *A = (*A)*(*B);
        *B = c;
    }
    #endif
  #endif
#else
    uint64 ha = *A >> 32;
    uint64 hb = *B >> 32;
    uint64 la = (uint32)*A;
    uint64 lb = (uint32)*B;
    uint64 hi;
    uint64 lo;
    uint64 rh = ha*hb;
    uint64 rm0 = ha*lb;
    uint64 rm1 = hb*la;
    uint64 rl = la*lb;
    uint64 t = rl + (rm0 << 32);
    uint64 c = t < rl;

    lo = t + (rm1 << 32);
    c += lo < t;
    hi = rh + (rm0 >> 32) + (rm1 >> 32) + c;
  #if defined(RAPIDHASH_PROTECTED)
    *A ^= lo;
    *B ^= hi;
  #else
    *A = lo;
    *B = hi;
  #endif
#endif
}

/*
 *  Multiply and xor mix function.
 *
 *  @param A  64-bit number.
 *  @param B  64-bit number.
 *
 *  Calculates 128-bit C = A * B.
 *  Returns 64-bit xor between high and low 64 bits of C.
 */
RAPIDHASH_INLINE uint64
rapid_mix(uint64 A, uint64 B) {
    rapid_mum(&A, &B);
    return A ^ B;
}

#if defined(RAPIDHASH_LITTLE_ENDIAN)
RAPIDHASH_INLINE uint64
rapid_read_64(const uint8 *p) {
    uint64 v;
    memcpy(&v, p, sizeof(*(&v)));
    return v;
}
RAPIDHASH_INLINE uint64
read32(const uint8 *p) {
    uint32 v;
    memcpy(&v, p, sizeof(*(&v)));
    return v;
}
#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
RAPIDHASH_INLINE uint64
rapid_read_64(const uint8 *p) {
    uint64 v;
    memcpy(&v, p, sizeof(*(&v)));
    return __builtin_bswap64(v);
}
RAPIDHASH_INLINE uint64
read32(const uint8 *p) {
    uint32 v;
    memcpy(&v, p, sizeof(*(&v)));
    return __builtin_bswap32(v);
}
#elif defined(_MSC_VER)
RAPIDHASH_INLINE uint64
rapid_read_64(const uint8 *p) {
    uint64 v;
    memcpy(&v, p, sizeof(*(&v)));
    return _byteswap_uint64(v);
}
RAPIDHASH_INLINE uint64
read32(const uint8 *p) {
    uint32 v;
    memcpy(&v, p, sizeof(*(&v)));
    return _byteswap_ulong(v);
}
#else
RAPIDHASH_INLINE uint64
rapid_read_64(const uint8 *p) {
    uint64 v;
    memcpy(&v, p, sizeof(*(&v)));
    return ((v >> 56) & 0xff)
         | ((v >> 40) & 0xff00)
         | ((v >> 24) & 0xff0000)
         | ((v >>  8) & 0xff000000)
         | ((v <<  8) & 0xff00000000)
         | ((v << 24) & 0xff0000000000)
         | ((v << 40) & 0xff000000000000)
         | ((v << 56) & 0xff00000000000000);
}
RAPIDHASH_INLINE uint64
read32(const uint8 *p) {
    uint32 v;
    memcpy(&v, p, sizeof(*(&v)));
    return ((v >> 24) & 0xff)
         | ((v >>  8) & 0xff00)
         | ((v <<  8) & 0xff0000)
         | ((v << 24) & 0xff000000);
}
#endif

/*
 *  Reads and combines 3 bytes of input.
 *
 *  @param p  Buffer to read from.
 *  @param k  Length of @p, in bytes.
 *
 *  Always reads and combines 3 bytes from memory.
 *  Guarantees to read each buffer position at least once.
 *
 *  Returns a 64-bit value containing all three bytes read.
 */
RAPIDHASH_INLINE uint64
readSmall(const uint8 *p, size_t k) {
    return (((uint64)p[0]) << 56) | (((uint64)p[k >> 1]) << 32) | p[k - 1];
}

/*
 *  rapidhash main function.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *  @param seed    64-bit seed used to alter the hash predictably.
 *  @param secret  Triplet of 64-bit secrets used to alter hash predictably.
 *
 *  Returns a 64-bit hash.
 */
RAPIDHASH_INLINE uint64
rapidhash_internal(const void *key, size_t len, uint64 seed,
                   const uint64 *secret) {
    const uint8 *p = (const uint8 *)key;
    const uint64 *s = secret;
    uint64 a;
    uint64 b;

    seed ^= rapid_mix(seed ^ secret[0], secret[1]) ^ len;
    if (LIKELY(len <= 16)) {
        if (LIKELY(len >= 4)) {
            const uint8 *plast = p + len - 4;
            const uint64 delta = ((len & 24) >> (len >> 3));
            a = (read32(p) << 32) | read32(plast);
            b = ((read32(p + delta) << 32) | read32(plast - delta));
        } else if (LIKELY(len > 0)) {
            a = readSmall(p, len);
            b = 0;
        } else {
            a = b = 0;
        }
    } else {
        size_t i = len;
        if (UNLIKELY(i > 48)) {
            uint64 see1 = seed;
            uint64 see2 = seed;
#if defined(RAPIDHASH_UNROLLED)
            while (LIKELY(i >= 96)) {
                seed = rapid_mix(rapid_read_64(p) ^ s[0], rapid_read_64(p + 8) ^ seed);
                see1 = rapid_mix(rapid_read_64(p + 16) ^ s[1], rapid_read_64(p + 24) ^ see1);
                see2 = rapid_mix(rapid_read_64(p + 32) ^ s[2], rapid_read_64(p + 40) ^ see2);
                seed = rapid_mix(rapid_read_64(p + 48) ^ s[0], rapid_read_64(p + 56) ^ seed);
                see1 = rapid_mix(rapid_read_64(p + 64) ^ s[1], rapid_read_64(p + 72) ^ see1);
                see2 = rapid_mix(rapid_read_64(p + 80) ^ s[2], rapid_read_64(p + 88) ^ see2);
                p += 96;
                i -= 96;
            }
            if (UNLIKELY(i >= 48)) {
                seed = rapid_mix(rapid_read_64(p) ^ s[0], rapid_read_64(p + 8) ^ seed);
                see1 = rapid_mix(rapid_read_64(p + 16) ^ s[1], rapid_read_64(p + 24) ^ see1);
                see2 = rapid_mix(rapid_read_64(p + 32) ^ s[2], rapid_read_64(p + 40) ^ see2);
                p += 48;
                i -= 48;
            }
#else
            do {
                seed = rapid_mix(rapid_read_64(p) ^ s[0], rapid_read_64(p + 8) ^ seed);
                see1 = rapid_mix(rapid_read_64(p + 16) ^ s[1], rapid_read_64(p + 24) ^ see1);
                see2 = rapid_mix(rapid_read_64(p + 32) ^ s[2], rapid_read_64(p + 40) ^ see2);
                p += 48;
                i -= 48;
            } while (LIKELY(i >= 48));
#endif
            seed ^= see1 ^ see2;
        }
        if (i > 16) {
            seed = rapid_mix(rapid_read_64(p) ^ s[2], rapid_read_64(p + 8) ^ seed ^ s[1]);
            if (i > 32) {
                seed = rapid_mix(rapid_read_64(p + 16) ^ s[2], rapid_read_64(p + 24) ^ seed);
            }
        }
        a = rapid_read_64(p + i - 16);
        b = rapid_read_64(p + i - 8);
    }
    a ^= secret[1];
    b ^= seed;
    rapid_mum(&a, &b);
    return rapid_mix(a ^ secret[0] ^ len, b ^ secret[1]);
}

RAPIDHASH_INLINE uint64
rapidhash_withSeed(const void *key, size_t len, uint64 seed) {
    return rapidhash_internal(key, len, seed, rapid_secret);
}

RAPIDHASH_INLINE uint64
rapidhash(const void *key, size_t len) {
    return rapidhash_withSeed(key, len, rapid_seed);
}
