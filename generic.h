#if !defined(GENERIC_H)
#define GENERIC_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if !defined(error2)
#define error2(...) fprintf(stderr, __VA_ARGS__)
#endif

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long ullong;

typedef signed char schar;
typedef long long llong;
typedef long double ldouble;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

void unsupported_type_for_generic(void);

#define TYPENAME(VAR) \
_Generic((VAR), \
  void *:  "void*",   \
  char *:  "char*",   \
  schar:   "schar",   \
  short:   "short",   \
  int:     "int",     \
  long:    "long",    \
  llong:   "llong",   \
  uchar:   "uchar",   \
  ushort:  "ushort",  \
  uint:    "uint",    \
  ulong:   "ulong",   \
  ullong:  "ullong",  \
  ldouble: "ldouble", \
  double:  "double",  \
  float:   "float",   \
  default: _Generic((VAR), \
    int8:    "int8",   \
    int16:   "int16",  \
    int32:   "int32",  \
    int64:   "int64",  \
    uint8:   "uint8",  \
    uint16:  "uint16", \
    uint32:  "uint32", \
    uint64:  "uint64", \
    default: unsupported_type_for_generic() \
  ) \
)

#define MINOF(VARIABLE) \
_Generic((VARIABLE), \
  schar:       SCHAR_MIN, \
  short:       SHRT_MIN,  \
  int:         INT_MIN,   \
  long:        LONG_MIN,  \
  llong:       LLONG_MIN, \
  uchar:       0,         \
  ushort:      0,         \
  uint:        0u,        \
  ulong:       0ul,       \
  ullong:      0ull,      \
  char:        CHAR_MIN,  \
  bool:        0,         \
  float:       -FLT_MAX,  \
  double:      -DBL_MAX,  \
  long double: -LDBL_MAX, \
  default: _Generic((VARIABLE), \
    int8:      INT8_MIN,  \
    int16:     INT16_MIN, \
    int32:     INT32_MIN, \
    int64:     INT64_MIN, \
    uint8:     (uint8)0,  \
    uint16:    (uint16)0, \
    uint32:    (uint32)0, \
    uint64:    (uint64)0, \
    default:   unsupported_type_for_generic() \
  ) \
)

#define MAXOF(VARIABLE) \
_Generic((VARIABLE), \
  schar:       SCHAR_MAX,  \
  short:       SHRT_MAX,   \
  int:         INT_MAX,    \
  long:        LONG_MAX,   \
  llong:       LLONG_MAX,  \
  uchar:       UCHAR_MAX,  \
  ushort:      USHRT_MAX,  \
  uint:        UINT_MAX,   \
  ulong:       ULONG_MAX,  \
  ullong:      ULLONG_MAX, \
  char:        CHAR_MAX,   \
  bool:        1,          \
  float:       FLT_MAX,    \
  double:      DBL_MAX,    \
  long double: LDBL_MAX,    \
  default: _Generic((VARIABLE), \
    int8:      INT8_MAX,   \
    int16:     INT16_MAX,  \
    int32:     INT32_MAX,  \
    int64:     INT64_MAX,  \
    uint8:     UINT8_MAX,  \
    uint16:    UINT16_MAX, \
    uint32:    UINT32_MAX, \
    uint64:    UINT64_MAX, \
    default:   unsupported_type_for_generic() \
  ) \
)

static ldouble ldouble_from_ldouble(ldouble x) { return x;             }
static ldouble ldouble_from_double(double x)   { return (ldouble)x;    }
static ldouble ldouble_from_float(float x)     { return (ldouble)x;    }
static ldouble ldouble_from_schar(schar x)     { return (ldouble)x;    }
static ldouble ldouble_from_short(short x)     { return (ldouble)x;    }
static ldouble ldouble_from_int(int x)         { return (ldouble)x;    }
static ldouble ldouble_from_long(long x)       { return (ldouble)x;    }
static ldouble ldouble_from_llong(llong x)     { return (ldouble)x;    }
static ldouble ldouble_from_uchar(uchar x)     { return (ldouble)x;    }
static ldouble ldouble_from_ushort(ushort x)   { return (ldouble)x;    }
static ldouble ldouble_from_uint(uint x)       { return (ldouble)x;    }
static ldouble ldouble_from_ulong(ulong x)     { return (ldouble)x;    }
static ldouble ldouble_from_ullong(ullong x)   { return (ldouble)x;    }
static ldouble ldouble_from_charp(char *x)     { (void)x; return 0.0l; }
static ldouble ldouble_from_voidp(void *x)     { (void)x; return 0.0l; }
static ldouble ldouble_from_bool(bool x)       { (void)x; return 0.0l; }
static ldouble ldouble_from_char(char x)       { (void)x; return 0.0l; }

