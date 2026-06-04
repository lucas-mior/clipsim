/*
 * Copyright (C) 2025 Mior, Lucas;
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the*License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(GENERIC_C)
#define GENERIC_C

#include <limits.h>
#include <float.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>

#if !defined(error2)
#define error2(...) fprintf(stderr, __VA_ARGS__)
#endif

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_generic 1
#elif !defined(TESTING_generic)
#define TESTING_generic 0
#endif

#if 1 == TESTING_generic
  #define TRAP(...) raise(SIGILL)
#elif !defined(TRAP)
  #if defined(__GNUC__) || defined(__clang__)
    #define TRAP(...) __builtin_trap()
  #elif defined(_MSC_VER)
    #define TRAP(...) __debugbreak()
  #else
    #define TRAP(...) *(int *)0 = 0
  #endif
#endif

#include "primitives.h"
#include "base_macros.h"

#include <assert.h>
#include <stdarg.h>
#include <string.h>

static int strlen2(char *string) {
    return (int)strlen(string);
}

#if !defined(GENERIC_S_BSZ)
#define GENERIC_S_BSZ 64
#endif

#if !defined(S_BSZ)
#define S_BSZ GENERIC_S_BSZ
#endif

#if !defined(MACRO_NAME)
#define MACRO_NAME(X) #X
#endif
#if !defined(MACRO_VALUE)
#define MACRO_VALUE(X) MACRO_NAME(X)
#endif

static inline int
fprint_0(FILE *restrict fp, ... /* strings, NULL */) {
    int count = 0;
    char *s;

    va_list ap;
    va_start(ap, fp);

    while ((s = (char *)va_arg(ap, char *))) {
        int64 slen;
        if (fputs(s, fp) == EOF) {
            va_end(ap);
            return -1;
        }

        slen = strlen2(s);
        if ((int64)INT_MAX - (int64)count < slen) {
            count = INT_MAX;
        } else {
            count += slen;
        }
    }

    va_end(ap);
    return count;
}

static inline int
snprint_0(char *restrict buf, int64 bufSize, ... /* strings, NULL */) {
    va_list ap;
    int64 remainingLen;
    int64 requiredLen = 0;
    char *dst = buf;
    char *s;

    if (bufSize) {
        remainingLen = bufSize - 1;
    } else {
        remainingLen = 0;
    }

    va_start(ap, bufSize);

    if (buf && bufSize) {
        buf[0] = '\0';
    }

    while ((s = va_arg(ap, char *))) {
        int64 sLen = strlen2(s);
        requiredLen += sLen;

        if (dst && remainingLen) {
            int64 copyLen;
            if (remainingLen < sLen) {
                copyLen = remainingLen;
            } else {
                copyLen = sLen;
            }
            memcpy(dst, s, (size_t)copyLen);
            dst += copyLen;
            remainingLen -= copyLen;
            *dst = '\0';
        }
    }

    va_end(ap);
    if (requiredLen > (int64)INT_MAX) {
        return INT_MAX;
    }
    return (int)requiredLen;
}

/* Like snprintf but returns a pointer to the buffer. */
static inline char *
toString(char *restrict buf, int64 bufSize, char *restrict fmt, ...) {
    va_list ap;

    assert(buf);
    assert(bufSize);
    assert(fmt);

    va_start(ap, fmt);
    vsnprintf(buf, (size_t)bufSize, fmt, ap);
    va_end(ap);
    return buf;
}

#define fprint(FP, ...) fprint_0((FP), __VA_ARGS__, (char *)0)
#define snprint(BUF, BSZ, ...) snprint_0((BUF), (BSZ), __VA_ARGS__, (char *)0)
#define print0(...) fprint_0(stdout, __VA_ARGS__, (char *)0)

#define S(X) toString((char[S_BSZ]){ "" }, S_BSZ, _Generic((X), \
    void *: "%p", \
    char *: "%s", \
    bool: "%i", \
    char: "%c", \
    schar: "%hhi", \
    short: "%hi", \
    int: "%i", \
    long: "%li", \
    llong: "%lli", \
    uchar: "%hhu", \
    ushort: "%hu", \
    uint: "%u", \
    ulong: "%lu", \
    ullong: "%llu", \
    float: "%." MACRO_VALUE(FLT_DIG) "g", \
    double: "%." MACRO_VALUE(DBL_DIG) "g", \
    default: _Generic((X), \
        ldouble: "%." MACRO_VALUE(LDBL_DIG) "Lg", \
        default: "%p" \
    ) \
), (X))

