#if !defined(ASSERT_C)
#define ASSERT_C

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <float.h>

#if defined(__GNUC__) || defined(__clang__)
#define trap(...) __builtin_trap()
#elif defined(_MSC_VER)
#define trap(...) __debugbreak()
#else
#define trap(...) *(volatile int *)0 = 0
#endif

#define ASSERT(c) if (!(c)) trap()

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_assert 1
#elif !defined(TESTING_assert)
#define TESTING_assert 0
#endif

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

// clang-format off

#if !defined(MINOF)

#define MINOF(VARIABLE) \
_Generic((VARIABLE), \
  signed char: SCHAR_MIN, \
  short:       SHRT_MIN,  \
  int:         INT_MIN,   \
  long:        LONG_MIN,  \
  uchar:       0,         \
  ushort:      0,         \
  uint:        0u,        \
  ulong:       0ul,       \
  ullong:      0ull,      \
  char:        CHAR_MIN,  \
  bool:        1,         \
  float:       FLT_MIN,   \
  double:      DBL_MIN,   \
  long double: LDBL_MIN   \
)

#endif

#if !defined(MAXOF)

#define MAXOF(VARIABLE) \
_Generic((VARIABLE), \
  signed char: SCHAR_MAX,  \
  short:       SHRT_MAX,   \
  int:         INT_MAX,    \
  long:        LONG_MAX,   \
  uchar:       UCHAR_MAX,  \
  ushort:      USHRT_MAX,  \
  uint:        UINT_MAX,   \
  ulong:       ULONG_MAX,  \
  ullong:      ULLONG_MAX, \
  char:        CHAR_MAX,   \
  bool:        1,          \
  float:       FLT_MAX,    \
  double:      DBL_MAX,    \
  long double: LDBL_MAX    \
)
#endif

// clang-format on

#define error2(...) fprintf(stderr, __VA_ARGS__)

static void
assert_strings_equal(char *file, uint32 line, char *name1, char *name2,
                     char *var1, char *var2) {
    if (strcmp(var1, var2)) {
        error2("Assertion failed at %s:%u\n", file, line);
        error2("%s = %s != %s = %s\n", name1, var1, var2, name2);
        trap();
    }
}

static void
assert_strings_not_equal(char *file, uint32 line, char *name1, char *name2,
                         char *var1, char *var2) {
    if (!strcmp(var1, var2)) {
        error2("Assertion failed at %s:%u\n", file, line);
        error2("%s = %s == %s = %s\n", name1, var1, var2, name2);
        trap();
    }
}

static void
assert_strings_less(char *file, uint32 line, char *name1, char *name2,
                    char *var1, char *var2) {
    if (strcmp(var1, var2) >= 0) {
        error2("Assertion failed at %s:%u\n", file, line);
        error2("%s = %s >= %s = %s\n", name1, var1, var2, name2);
        trap();
    }
}

static void
assert_strings_less_equal(char *file, uint32 line, char *name1, char *name2,
                          char *var1, char *var2) {
    if (strcmp(var1, var2) > 0) {
        error2("Assertion failed at %s:%u\n", file, line);
        error2("%s = %s > %s = %s\n", name1, var1, var2, name2);
        trap();
    }
}

#define INTEGER_LESS(TYPE, FORMAT) \
    static void \
    assert_##TYPE##_less(char *file, uint32 line, \
                               char *name1, char *name2, \
                               TYPE long long var1, TYPE long long var2) { \
    if (var1 >= var2) { \
        error2("Assertion failed at %s:%u\n", file, line); \
        error2("%s = "FORMAT" >= "FORMAT" = %s\n", name1, var1, var2, name2); \
        trap(); \
    } \
}

#define INTEGER_LESS_EQUAL(TYPE, FORMAT) \
    static void \
    assert_##TYPE##_less_equal(char *file, uint32 line, \
                               char *name1, char *name2, \
                               TYPE long long var1, TYPE long long var2) { \
    if (var1 > var2) { \
        error2("Assertion failed at %s:%u\n", file, line); \
        error2("%s = "FORMAT" > "FORMAT" = %s\n", name1, var1, var2, name2); \
        trap(); \
    } \
}

#define INTEGER_EQUAL(TYPE, FORMAT) \
    static void \
    assert_##TYPE##_equal(char *file, uint32 line, \
                             char *name1, char *name2, \
                             TYPE long long var1, TYPE long long var2) { \
    if (var1 != var2) { \
        error2("Assertion failed at %s:%u\n", file, line); \
        error2("%s = "FORMAT" != "FORMAT" = %s\n", name1, var1, var2, name2); \
        trap(); \
    } \
}

