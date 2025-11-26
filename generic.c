#if !defined(GENERIC_C)
#define GENERIC_C

#include <limits.h>
#include <float.h>
#include <stdint.h>
#include <stdbool.h>

#if !defined(error2)
#define error2(...) fprintf(stderr, __VA_ARGS__)
#endif

#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ == 0
#define TESTING_generic 1
#elif !defined(TESTING_generic)
#define TESTING_generic 0
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

#define TYPENAME(VAR) \
_Generic((VAR), \
    void*:   "void*",  \
    char*:   "char*",  \
    bool:    "bool",   \
    char:    "char",   \
    schar:   "schar",  \
    short:   "short",  \
    int:     "int",    \
    long:    "long",   \
    llong:   "llong",  \
    uchar:   "uchar",  \
    ushort:  "ushort", \
    uint:    "uint",   \
    ulong:   "ulong",  \
    ullong:  "ullong", \
    float:   "float",  \
    double:  "double", \
    ldouble: "ldouble" \
)

#define MINOF(VARIABLE) \
_Generic((VARIABLE), \
    schar:   SCHAR_MIN, \
    short:   SHRT_MIN,  \
    int:     INT_MIN,   \
    long:    LONG_MIN,  \
    llong:   LLONG_MIN, \
    uchar:   0,         \
    ushort:  0,         \
    uint:    0u,        \
    ulong:   0ul,       \
    ullong:  0ull,      \
    char:    CHAR_MIN,  \
    bool:    0,         \
    float:   -FLT_MAX,  \
    double:  -DBL_MAX,  \
    ldouble: -LDBL_MAX  \
)

#define MAXOF(VARIABLE) \
_Generic((VARIABLE), \
    schar:   SCHAR_MAX,  \
    short:   SHRT_MAX,   \
    int:     INT_MAX,    \
    long:    LONG_MAX,   \
    llong:   LLONG_MAX,  \
    uchar:   UCHAR_MAX,  \
    ushort:  USHRT_MAX,  \
    uint:    UINT_MAX,   \
    ulong:   ULONG_MAX,  \
    ullong:  ULLONG_MAX, \
    char:    CHAR_MAX,   \
    bool:    1,          \
    float:   FLT_MAX,    \
    double:  DBL_MAX,    \
    ldouble: LDBL_MAX    \
)

// clang-format off
static ldouble ldouble_from_voidp(void* x) {
    (void)x;
    *(volatile int *)0 = 0;
    return 0.0l;
}
static ldouble ldouble_from_charp(char* x) {
    (void)x;
    *(volatile int *)0 = 0;
    return 0.0l;
}
static ldouble ldouble_from_bool(bool x) {
    (void)x;
    *(volatile int *)0 = 0;
    return 0.0l;
}
static ldouble ldouble_from_char(char x) {
    (void)x;
    *(volatile int *)0 = 0;
    return 0.0l;
}
static ldouble ldouble_from_schar(schar x)     { return (ldouble)x; }
static ldouble ldouble_from_short(short x)     { return (ldouble)x; }
static ldouble ldouble_from_int(int x)         { return (ldouble)x; }
static ldouble ldouble_from_long(long x)       { return (ldouble)x; }
static ldouble ldouble_from_llong(llong x)     { return (ldouble)x; }
static ldouble ldouble_from_uchar(uchar x)     { return (ldouble)x; }
static ldouble ldouble_from_ushort(ushort x)   { return (ldouble)x; }
static ldouble ldouble_from_uint(uint x)       { return (ldouble)x; }
static ldouble ldouble_from_ulong(ulong x)     { return (ldouble)x; }
static ldouble ldouble_from_ullong(ullong x)   { return (ldouble)x; }
static ldouble ldouble_from_float(float x)     { return (ldouble)x; }
static ldouble ldouble_from_double(double x)   { return (ldouble)x; }
static ldouble ldouble_from_ldouble(ldouble x) { return x;          }

