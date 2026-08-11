// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(PRIMITIVES_H)
#define PRIMITIVES_H

// libc.h is needed for limits.h and stdint.h
#include "libc.h"

// Note: int64_t is defined as long on unix systems,
// while int64_t is defined as long long on windows.
// defining int64 as long long creates a compatibility between the two,
// allowing we to use %lld for printing them without warnings.
// However, if in the future some unix platform decides
// that long long should be something other than 64 bits,
// this compatibility will be impossible.

_Static_assert(CHAR_BIT == 8, "primitives.h requires CHAR_BIT == 8");

_Static_assert(~(0ull) == 18446744073709551615ul,
               "primitives.h requires unsigned long to be 64 bits");
_Static_assert((unsigned char)~0 == (unsigned char)255,
               "primitives.h requires CHAR_BIT == 8");
_Static_assert(sizeof(char)*CHAR_BIT      == 8,  "char must be 8 bits");
_Static_assert(sizeof(short)*CHAR_BIT     == 16, "short must be 16 bits");
_Static_assert(sizeof(int)*CHAR_BIT       == 32, "int must be 32 bits");
_Static_assert(sizeof(long long)*CHAR_BIT == 64, "long long must be 64 bits");

typedef unsigned char      uchar;
typedef unsigned short     ushort;
typedef unsigned int       uint;
typedef unsigned long      ulong;
typedef unsigned long long ullong;

typedef signed char schar;
typedef long long   llong;
typedef long double ldouble;

typedef schar  int8;
typedef short  int16;
typedef int    int32;
typedef llong  int64;
typedef uchar  uint8;
typedef ushort uint16;
typedef uint   uint32;
typedef ullong uint64;

typedef uintptr_t uintptr;
typedef intptr_t  intptr;

#if CBASE_CRT_MSVC
typedef struct __declspec(align(16)) CbaseMaxAlign {
    char data[16];
} CbaseMaxAlign;
#else
typedef max_align_t CbaseMaxAlign;
#endif

#if SCHAR_MIN != -128
#error "This compiler/machine does not use 2's complement. Throw it out."
#endif
#if SHRT_MIN != -32768
#error "This compiler/machine does not use 2's complement. Throw it out."
#endif
#if INT_MIN != -2147483648
#error "This compiler/machine does not use 2's complement. Throw it out."
#endif
#if (LLONG_MIN + 1) != (-9223372036854775807)
#error "This compiler/machine does not use 2's complement. Throw it out."
#endif

#endif /* PRIMITIVES_H */
