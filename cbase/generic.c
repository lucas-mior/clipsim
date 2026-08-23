// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(GENERIC_C)
#define GENERIC_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_generic 1
#elif !defined(TESTING_generic)
#define TESTING_generic 0
#endif

#if TESTING_generic
#define CBASE_IMPLEMENT
#endif

#include "cbase.h"

int
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

        slen = strlen32(s);
        if ((int64)INT_MAX - (int64)count < slen) {
            count = INT_MAX;
        } else {
            count += (int32)slen;
        }
    }

    va_end(ap);
    return count;
}

int
snprint_0(char *restrict buf, int64 bufSize, ... /* strings, NULL */) {
    va_list ap;
    int64 remainingLen;
    int64 requiredLen = 0;
    char *dst = buf;
    char *s;

    ASSERT(bufSize >= 0);
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
        int64 sLen = strlen32(s);
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
char *
toString(char *restrict buf, int64 bufSize, char *restrict fmt, ...) {
    va_list ap;

    ASSERT(buf);
    ASSERT(bufSize > 0);
    ASSERT(fmt);

    va_start(ap, fmt);
    vsnprintf(buf, (size_t)bufSize, fmt, ap);
    va_end(ap);
    return buf;
}

double
double_from_voidp(void *x) {
    (void)x;
    TRAP();
    return 0.0;  // NOLINT
}

double
double_from_charp(char *x) {
    (void)x;
    TRAP();
    return 0.0;  // NOLINT
}

double
double_from_bool(bool x) {
    (void)x;
    TRAP();
    return 0.0;  // NOLINT
}

double
double_from_char(char x) {
    (void)x;
    TRAP();
    return 0.0;  // NOLINT
}

static void
check_integer_fits_in_double(llong x) {
    if (x > (1ll << DBL_MANT_DIG)) {
        TRAP();
    }
    if (x < -(1ll << DBL_MANT_DIG)) {
        TRAP();
    }
    return;
}

double
double_from_schar(schar x) {
    return (double)x;
}

double
double_from_short(short x) {
    return (double)x;
}

double
double_from_int(int x) {
    return (double)x;
}

double
double_from_long(long x) {
    check_integer_fits_in_double((llong)x);
    return (double)x;
}

double
double_from_llong(llong x) {
    check_integer_fits_in_double(x);
    return (double)x;
}

double
double_from_uchar(uchar x) {
    return (double)x;
}

double
double_from_ushort(ushort x) {
    return (double)x;
}

double
double_from_uint(uint x) {
    return (double)x;
}

double
double_from_ulong(ulong x) {
#if ULONG_MAX >= LLONG_MAX
    if ((ullong)x >= (ullong)LLONG_MAX) {
        TRAP();
    }
#endif
    check_integer_fits_in_double((llong)x);
    return (double)x;
}

double
double_from_ullong(ullong x) {
    if (x >= (ullong)LLONG_MAX) {
        TRAP();
    }
    check_integer_fits_in_double((llong)x);
    return (double)x;
}

double
double_from_float(float x) {
    return (double)x;
}

double
double_from_double(double x)  {
    return (double)x;
}

llong
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
    case TYPE_BOOL:    size = SIZEOF(bool);    break;
    case TYPE_CHAR:    size = SIZEOF(char);    break;
    case TYPE_SCHAR:   size = SIZEOF(schar);   break;
    case TYPE_SHORT:   size = SIZEOF(short);   break;
    case TYPE_INT:     size = SIZEOF(int);     break;
    case TYPE_LONG:    size = SIZEOF(long);    break;
    case TYPE_LLONG:   size = SIZEOF(llong);   break;
    case TYPE_UCHAR:   size = SIZEOF(uchar);   break;
    case TYPE_USHORT:  size = SIZEOF(ushort);  break;
    case TYPE_UINT:    size = SIZEOF(uint);    break;
    case TYPE_ULONG:   size = SIZEOF(ulong);   break;
    case TYPE_ULLONG:  size = SIZEOF(ullong);  break;
    case TYPE_FLOAT:   size = SIZEOF(float);   break;
    case TYPE_DOUBLE:  size = SIZEOF(double);  break;
    case TYPE_OTHER:
    default: TRAP();
    }
    return size*CHAR_BIT;
}

char *
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
    case TYPE_OTHER:
    default:           return "unknown type";
    }
}