// clang-format on

enum Type {
    TYPE_VOIDP = 1,
    TYPE_CHARP,
    TYPE_BOOL,
    TYPE_CHAR,
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
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_LDOUBLE,
};

// clang-format off

#define TYPEID(VAR) \
_Generic((VAR), \
    void*:   TYPE_VOIDP,  \
    char*:   TYPE_CHARP,  \
    bool:    TYPE_BOOL,   \
    char:    TYPE_CHAR,   \
    schar:   TYPE_SCHAR,  \
    short:   TYPE_SHORT,  \
    int:     TYPE_INT,    \
    long:    TYPE_LONG,   \
    llong:   TYPE_LLONG,  \
    uchar:   TYPE_UCHAR,  \
    ushort:  TYPE_USHORT, \
    uint:    TYPE_UINT,   \
    ulong:   TYPE_ULONG,  \
    ullong:  TYPE_ULLONG, \
    float:   TYPE_FLOAT,  \
    double:  TYPE_DOUBLE, \
    ldouble: TYPE_LDOUBLE \
)

union Primitive {
    void*   avoidp;
    char*   acharp;
    bool    abool;
    char    achar;
    schar   aschar;
    short   ashort;
    int     aint;
    long    along;
    llong   allong;
    uchar   auchar;
    ushort  aushort;
    uint    auint;
    ulong   aulong;
    ullong  aullong;
    float   afloat;
    double  adouble;
    ldouble aldouble;
};

static llong
typebits(enum Type type) {
    llong size;
    union Primitive primitive;
    void **pointer;

    switch (type) {
    case TYPE_VOIDP:
        pointer = &(primitive.avoidp);
        size = ((char*)(pointer + 1)) - (char*)pointer;
        break;
    case TYPE_CHARP:
        pointer = (void*)&(primitive.acharp);
        size = ((char*)(pointer + 1)) - (char*)pointer;
        break;
    case TYPE_BOOL:    size = sizeof(bool);    break;
    case TYPE_CHAR:    size = sizeof(char);    break;
    case TYPE_SCHAR:   size = sizeof(schar);   break;
    case TYPE_SHORT:   size = sizeof(short);   break;
    case TYPE_INT:     size = sizeof(int);     break;
    case TYPE_LONG:    size = sizeof(long);    break;
    case TYPE_LLONG:   size = sizeof(llong);   break;
    case TYPE_UCHAR:   size = sizeof(uchar);   break;
    case TYPE_USHORT:  size = sizeof(ushort);  break;
    case TYPE_UINT:    size = sizeof(uint);    break;
    case TYPE_ULONG:   size = sizeof(ulong);   break;
    case TYPE_ULLONG:  size = sizeof(ullong);  break;
    case TYPE_FLOAT:   size = sizeof(float);   break;
    case TYPE_DOUBLE:  size = sizeof(double);  break;
    case TYPE_LDOUBLE: size = sizeof(ldouble); break;
    default: *(volatile int *)0 = 0; return 0ll;
    }
    return size*CHAR_BIT;
}

#define TYPEBITS(VAR) (sizeof(VAR)*CHAR_BIT)

static char *
typename(enum Type type) {
    switch (type) {
    case TYPE_VOIDP:   return "void*";
    case TYPE_CHARP:   return "char*";
    case TYPE_BOOL:    return "bool";
    case TYPE_CHAR:    return "char";
    case TYPE_SCHAR:   return "schar";
    case TYPE_SHORT:   return "short";
    case TYPE_INT:     return "int";
    case TYPE_LONG:    return "long";
    case TYPE_LLONG:   return "llong";
    case TYPE_UCHAR:   return "uchar";
    case TYPE_USHORT:  return "ushort";
    case TYPE_UINT:    return "uint";
    case TYPE_ULONG:   return "ulong";
    case TYPE_ULLONG:  return "ullong";
    case TYPE_FLOAT:   return "float";
    case TYPE_DOUBLE:  return "double";
    case TYPE_LDOUBLE: return "ldouble";
    default:           return "unknown type";
    }
}