#define V(X) "", S(X), ""
#define W(X) "", (X), ""
#define SF(F, X) toString((char[S_BSZ]){ "" }, S_BSZ, (F), (X))
#define VF(F, X) "", SF((F), (X)), ""


#define TYPENAME(VAR)        \
_Generic((VAR),              \
    void*:   "void*",        \
    char*:   "char*",        \
    bool:    "bool",         \
    char:    "char",         \
    schar:   "schar",        \
    short:   "short",        \
    int:     "int",          \
    long:    "long",         \
    llong:   "llong",        \
    uchar:   "uchar",        \
    ushort:  "ushort",       \
    uint:    "uint",         \
    ulong:   "ulong",        \
    ullong:  "ullong",       \
    float:   "float",        \
    double:  "double",       \
    default: _Generic((VAR), \
        ldouble: "ldouble",  \
        default: "unknown"   \
    )                        \
)

#define MINOF(VARIABLE)           \
_Generic((VARIABLE),              \
    schar:   SCHAR_MIN,           \
    short:   SHRT_MIN,            \
    int:     INT_MIN,             \
    long:    LONG_MIN,            \
    llong:   LLONG_MIN,           \
    uchar:   0,                   \
    ushort:  0,                   \
    uint:    0u,                  \
    ulong:   0ul,                 \
    ullong:  0ull,                \
    char:    CHAR_MIN,            \
    bool:    0,                   \
    float:   -FLT_MAX,            \
    double:  -DBL_MAX,            \
    default: _Generic((VARIABLE), \
        ldouble: -LDBL_MAX,       \
        default: 0                \
    )                             \
)

#define MAXOF(VARIABLE)           \
_Generic((VARIABLE),              \
    schar:   SCHAR_MAX,           \
    short:   SHRT_MAX,            \
    int:     INT_MAX,             \
    long:    LONG_MAX,            \
    llong:   LLONG_MAX,           \
    uchar:   UCHAR_MAX,           \
    ushort:  USHRT_MAX,           \
    uint:    UINT_MAX,            \
    ulong:   ULONG_MAX,           \
    ullong:  ULLONG_MAX,          \
    char:    CHAR_MAX,            \
    bool:    1,                   \
    float:   FLT_MAX,             \
    double:  DBL_MAX,             \
    default: _Generic((VARIABLE), \
        ldouble: LDBL_MAX,        \
        default: 1                \
    )                             \
)

static ldouble
ldouble_from_voidp(void* x) {
    (void)x;
    TRAP();
    return (ldouble)0.0;
}
static ldouble
ldouble_from_charp(char* x) {
    (void)x;
    TRAP();
    return (ldouble)0.0;
}
static ldouble
ldouble_from_bool(bool x) {
    (void)x;
    TRAP();
    return (ldouble)0.0;
}
static ldouble
ldouble_from_char(char x) {
    (void)x;
    TRAP();
    return (ldouble)0.0;
}
static ldouble ldouble_from_schar  (schar x)   {
    return (ldouble)x;
}
static ldouble ldouble_from_short  (short x)   {
    return (ldouble)x;
}
static ldouble ldouble_from_int    (int x)     {
    return (ldouble)x;
}
static ldouble ldouble_from_long   (long x)    {
    return (ldouble)x;
}
static ldouble ldouble_from_llong  (llong x)   {
    return (ldouble)x;
}
static ldouble ldouble_from_uchar  (uchar x)   {
    return (ldouble)x;
}
static ldouble ldouble_from_ushort (ushort x)  {
    return (ldouble)x;
}
static ldouble ldouble_from_uint   (uint x)    {
    return (ldouble)x;
}
static ldouble ldouble_from_ulong  (ulong x)   {
    return (ldouble)x;
}
static ldouble ldouble_from_ullong (ullong x)  {
    return (ldouble)x;
}
static ldouble ldouble_from_float  (float x)   {
    return (ldouble)x;
}
static ldouble ldouble_from_double (double x)  {
    return (ldouble)x;
}
static ldouble ldouble_from_ldouble(ldouble x) {
    return x;
}

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
    TYPE_OTHER = 0,
};