double
double_get(union Primitive var, enum Type type) {
    switch (type) {
    case TYPE_VOIDP:
        TRAP();
        break;  // NOLINT
    case TYPE_CHARP:
        TRAP();
        break;  // NOLINT
    case TYPE_BOOL:
        TRAP();
        break;  // NOLINT
    case TYPE_CHAR:
        TRAP();
        break;  // NOLINT
    case TYPE_SCHAR:
        return (double)var.aschar;
    case TYPE_SHORT:
        return (double)var.ashort;
    case TYPE_INT:
        return (double)var.aint;
    case TYPE_LONG:
        check_integer_fits_in_double(var.along);
        return (double)var.along;
    case TYPE_LLONG:
        check_integer_fits_in_double(var.allong);
        return (double)var.allong;
    case TYPE_UCHAR:
        return (double)var.auchar;
    case TYPE_USHORT:
        return (double)var.aushort;
    case TYPE_UINT:
        return (double)var.auint;
    case TYPE_ULONG:
#if ULONG_MAX >= LLONG_MAX
        if (var.aulong >= (ullong)LLONG_MAX) {
            TRAP();
        }
#endif
        check_integer_fits_in_double((llong)var.aulong);
        return (double)var.aulong;
    case TYPE_ULLONG:
        if (var.aullong >= (ullong)LLONG_MAX) {
            TRAP();
        }
        check_integer_fits_in_double((llong)var.aullong);
        return (double)var.aullong;
    case TYPE_FLOAT:
        return (double)var.afloat;
    case TYPE_DOUBLE:
        return (double)var.adouble;
    case TYPE_OTHER:
    default:
        TRAP();
        break;  // NOLINT
    }
    return (double)0.0;
}

#if TESTING_generic
#define CBASE_IMPLEMENT
#include "cbase.h"