#define INTEGER_NOT_EQUAL(TYPE, FORMAT) \
    static void \
    assert_##TYPE##_not_equal(char *file, uint32 line, \
                              char *name1, char *name2, \
                              TYPE long long var1, TYPE long long var2) { \
    if (var1 == var2) { \
        error2("Assertion failed at %s:%u\n", file, line); \
        error2("%s = "FORMAT" == "FORMAT" = %s\n", name1, var1, var2, name2); \
        trap(); \
    } \
}

INTEGER_LESS(signed, "%lld")
INTEGER_LESS(unsigned, "%llu")
INTEGER_LESS_EQUAL(signed, "%lld")
INTEGER_LESS_EQUAL(unsigned, "%llu")
INTEGER_EQUAL(signed, "%lld")
INTEGER_EQUAL(unsigned, "%llu")
INTEGER_NOT_EQUAL(signed, "%lld")
INTEGER_NOT_EQUAL(unsigned, "%llu")

static int
integer_un_si(ullong u, llong s) {
    ullong saux;
    if (s < 0) {
        return 1;
    }
    saux = (ullong)s;
    if (saux < u) {
        return 1;
    } else if (saux == u) {
        return 0;
    } else {
        return -1;
    }
}

static void
assert_un_si_equal(char *file, uint line, char *name1, char *name2, ullong var1,
                   llong var2) {
    if (integer_un_si(var1, var2) != 0) {
        error2("%s Assertion failed at %s:%u\n", __func__, file, line);
        error2("%s = %llu != %lld = %s\n", name1, var1, var2, name2);
        trap();
    }
    return;
}

static void
assert_si_un_equal(char *file, uint line, char *name1, char *name2, llong var1,
                   ullong var2) {
    if (integer_un_si(var2, var1) != 0) {
        error2("%s Assertion failed at %s:%u\n", __func__, file, line);
        error2("%s = %lld != %llu = %s\n", name1, var1, var2, name2);
        trap();
    }
    return;
}

static void
assert_un_si_not_equal(char *file, uint line, char *name1, char *name2,
                       ullong var1, llong var2) {
    if (integer_un_si(var1, var2) == 0) {
        error2("%s Assertion failed at %s:%u\n", __func__, file, line);
        error2("%s = %llu == %lld = %s\n", name1, var1, var2, name2);
        trap();
    }
    return;
}

static void
assert_si_un_not_equal(char *file, uint line, char *name1, char *name2,
                       llong var1, ullong var2) {
    if (integer_un_si(var2, var1) == 0) {
        error2("%s Assertion failed at %s:%u\n", __func__, file, line);
        error2("%s = %lld == %llu = %s\n", name1, var1, var2, name2);
        trap();
    }
    return;
}

static void
assert_un_si_less(char *file, uint line, char *name1, char *name2, ullong var1,
                  llong var2) {
    if (integer_un_si(var1, var2) >= 0) {
        error2("%s Assertion failed at %s:%u\n", __func__, file, line);
        error2("%s = %llu <= %lld = %s\n", name1, var1, var2, name2);
        trap();
    }
    return;
}

static void
assert_un_si_less_equal(char *file, uint line, char *name1, char *name2,
                        ullong var1, llong var2) {
    if (integer_un_si(var1, var2) > 0) {
        error2("Assertion failed at %s:%u\n", file, line);
        error2("%s = %llu < %lld = %s\n", name1, var1, var2, name2);
        trap();
    }
    return;
}

static void
assert_si_un_less(char *file, uint line, char *name1, char *name2, llong var1,
                  ullong var2) {
    if (integer_un_si(var2, var1) <= 0) {
        error2("%s Assertion failed at %s:%u\n", __func__, file, line);
        error2("%s = %lld >= %llu = %s\n", name1, var1, var2, name2);
        trap();
    }
    return;
}

static void
assert_si_un_less_equal(char *file, uint line, char *name1, char *name2,
                        llong var1, ullong var2) {
    if (integer_un_si(var2, var1) < 0) {
        error2("%s Assertion failed at %s:%u\n", __func__, file, line);
        error2("%s = %lld > %llu = %s\n", name1, var1, var2, name2);
        trap();
    }
    return;
}