#define TYPEID(VAR) \
_Generic((VAR), \
    void*:   TYPE_VOIDP,       \
    char*:   TYPE_CHARP,       \
    bool:    TYPE_BOOL,        \
    char:    TYPE_CHAR,        \
    schar:   TYPE_SCHAR,       \
    short:   TYPE_SHORT,       \
    int:     TYPE_INT,         \
    long:    TYPE_LONG,        \
    llong:   TYPE_LLONG,       \
    uchar:   TYPE_UCHAR,       \
    ushort:  TYPE_USHORT,      \
    uint:    TYPE_UINT,        \
    ulong:   TYPE_ULONG,       \
    ullong:  TYPE_ULLONG,      \
    float:   TYPE_FLOAT,       \
    double:  TYPE_DOUBLE,      \
    default: _Generic((VAR),   \
        ldouble: TYPE_LDOUBLE, \
        default: TYPE_OTHER    \
    )                          \
)

union Primitive {
    void* avoidp;
    char* acharp;
    bool abool;
    char achar;
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
    float afloat;
    double adouble;
    ldouble aldouble;
};

static llong
typebits(enum Type type) {
    llong size = 0;
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
    case TYPE_OTHER:
    default: TRAP();
    }
    return size*CHAR_BIT;
}

#define TYPEBITS(VAR) (sizeof(VAR)*CHAR_BIT)

static char *
typename(enum Type type) {
    switch (type) {
    case TYPE_VOIDP:  return "void*";
    case TYPE_CHARP:  return "char*";
    case TYPE_BOOL:   return "bool";
    case TYPE_CHAR:   return "char";
    case TYPE_SCHAR:  return "schar";
    case TYPE_SHORT:  return "short";
    case TYPE_INT:    return "int";
    case TYPE_LONG:   return "long";
    case TYPE_LLONG:  return "llong";
    case TYPE_UCHAR:  return "uchar";
    case TYPE_USHORT: return "ushort";
    case TYPE_UINT:   return "uint";
    case TYPE_ULONG:  return "ulong";
    case TYPE_ULLONG: return "ullong";
    case TYPE_FLOAT:  return "float";
    case TYPE_DOUBLE: return "double";
    case TYPE_LDOUBLE: return "ldouble";
    case TYPE_OTHER:
    default:           return "unknown type";
    }
}

static ldouble
ldouble_get(union Primitive var, enum Type type) {
    switch (type) {
    case TYPE_VOIDP:   TRAP(); break;
    case TYPE_CHARP:   TRAP(); break;
    case TYPE_BOOL:    TRAP(); break;
    case TYPE_CHAR:    TRAP(); break;
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
    case TYPE_OTHER:
    default:           TRAP(); break;
    }
    return (ldouble)0.0;
}

void UNSUPPORTED_TYPE_FOR_LDOUBLE_GET_GENERIC(void);

#define LDOUBLE_GET(x) \
_Generic((x), \
    void*:   ldouble_from_voidp,                                  \
    char*:   ldouble_from_charp,                                  \
    bool:    ldouble_from_bool,                                   \
    char:    ldouble_from_char,                                   \
    schar:   ldouble_from_schar,                                  \
    short:   ldouble_from_short,                                  \
    int:     ldouble_from_int,                                    \
    long:    ldouble_from_long,                                   \
    llong:   ldouble_from_llong,                                  \
    uchar:   ldouble_from_uchar,                                  \
    ushort:  ldouble_from_ushort,                                 \
    uint:    ldouble_from_uint,                                   \
    ulong:   ldouble_from_ulong,                                  \
    ullong:  ldouble_from_ullong,                                 \
    float:   ldouble_from_float,                                  \
    double:  ldouble_from_double,                                 \
    default: _Generic((x),                                        \
        ldouble: ldouble_from_ldouble,                            \
        default: UNSUPPORTED_TYPE_FOR_LDOUBLE_GET_GENERIC \
    )                                                             \
)(x)

#if defined(__GNUC__) || defined(__clang__)
#define LDOUBLE_GET2(VAR, TYPE) ldouble_get((union Primitive)(VAR), TYPE)
#else
#define LDOUBLE_GET2(VAR, TYPE) LDOUBLE_GET(VAR)
#endif