int
main(void) {
    union Primitive primitive;

    ASSERT(MINOF(primitive.afloat)  == -FLT_MAX);
    ASSERT(MINOF(primitive.aint)    == INT_MIN);
    ASSERT(MINOF(primitive.allong)  == LLONG_MIN);
    ASSERT(MINOF(primitive.along)   == LONG_MIN);
    ASSERT(MINOF(primitive.aschar)  == SCHAR_MIN);
    ASSERT(MINOF(primitive.ashort)  == SHRT_MIN);
    ASSERT(MINOF(primitive.auchar)  == 0);
    ASSERT(MINOF(primitive.auint)   == 0u);
    ASSERT(MINOF(primitive.aullong) == 0ull);
    ASSERT(MINOF(primitive.aulong)  == 0ul);
    ASSERT(MINOF(primitive.aushort) == 0);

    ASSERT(MAXOF(primitive.afloat)  == FLT_MAX);
    ASSERT(MAXOF(primitive.aschar)  == SCHAR_MAX);
    ASSERT(MAXOF(primitive.ashort)  == SHRT_MAX);
    ASSERT(MAXOF(primitive.aint)    == INT_MAX);
    ASSERT(MAXOF(primitive.along)   == LONG_MAX);
    ASSERT(MAXOF(primitive.allong)  == LLONG_MAX);
    ASSERT(MAXOF(primitive.auchar)  == UCHAR_MAX);
    ASSERT(MAXOF(primitive.aushort) == USHRT_MAX);
    ASSERT(MAXOF(primitive.auint)   == UINT_MAX);
    ASSERT(MAXOF(primitive.aulong)  == ULONG_MAX);
    ASSERT(MAXOF(primitive.aullong) == ULLONG_MAX);
    ASSERT(MAXOF(primitive.abool)   == 1);

    ASSERT_EQUAL(TYPENAME(primitive.avoidp),
                 typename(TYPEID(primitive.avoidp)));
    ASSERT_EQUAL(TYPENAME(primitive.acharp),
                 typename(TYPEID(primitive.acharp)));
    ASSERT_EQUAL(TYPENAME(primitive.abool),
                 typename(TYPEID(primitive.abool)));
    ASSERT_EQUAL(TYPENAME(primitive.aschar),
                 typename(TYPEID(primitive.aschar)));
    ASSERT_EQUAL(TYPENAME(primitive.ashort),
                 typename(TYPEID(primitive.ashort)));
    ASSERT_EQUAL(TYPENAME(primitive.aint),
                 typename(TYPEID(primitive.aint)));
    ASSERT_EQUAL(TYPENAME(primitive.along),
                 typename(TYPEID(primitive.along)));
    ASSERT_EQUAL(TYPENAME(primitive.allong),
                 typename(TYPEID(primitive.allong)));
    ASSERT_EQUAL(TYPENAME(primitive.auchar),
                 typename(TYPEID(primitive.auchar)));
    ASSERT_EQUAL(TYPENAME(primitive.aushort),
                 typename(TYPEID(primitive.aushort)));
    ASSERT_EQUAL(TYPENAME(primitive.auint),
                 typename(TYPEID(primitive.auint)));
    ASSERT_EQUAL(TYPENAME(primitive.aulong),
                 typename(TYPEID(primitive.aulong)));
    ASSERT_EQUAL(TYPENAME(primitive.aullong),
                 typename(TYPEID(primitive.aullong)));
    ASSERT_EQUAL(TYPENAME(primitive.afloat),
                 typename(TYPEID(primitive.afloat)));
    ASSERT_EQUAL(TYPENAME(primitive.adouble),
                 typename(TYPEID(primitive.adouble)));

    {
        int32 var_int32;
        uint32 var_uint32;
        int64 var_int64;
        uint64 var_uint64;

        ASSERT(MAXOF(var_int32) == INT32_MAX);
        ASSERT(MAXOF(var_int64) == INT64_MAX);
        ASSERT(MAXOF(var_uint32) == UINT32_MAX);
        ASSERT(MAXOF(var_uint64) == UINT64_MAX);

        ASSERT(MINOF(var_int32) == INT32_MIN);
        ASSERT(MINOF(var_int64) == INT64_MIN);
        ASSERT(MINOF(var_uint32) == 0u);
        ASSERT(MINOF(var_uint64) == 0ull);
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

        PRINTLN(*var_string);
        PRINTLN(var_uint - (uint)var_int);
    }

    {
        char a = 'i';
        char *b = "able";
        int c = 1;
        double d = (double)8.0;
        char *e = "a long string that won't fit in the compound literal "
                  "buffer. "
                  "You can print it using the W(X) macro.";
        char buf[512];
        char expected[512];
        char small2[8];
        FILE *fp;
        int n;

        ASSERT_EQUAL(S_(a), "i");
        ASSERT_EQUAL(S_(b), "able");
        ASSERT_EQUAL(S_(c), "1");
        ASSERT_EQUAL(S_((uint)42), "42");
        ASSERT_EQUAL(S_((long)-42), "-42");
        ASSERT_EQUAL(S_((ullong)42), "42");
        ASSERT_EQUAL(S_(true), "1");
        ASSERT_EQUAL(S_(false), "0");
        ASSERT_EQUAL(SF("0x%02x", 10), "0x0a");

        n = snprint(buf, SIZEOF(buf),
                    "Now you can insert var" V(a) V(b) "s in situ:\n"
                    V(c) " divided by " V(d) " equals " V(c/d) "\n");
        ASSERT(n == strlen32("Now you can insert variables in situ:\n"
                            "1 divided by 8 equals 0.125\n"));

        ASSERT_EQUAL(buf, "Now you can insert variables in situ:\n"
                          "1 divided by 8 equals 0.125\n");

        n = snprint(buf, SIZEOF(buf),
                    "This is " W(e) " It's " V(strlen(e)) " characters long\n");
        snprintf(expected, SIZEOF(expected),
                 "This is %s It's %lu characters long\n",
                 e, (ulong)strlen(e));
        ASSERT(n == strlen32(expected));
        ASSERT_EQUAL(buf, expected);

        n = snprint(buf, SIZEOF(buf),
                    "custom " VF("%04i", c) " " VF("%c", a) "\n");
        ASSERT(n == strlen32("custom 0001 i\n"));
        ASSERT_EQUAL(buf, "custom 0001 i\n");

        n = snprint(small2, SIZEOF(small2), "prefix-" W(e));
        ASSERT(n == (int)(strlen("prefix-") + strlen(e)));
        ASSERT_EQUAL(small2, "prefix-");

        fp = tmpfile();
        ASSERT(fp);
        n = fprint(fp, "file ", V(c), " ", VF("%04i", c), "\n");
        ASSERT(n == strlen32("file 1 0001\n"));
        rewind(fp);
        ASSERT(fgets(buf, SIZEOF(buf), fp));
        ASSERT_EQUAL(buf, "file 1 0001\n");
        fclose(fp);

        n = print0("print ", V(a), " ", W(b), "\n");
        ASSERT(n == strlen32("print i able\n"));
        {
            char buffer[16];
            ASSERT((print0(V(c), "\n")
                    == snprintf(buffer, SIZEOF(buffer), "%d\n", c)));
        }
        print0("PRINTING a=", V(a), "; b=", V(b), "\n");
    }
}

#endif

#endif /* GENERIC_C */
