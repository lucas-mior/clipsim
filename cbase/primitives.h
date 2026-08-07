// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(PRIMITIVES_H)
#define PRIMITIVES_H

#include <libc.h>

_Static_assert(CHAR_BIT == 8, "primitives.h requires CHAR_BIT == 8");

_Static_assert(~(0ull) == 18446744073709551615ul,
               "primitives.h requires unsigned long to be 64 bits");
_Static_assert((unsigned char)~0 == (unsigned char)255,
               "primitives.h requires CHAR_BIT == 8");
_Static_assert(sizeof(char)*CHAR_BIT      == 8,  "char must be 8 bits");
_Static_assert(sizeof(short)*CHAR_BIT     == 16, "short must be 16 bits");
_Static_assert(sizeof(int)*CHAR_BIT       == 32, "int must be 32 bits");
_Static_assert(sizeof(long long)*CHAR_BIT == 64, "long long must be 64 bits");
_Static_assert(sizeof(void *)*CHAR_BIT    == 64, "pointers must be 64 bits");

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long ullong;

typedef signed char schar;
typedef long long llong;
#if !defined(__CPROC__)
typedef long double ldouble;
#else
typedef double ldouble;
#endif

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

#if SCHAR_MIN != -128
#error "This compiler/machine does not use two's complement for integers. Throw it out."
#endif

#if SHRT_MIN != -32768
#error "This compiler/machine does not use two's complement for integers. Throw it out."
#endif

#if INT_MIN != -2147483648
#error "This compiler/machine does not use two's complement for integers. Throw it out."
#endif

#if (LLONG_MIN + 1) != -9223372036854775807
#error "This compiler/machine does not use two's complement for integers. Throw it out."
#endif

#endif /* PRIMITIVES_H */
