// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(PRIMITIVES_H)
#define PRIMITIVES_H

#if defined(CHAR_BIT)
_Static_assert(CHAR_BIT == 8, "primitives.h requires CHAR_BIT == 8");
#endif

#define CHAR_BIT2 8

_Static_assert(~(0ull) == 18446744073709551615ul,
               "primitives.h requires unsigned long to be 64 bits");
_Static_assert((unsigned char)~0 == (unsigned char)255,
               "primitives.h requires CHAR_BIT == 8");
_Static_assert(sizeof(char)*CHAR_BIT2      == 8,  "char must be 8 bits");
_Static_assert(sizeof(short)*CHAR_BIT2     == 16, "short must be 16 bits");
_Static_assert(sizeof(int)*CHAR_BIT2       == 32, "int must be 32 bits");
_Static_assert(sizeof(long long)*CHAR_BIT2 == 64, "long long must be 64 bits");
_Static_assert(sizeof(void *)*CHAR_BIT2    == 64, "pointers must be 64 bits");

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
  #if __has_include(<stdint.h>)
    #include <stdint.h>
    #define HAS_STDINT 1
  #else
    #define HAS_STDINT 0
  #endif
#else
  #define HAS_STDINT 0
#endif

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

#if !HAS_STDINT

#define SCHAR_MIN (-127 - 1)
#define SCHAR_MAX 127
#define UCHAR_MAX 255
#define CHAR_MIN (((char)-1 < 0) ? SCHAR_MIN : 0)
#define CHAR_MAX (((char)-1 < 0) ? SCHAR_MAX : UCHAR_MAX)
#define SHRT_MIN (-32767 - 1)
#define SHRT_MAX 32767
#define USHRT_MAX 65535
#define INT_MIN (-2147483647 - 1)
#define INT_MAX 2147483647
#define UINT_MAX 4294967295u

#define LONG_MIN ((sizeof(long) == 4)               \
                  ? (-2147483647L - 1L)             \
                  : (-9223372036854775807ll - 1ll))
#define LONG_MAX ((sizeof(long) == 4)               \
                  ? 2147483647L                     \
                  : 9223372036854775807ll)
#define ULONG_MAX ((sizeof(long) == 4)              \
                   ? 4294967295u                    \
                   : 18446744073709551615ul)

#define LLONG_MIN (-9223372036854775807ll - 1ll)
#define LLONG_MAX 9223372036854775807ll
#define ULLONG_MAX 18446744073709551615ul

#define INT8_MIN SCHAR_MIN
#define INT8_MAX SCHAR_MAX
#define UINT8_MAX UCHAR_MAX

#define INT16_MIN SHRT_MIN
#define INT16_MAX SHRT_MAX
#define UINT16_MAX USHRT_MAX

#define INT32_MIN INT_MIN
#define INT32_MAX INT_MAX
#define UINT32_MAX UINT_MAX

#define INT64_MIN LLONG_MIN
#define INT64_MAX LLONG_MAX
#define UINT64_MAX ULLONG_MAX

#define INTPTR_MIN LLONG_MIN
#define INTPTR_MAX LLONG_MAX
#define UINTPTR_MAX ULLONG_MAX

#define PTRDIFF_MIN LONG_MIN
#define PTRDIFF_MAX LONG_MAX
#define SIZE_MAX ULONG_MAX

#endif

#endif /* PRIMITIVES_H */