static ldouble
ldouble_get(union Primitive var, enum Type type) {
    switch (type) {
    case TYPE_VOIDP:   *(volatile int *)0 = 0; return 0.0l;
    case TYPE_CHARP:   *(volatile int *)0 = 0; return 0.0l;
    case TYPE_BOOL:    *(volatile int *)0 = 0; return 0.0l;
    case TYPE_CHAR:    *(volatile int *)0 = 0; return 0.0l;
    case TYPE_SCHAR:   return (ldouble)var.aschar;
    case TYPE_SHORT:   return (ldouble)var.ashort;
    case TYPE_INT:     return (ldouble)var.aint;
    case TYPE_LONG:    return (ldouble)var.along;
    case TYPE_LLONG:   return (ldouble)var.allong;
    case TYPE_UCHAR:   return (ldouble)var.auchar;
    case TYPE_USHORT:  return (ldouble)var.aushort;
    case TYPE_UINT:    return (ldouble)var.auint;
    case TYPE_ULONG:   return (ldouble)var.aulong;
    case TYPE_ULLONG:  return (ldouble)var.aullong;
    case TYPE_FLOAT:   return (ldouble)var.afloat;
    case TYPE_DOUBLE:  return (ldouble)var.adouble;
    case TYPE_LDOUBLE: return var.aldouble;
    default:           *(volatile int *)0 = 0; return 0.0l;
    }
}

// clang-format on

#define LDOUBLE_GET(x) \
_Generic((x), \
    void*:   ldouble_from_voidp,  \
    char*:   ldouble_from_charp,  \
    bool:    ldouble_from_char,   \
    char:    ldouble_from_bool,   \
    schar:   ldouble_from_schar,  \
    short:   ldouble_from_short,  \
    int:     ldouble_from_int,    \
    long:    ldouble_from_long,   \
    llong:   ldouble_from_llong,  \
    uchar:   ldouble_from_uchar,  \
    ushort:  ldouble_from_ushort, \
    uint:    ldouble_from_uint,   \
    ulong:   ldouble_from_ulong,  \
    ullong:  ldouble_from_ullong, \
    float:   ldouble_from_float,  \
    double:  ldouble_from_double, \
    ldouble: ldouble_from_ldouble \
)(x)

#if defined(__GNUC__) || defined(__clang__)
#define LDOUBLE_GET2(VAR, TYPE) ldouble_get((union Primitive)(VAR), TYPE)
#else
#define LDOUBLE_GET2(VAR, TYPE) LDOUBLE_GET(VAR)
#endif

// clang-format off