typedef enum Type {
    TYPE_LDOUBLE,
    TYPE_DOUBLE,
    TYPE_FLOAT,
    TYPE_SCHAR,
    TYPE_SHORT,
    TYPE_INT,
    TYPE_LONG,
    TYPE_LLONG,
    TYPE_UCHAR,
    TYPE_USHORT,
    TYPE_UINT,
    TYPE_ULONG,
    TYPE_ULLONG,
    TYPE_CHARP,
    TYPE_VOIDP,
    TYPE_BOOL,
    TYPE_CHAR,
} Type;

typedef union LongDoubleUnion {
  ldouble aldouble;
  double adouble;
  float afloat;
  schar aschar;
  short ashort;
  int aint;
  long along;
  llong allong;
  uchar auchar;
  ushort aushort;
  uint auint;
  ulong aulong;
  ullong aullong;
  char *aucharp;
  void *avoidp;
  bool abool;
  char achar;
} LongDoubleUnion;

static char *
typename(Type type) {
    switch (type) {
    case TYPE_LDOUBLE:
        return "ldouble";
    case TYPE_DOUBLE:
        return "double";
    case TYPE_FLOAT:
        return "float";
    case TYPE_SCHAR:
        return "schar";
    case TYPE_SHORT:
        return "short";
    case TYPE_INT:
        return "int";
    case TYPE_LONG:
        return "long";
    case TYPE_LLONG:
        return "llong";
    case TYPE_UCHAR:
        return "uchar";
    case TYPE_USHORT:
        return "ushort";
    case TYPE_UINT:
        return "uint";
    case TYPE_ULONG:
        return "ulong";
    case TYPE_ULLONG:
        return "ullong";
    case TYPE_CHARP:
        return "char*";
    case TYPE_VOIDP:
        return "void*";
    case TYPE_BOOL:
        return "bool";
    case TYPE_CHAR:
        return "char";
    default:
        return "unknown type";
    }
}

static ldouble
ldouble_get(LongDoubleUnion var, Type type) {
    switch (type) {
    case TYPE_LDOUBLE:
        return var.aldouble;
    case TYPE_DOUBLE:
        return (ldouble)var.adouble;
    case TYPE_FLOAT:
        return (ldouble)var.afloat;
    case TYPE_SCHAR:
        return (ldouble)var.aschar;
    case TYPE_SHORT:
        return (ldouble)var.ashort;
    case TYPE_INT:
        return (ldouble)var.aint;
    case TYPE_LONG:
        return (ldouble)var.along;
    case TYPE_LLONG:
        return (ldouble)var.allong;
    case TYPE_UCHAR:
        return (ldouble)var.auchar;
    case TYPE_USHORT:
        return (ldouble)var.aushort;
    case TYPE_UINT:
        return (ldouble)var.auint;
    case TYPE_ULONG:
        return (ldouble)var.aulong;
    case TYPE_ULLONG:
        return (ldouble)var.aullong;
    case TYPE_CHARP:
        return (ldouble)0.0l;
    case TYPE_VOIDP:
        return (ldouble)0.0l;
    case TYPE_BOOL:
        return (ldouble)0.0l;
    case TYPE_CHAR:
        return (ldouble)0.0l;
    default:
        return 0.0l;
    }
}

#define LDOUBLE_GET(x) \
_Generic((x), \
  ldouble: ldouble_from_ldouble, \
  double:  ldouble_from_double, \
  float:   ldouble_from_float, \
  schar:   ldouble_from_schar, \
  short:   ldouble_from_short, \
  int:     ldouble_from_int, \
  long:    ldouble_from_long, \
  llong:   ldouble_from_llong, \
  uchar:   ldouble_from_uchar, \
  ushort:  ldouble_from_ushort, \
  uint:    ldouble_from_uint, \
  ulong:   ldouble_from_ulong, \
  ullong:  ldouble_from_ullong, \
  char *:  ldouble_from_charp, \
  void *:  ldouble_from_voidp, \
  bool:    ldouble_from_bool, \
  char:    ldouble_from_char \
)(x)

#if defined(__GNUC__) || defined(__clang__)
#define LDOUBLE_GET2(VAR, TYPE) ldouble_get((LongDoubleUnion)(VAR), TYPE)
#else
#define LDOUBLE_GET2(VAR, TYPE) LDOUBLE_GET(VAR)
#endif

#endif
