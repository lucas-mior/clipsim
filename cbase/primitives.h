#if !defined(PRIMITIVES_H)
#define PRIMITIVES_H

#include <stdint.h>

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

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#endif /* PRIMITIVES_H */