#define PRINT_SIGNED(VAR, TYPE) \
  fprintf(stderr, "[%s%lld]%s = %lld ", \
                  typename(TYPE), typebits(TYPE), #VAR, (llong)(VAR))

#define PRINT_UNSIGNED(VAR, TYPE) \
  fprintf(stderr, "[%s%lld]%s = %llu ", \
                  typename(TYPE), typebits(TYPE), #VAR, (ullong)(VAR))

#define PRINT_LDOUBLE(VAR, TYPE) \
  fprintf(stderr, "[%s%lld]%s = %Lf ", \
                  typename(TYPE), typebits(TYPE), #VAR, LDOUBLE_GET2(VAR, TYPE))

#define PRINT_OTHER(VAR, TYPE, FORMAT, CAST) \
  fprintf(stderr, "[%s%lld]%s = "FORMAT" ", \
                  typename(TYPE), typebits(TYPE), #VAR, (CAST)(uintptr_t)(VAR))

#define PRINT(VAR) \
_Generic((VAR), \
    void*:   PRINT_OTHER(VAR,    TYPE_VOIDP, "%p",     void*), \
    char*:   PRINT_OTHER(VAR,    TYPE_CHARP, "\"%s\"", char*), \
    bool:    PRINT_OTHER(VAR,    TYPE_BOOL,  "%u",     bool),  \
    char:    PRINT_OTHER(VAR,    TYPE_CHAR,  "'%c'",   char),  \
    schar:   PRINT_SIGNED(VAR,   TYPE_SCHAR),                  \
    short:   PRINT_SIGNED(VAR,   TYPE_SHORT),                  \
    int:     PRINT_SIGNED(VAR,   TYPE_INT),                    \
    long:    PRINT_SIGNED(VAR,   TYPE_LONG),                   \
    llong:   PRINT_SIGNED(VAR,   TYPE_LLONG),                  \
    uchar:   PRINT_UNSIGNED(VAR, TYPE_UCHAR),                  \
    ushort:  PRINT_UNSIGNED(VAR, TYPE_USHORT),                 \
    uint:    PRINT_UNSIGNED(VAR, TYPE_UINT),                   \
    ulong:   PRINT_UNSIGNED(VAR, TYPE_ULONG),                  \
    ullong:  PRINT_UNSIGNED(VAR, TYPE_ULLONG),                 \
    float:   PRINT_LDOUBLE(VAR,  TYPE_FLOAT),                  \
    double:  PRINT_LDOUBLE(VAR,  TYPE_DOUBLE),                 \
    ldouble: PRINT_LDOUBLE(VAR,  TYPE_LDOUBLE)                 \
)

#define PRINTLN(VAR) do { PRINT(VAR); fprintf(stderr, "\n"); } while (0)

#if TESTING_generic
#include <assert.h>
#include <string.h>
#include <stdio.h>

// clang-format off

int
main(void) {
    union Primitive primitive;

    assert(MINOF(primitive.aldouble) == -LDBL_MAX);
    assert(MINOF(primitive.adouble)  == -DBL_MAX);
    assert(MINOF(primitive.afloat)   == -FLT_MAX);
    assert(MINOF(primitive.aschar)   == SCHAR_MIN);
    assert(MINOF(primitive.ashort)   == SHRT_MIN);
    assert(MINOF(primitive.aint)     == INT_MIN);
    assert(MINOF(primitive.along)    == LONG_MIN);
    assert(MINOF(primitive.allong)   == LLONG_MIN);
    assert(MINOF(primitive.auchar)   == 0);
    assert(MINOF(primitive.aushort)  == 0);
    assert(MINOF(primitive.auint)    == 0u);
    assert(MINOF(primitive.aulong)   == 0ul);
    assert(MINOF(primitive.aullong)  == 0ull);
    assert(MINOF(primitive.abool)    == 0);

    assert(MAXOF(primitive.aldouble) == LDBL_MAX);
    assert(MAXOF(primitive.adouble)  == DBL_MAX);
    assert(MAXOF(primitive.afloat)   == FLT_MAX);
    assert(MAXOF(primitive.aschar)   == SCHAR_MAX);
    assert(MAXOF(primitive.ashort)   == SHRT_MAX);
    assert(MAXOF(primitive.aint)     == INT_MAX);
    assert(MAXOF(primitive.along)    == LONG_MAX);
    assert(MAXOF(primitive.allong)   == LLONG_MAX);
    assert(MAXOF(primitive.auchar)   == UCHAR_MAX);
    assert(MAXOF(primitive.aushort)  == USHRT_MAX);
    assert(MAXOF(primitive.auint)    == UINT_MAX);
    assert(MAXOF(primitive.aulong)   == ULONG_MAX);
    assert(MAXOF(primitive.aullong)  == ULLONG_MAX);
    assert(MAXOF(primitive.abool)    == 1);

    assert(!strcmp(TYPENAME(primitive.avoidp),
                   typename(TYPEID(primitive.avoidp))));
    assert(!strcmp(TYPENAME(primitive.acharp),
                   typename(TYPEID(primitive.acharp))));
    assert(!strcmp(TYPENAME(primitive.abool),
                   typename(TYPEID(primitive.abool))));
    assert(!strcmp(TYPENAME(primitive.aschar),
                   typename(TYPEID(primitive.aschar))));
    assert(!strcmp(TYPENAME(primitive.ashort),
                   typename(TYPEID(primitive.ashort))));
    assert(!strcmp(TYPENAME(primitive.aint),
                   typename(TYPEID(primitive.aint))));
    assert(!strcmp(TYPENAME(primitive.along),
                   typename(TYPEID(primitive.along))));
    assert(!strcmp(TYPENAME(primitive.allong),
                   typename(TYPEID(primitive.allong))));
    assert(!strcmp(TYPENAME(primitive.auchar),
                   typename(TYPEID(primitive.auchar))));
    assert(!strcmp(TYPENAME(primitive.aushort),
                   typename(TYPEID(primitive.aushort))));
    assert(!strcmp(TYPENAME(primitive.auint),
                   typename(TYPEID(primitive.auint))));
    assert(!strcmp(TYPENAME(primitive.aulong),
                   typename(TYPEID(primitive.aulong))));
    assert(!strcmp(TYPENAME(primitive.aullong),
                   typename(TYPEID(primitive.aullong))));
    assert(!strcmp(TYPENAME(primitive.afloat),
                   typename(TYPEID(primitive.afloat))));
    assert(!strcmp(TYPENAME(primitive.adouble),
                   typename(TYPEID(primitive.adouble))));
    assert(!strcmp(TYPENAME(primitive.aldouble),
                   typename(TYPEID(primitive.aldouble))));

    {
        int32 var_int32;
        uint32 var_uint32;
        int64 var_int64;
        uint64 var_uint64;

        assert(MAXOF(var_int32) == INT32_MAX);
        assert(MAXOF(var_int64) == INT64_MAX);
        assert(MAXOF(var_uint32) == UINT32_MAX);
        assert(MAXOF(var_uint64) == UINT64_MAX);

        assert(MINOF(var_int32) == INT32_MIN);
        assert(MINOF(var_int64) == INT64_MIN);
        assert(MINOF(var_uint32) == 0u);
        assert(MINOF(var_uint64) == 0ull);
    }

    {
        void* var_voidptr = NULL;
        char* var_string = "a nice string";
        char var_buffer[128] = "a nice buffer";
        bool var_bool = true;
        char var_char = 'c';
        int8 var_int8 = INT8_MAX;
        int16 var_int16 = INT16_MAX;
        int32 var_int32 = INT32_MAX;
        int var_int = INT_MAX;
        int64 var_int64 = INT64_MAX;
        uint8 var_uint8 = UINT8_MAX;
        uint16 var_uint16 = UINT16_MAX;
        uint32 var_uint32 = UINT32_MAX;
        uint var_uint = UINT_MAX;
        uint64 var_uint64 = UINT64_MAX;
        float var_float = FLT_MAX;
        double var_double = DBL_MAX;
        long double var_longdouble = (ldouble)DBL_MAX;

        PRINTLN(var_voidptr);
        PRINTLN(var_string);
        PRINTLN(var_buffer);
        PRINTLN(var_bool);
        PRINTLN(var_char);
        PRINTLN(var_int8);
        PRINTLN(var_int16);
        PRINT(var_int32);
        PRINTLN(var_int);
        PRINTLN(var_int64);
        PRINTLN(var_uint8);
        PRINTLN(var_uint16);
        PRINTLN(var_uint32);
        PRINTLN(var_uint);
        PRINTLN(var_uint64);
        PRINTLN(var_float);
        PRINTLN(var_double);
        PRINTLN(var_longdouble);

        PRINTLN(*var_string);
        PRINTLN(var_uint - (uint)var_int);
        PRINTLN((void*)main);
    }
}

#endif

#endif
