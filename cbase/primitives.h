// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(PRIMITIVES_H)
#define PRIMITIVES_H

#include <limits.h>

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

typedef ullong uintptr;
typedef llong  intptr;

_Static_assert(sizeof(uintptr) == sizeof(void *),
               "uintptr must match pointer width");
_Static_assert(sizeof(intptr) == sizeof(void *),
               "intptr must match pointer width");

#if defined(__has_include)
  #if __has_include(<stdbool.h>)
    #include <stdbool.h>
    #define HAS_STDBOOl 1
  #else
    #define HAS_STDBOOL 0
  #endif
#else
  #define HAS_STDBOOL 0
#endif

#endif /* PRIMITIVES_H */