#define PRINT_SIGNED(VAR, TYPE) \
  fprintf(stderr, "["GREEN("%s%lld")"]%s = %lld ", \
                  typename(TYPE), typebits(TYPE), #VAR, (llong)(VAR))

#define PRINT_UNSIGNED(VAR, TYPE) \
  fprintf(stderr, "["GREEN("%s%lld")"]%s = %llu ", \
                  typename(TYPE), typebits(TYPE), #VAR, (ullong)(VAR))

#if defined(__CPROC__)
#define LDOUBLE_FORMAT "%f"
#else
#define LDOUBLE_FORMAT "%Lf"
#endif

#define PRINT_LDOUBLE(VAR, TYPE) \
  fprintf(stderr, "["GREEN("%s%lld")"]%s = "LDOUBLE_FORMAT" ", \
                  typename(TYPE), typebits(TYPE), #VAR, LDOUBLE_GET2(VAR, TYPE))

#define PRINT_OTHER(VAR, TYPE, FORMAT, CAST) \
  fprintf(stderr, "["GREEN("%s%lld")"]%s = "FORMAT" ", \
                  typename(TYPE), typebits(TYPE), #VAR, (CAST)(uintptr_t)(VAR))

#define PRINT(VAR) \
_Generic((VAR), \
    void*:   PRINT_OTHER(VAR,    TYPE_VOIDP,   "%p",           void*), \
    char*:   PRINT_OTHER(VAR,    TYPE_CHARP,   RED("\"%s\""),  char*), \
    bool:    PRINT_OTHER(VAR,    TYPE_BOOL,    "%u",           bool),  \
    char:    PRINT_OTHER(VAR,    TYPE_CHAR,    YELLOW("'%c'"), char),  \
    schar:   PRINT_SIGNED(VAR,   TYPE_SCHAR),                          \
    short:   PRINT_SIGNED(VAR,   TYPE_SHORT),                          \
    int:     PRINT_SIGNED(VAR,   TYPE_INT),                            \
    long:    PRINT_SIGNED(VAR,   TYPE_LONG),                           \
    llong:   PRINT_SIGNED(VAR,   TYPE_LLONG),                          \
    uchar:   PRINT_UNSIGNED(VAR, TYPE_UCHAR),                          \
    ushort:  PRINT_UNSIGNED(VAR, TYPE_USHORT),                         \
    uint:    PRINT_UNSIGNED(VAR, TYPE_UINT),                           \
    ulong:   PRINT_UNSIGNED(VAR, TYPE_ULONG),                          \
    ullong:  PRINT_UNSIGNED(VAR, TYPE_ULLONG),                         \
    float:   PRINT_LDOUBLE(VAR,  TYPE_FLOAT),                          \
    double:  PRINT_LDOUBLE(VAR,  TYPE_DOUBLE),                         \
    default: _Generic((VAR),                                           \
        ldouble: PRINT_LDOUBLE(VAR,  TYPE_LDOUBLE),                    \
        default: 0                                                     \
    )                                                                  \
)

#define PRINTLN(VAR) do { \
    fprintf(stderr, "%s:%d %s():", __FILE__, __LINE__, __func__); \
    PRINT(VAR); \
    fprintf(stderr, "\n"); \
} while (0)

#if 0 == TESTING_generic
static inline void
generic_functions_sink(void) {
    (void)ldouble_from_voidp;
    (void)ldouble_from_charp;
    (void)ldouble_from_bool;
    (void)ldouble_from_char;
    (void)ldouble_from_schar;
    (void)ldouble_from_short;
    (void)ldouble_from_int;
    (void)ldouble_from_long;
    (void)ldouble_from_llong;
    (void)ldouble_from_uchar;
    (void)ldouble_from_ushort;
    (void)ldouble_from_uint;
    (void)ldouble_from_ulong;
    (void)ldouble_from_ullong;
    (void)ldouble_from_float;
    (void)ldouble_from_double;
    (void)ldouble_from_ldouble;

    (void)typebits;
    (void)typename;
    (void)ldouble_get;
    (void)generic_functions_sink;
    return;
}
#endif

#if TESTING_generic

#include <assert.h>
#include <string.h>
#include <stdio.h>

int
main(void) {
    union Primitive primitive;

    assert(MINOF(primitive.afloat)   == -FLT_MAX);
    assert(MINOF(primitive.aint)     == INT_MIN);
#if !defined(__CPROC__)
    assert(MINOF(primitive.adouble)  == -DBL_MAX);
    assert(MINOF(primitive.aldouble) == -LDBL_MAX);
#endif
    assert(MINOF(primitive.allong)   == LLONG_MIN);
    assert(MINOF(primitive.along)    == LONG_MIN);
    assert(MINOF(primitive.aschar)   == SCHAR_MIN);
    assert(MINOF(primitive.ashort)   == SHRT_MIN);
    assert(MINOF(primitive.auchar)   == 0);
    assert(MINOF(primitive.auint)    == 0u);
    assert(MINOF(primitive.aullong)  == 0ull);
    assert(MINOF(primitive.aulong)   == 0ul);
    assert(MINOF(primitive.aushort)  == 0);

#if !defined(__CPROC__)
    assert(MAXOF(primitive.aldouble) == LDBL_MAX);
    assert(MAXOF(primitive.adouble)  == DBL_MAX);
#endif
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
#if !defined(__CPROC__)
    assert(!strcmp(TYPENAME(primitive.aldouble),
                   typename(TYPEID(primitive.aldouble))));
#endif

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
#if !defined(__CPROC__)
        // DBL_MAX is defined with L suffix in the gcc headers,
        // which means long double,
        // and cproc uses the gcc pre processor
        // and cproc uses the gcc headers (at least in this case),
        // and qbe does not support long double,
        // which means that cproc also does not support long double
        double var_double = DBL_MAX;
#else
        double var_double = 1e300;
#endif

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
#if !defined(__CPROC__)
        {
            ldouble var_longdouble = (ldouble)DBL_MAX;
            ldouble var_longdouble2 = LDOUBLE_GET(var_longdouble);
            PRINTLN(var_longdouble);
            PRINTLN(var_longdouble2);
        }
#endif

        PRINTLN(*var_string);
        PRINTLN(var_uint - (uint)var_int);
    }

    {
        char a = 'i';
        char *b = "able";
        int c = 1;
        ldouble d = (ldouble)8.0;
        char *e = "a long string that won't fit in the compound literal buffer. "
                  "You can print it using the W(X) macro.";
        char buf[512];
        char expected[512];
        char small[8];
        FILE *fp;
        int n;

        assert(!strcmp(S(a), "i"));
        assert(!strcmp(S(b), "able"));
        assert(!strcmp(S(c), "1"));
        assert(!strcmp(S((uint)42), "42"));
        assert(!strcmp(S((long)-42), "-42"));
        assert(!strcmp(S((ullong)42), "42"));
        assert(!strcmp(S(true), "1"));
        assert(!strcmp(S(false), "0"));
        assert(!strcmp(SF("0x%02x", 10), "0x0a"));

        n = snprint(buf, sizeof(buf),
                    "Now you can insert var" V(a) V(b) "s in situ:\n"
                    V(c) " divided by " V(d) " equals " V(c/d) "\n");
        assert(n == strlen2("Now you can insert variables in situ:\n"
                            "1 divided by 8 equals 0.125\n"));


        assert(!strcmp(buf, "Now you can insert variables in situ:\n"
                            "1 divided by 8 equals 0.125\n"));

        n = snprint(buf, sizeof(buf),
                    "This is " W(e) " It's " V(strlen(e)) " characters long\n");
        snprintf(expected, sizeof(expected),
                 "This is %s It's %lu characters long\n",
                 e, (ulong)strlen(e));
        assert(n == strlen2(expected));
        assert(!strcmp(buf, expected));

        n = snprint(buf, sizeof(buf),
                    "custom " VF("%04i", c) " " VF("%c", a) "\n");
        assert(n == strlen2("custom 0001 i\n"));
        assert(!strcmp(buf, "custom 0001 i\n"));

        n = snprint(small, sizeof(small), "prefix-" W(e));
        assert(n == (int)(strlen("prefix-") + strlen(e)));
        assert(!strcmp(small, "prefix-"));

        fp = tmpfile();
        assert(fp);
        n = fprint(fp, "file ", V(c), " ", VF("%04i", c), "\n");
        assert(n == strlen2("file 1 0001\n"));
        rewind(fp);
        assert(fgets(buf, sizeof(buf), fp));
        assert(!strcmp(buf, "file 1 0001\n"));
        fclose(fp);

        n = print0("print ", V(a), " ", W(b), "\n");
        assert(n == strlen2("print i able\n"));
        {
            char buffer[16];
            assert((print0(V(c), "\n")
                    == snprintf(buffer, sizeof(buffer), "%d\n", c)));
        }
    }
}

#endif

#endif /* GENERIC_C */