#define COMPARE_SIGNED(VAR1, VAR2, MODE) \
_Generic((VAR2), \
  char: assert_signed_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                             (llong)(VAR1), (llong)(VAR2)), \
  short: assert_signed_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                             (llong)(VAR1), (llong)(VAR2)), \
  int: assert_signed_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                           (llong)(VAR1), (llong)(VAR2)), \
  long: assert_signed_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                            (llong)(VAR1), (llong)(VAR2)), \
  llong: assert_signed_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                            (llong)(VAR1), (llong)(VAR2)), \
  ushort: assert_si_un_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                              (llong)(VAR1), (ullong)(VAR2)), \
  uint: assert_si_un_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                            (llong)(VAR1), (ullong)(VAR2)), \
  ulong: assert_si_un_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                             (llong)(VAR1), (ullong)(VAR2)), \
  ullong: assert_si_un_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                              (llong)(VAR1), (ullong)(VAR2)), \
  default: assert(false) \
)

#define COMPARE_UNSIGNED(VAR1, VAR2, MODE) \
_Generic((VAR2), \
  ushort: assert_unsigned_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                                (ullong)(VAR1), (ullong)(VAR2)), \
  uint: assert_unsigned_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                              (ullong)(VAR1), (ullong)(VAR2)), \
  ulong: assert_unsigned_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                               (ullong)(VAR1), (ullong)(VAR2)), \
  ullong: assert_unsigned_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                                (ullong)(VAR1), (ullong)(VAR2)), \
  short: assert_un_si_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                             (ullong)(VAR1), (llong)(VAR2)), \
  int: assert_un_si_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                           (ullong)(VAR1), (llong)(VAR2)), \
  long: assert_un_si_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                            (ullong)(VAR1), (llong)(VAR2)), \
  llong: assert_un_si_##MODE(__FILE__, __LINE__, #VAR1, #VAR2, \
                             (ullong)(VAR1), (llong)(VAR2)), \
  default: assert(false) \
)

#define ASSERT_COMPARE(WHAT, VAR1, VAR2) \
_Generic((VAR1), \
  char *: _Generic((VAR2), \
    char *: assert_strings_##WHAT(__FILE__, __LINE__, #VAR1, #VAR2, \
                                 (char *)(uintptr_t)(VAR1), \
                                 (char *)(uintptr_t)(VAR2)), \
    default: assert(false) \
  ), \
  short: COMPARE_SIGNED(VAR1, VAR2, WHAT), \
  int: COMPARE_SIGNED(VAR1, VAR2, WHAT), \
  long: COMPARE_SIGNED(VAR1, VAR2, WHAT), \
  llong: COMPARE_SIGNED(VAR1, VAR2, WHAT), \
  ushort: COMPARE_UNSIGNED(VAR1, VAR2, WHAT), \
  uint: COMPARE_UNSIGNED(VAR1, VAR2, WHAT), \
  ulong: COMPARE_UNSIGNED(VAR1, VAR2, WHAT), \
  ullong: COMPARE_UNSIGNED(VAR1, VAR2, WHAT) \
)

#define ASSERT_EQUAL(VAR1, VAR2) ASSERT_COMPARE(equal, VAR1, VAR2)
#define ASSERT_NOT_EQUAL(VAR1, VAR2) ASSERT_COMPARE(not_equal, VAR1, VAR2)
#define ASSERT_LESS(VAR1, VAR2) ASSERT_COMPARE(less, VAR1, VAR2)
#define ASSERT_LESS_EQUAL(VAR1, VAR2) ASSERT_COMPARE(less_equal, VAR1, VAR2)

#if TESTING_assert
int
main(void) {
    int a = 0;
    int b = 1;
    int c = 2;
    uint d = 2;
    uint e = 3;
    int f = -1;
    long g = -10;
    long h = 10;
    long g4 = MINOF(g4);
    long g2 = MAXOF(g2);
    ulong g3 = MAXOF(g3);
    ulong g8 = 0;
    char *s1 = "aaa";
    char *s2 = "aaa";
    char *s3 = "BBB";

    ASSERT_EQUAL(a, a);
    ASSERT_LESS(b, c);
    ASSERT_LESS_EQUAL(a, b);

    ASSERT_EQUAL(c, d);
    ASSERT_LESS_EQUAL(c, d);
    ASSERT_LESS(f, e);
    ASSERT_LESS(f, g8);
    ASSERT_LESS_EQUAL(f, e);
    ASSERT_LESS_EQUAL(d, c);
    ASSERT_LESS(g, h);
    ASSERT_LESS_EQUAL(g, h);
    ASSERT_LESS(g2, g3);
    ASSERT_LESS(g4, g3);
    ASSERT_LESS(g4, g2);
    ASSERT_LESS(g4, g8);
    ASSERT_LESS(g8, g2);
    ASSERT_EQUAL(s1, s2);
    ASSERT_NOT_EQUAL(a, b);
    ASSERT_NOT_EQUAL(s1, s3);
}
#endif

#endif
