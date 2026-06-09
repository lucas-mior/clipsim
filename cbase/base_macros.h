#if !defined(BASE_MACROS_H)
#define BASE_MACROS_H

#include "platform_detection.h"

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)

#define STRLIT_LEN(s) ((int32)(sizeof(s) - 1))
#define STRLIT_ARGS(s) s, STRLIT_LEN(s)

#define CAT_(a, b) a ## b
#define CAT_SELECT(a, b) CAT_(a, b)

#define CAT1_(a) a
#define CAT2_(a, b) a ## b
#define CAT3_(a, b, c) a ## b ## c
#define CAT4_(a, b, c, d) a ## b ## c ## d
#define CAT5_(a, b, c, d, e) a ## b ## c ## d ## e
#define CAT6_(a, b, c, d, e, f) a ## b ## c ## d ## e ## f
#define CAT7_(a, b, c, d, e, f, g) a ## b ## c ## d ## e ## f ## g
#define CAT8_(a, b, c, d, e, f, g, h) a ## b ## c ## d ## e ## f ## g ## h
#define CAT9_(a, b, c, d, e, f, g, h, i) a ## b ## c ## d ## e ## f ## g ## h ## i

#define CAT1(a) CAT1_(a)
#define CAT2(a, b) CAT2_(a, b)
#define CAT3(a, b, c) CAT3_(a, b, c)
#define CAT4(a, b, c, d) CAT4_(a, b, c, d)
#define CAT5(a, b, c, d, e) CAT5_(a, b, c, d, e)
#define CAT6(a, b, c, d, e, f) CAT6_(a, b, c, d, e, f)
#define CAT7(a, b, c, d, e, f, g) CAT7_(a, b, c, d, e, f, g)
#define CAT8(a, b, c, d, e, f, g, h) CAT8_(a, b, c, d, e, f, g, h)
#define CAT9(a, b, c, d, e, f, g, h, i) CAT9_(a, b, c, d, e, f, g, h, i)

#define NUM_ARGS_(_1, _2, _3, _4, _5, _6, _7, _8, _9, n, ...) n
#define NUM_ARGS(...) NUM_ARGS_(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, x)
#define SELECT_ON_NUM_ARGS(macro, ...) \
  CAT_SELECT(macro, NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)

#define CAT_SELECT_ON_NUM_ARGS(macro, ...) \
  CAT_SELECT(macro, NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)
#define CAT(...) CAT_SELECT_ON_NUM_ARGS(CAT, __VA_ARGS__)

#include <stddef.h>
#if !defined(offsetof)
#define OFFSET_OF(STRUCT, FIELD) ((size_t)&(((STRUCT *)0)->FIELD))
#else
#define OFFSET_OF(STRUCT, FIELD) offsetof(STRUCT, FIELD)
#endif

#define RESET     "\x1b[0m"

#define RED(S)    "\x1b[0;31m"  S RESET
#define GREEN(S)  "\x1b[0;32m"  S RESET
#define YELLOW(S) "\x1b[0;33m"  S RESET
#define BLUE(S)   "\x1b[0;34m"  S RESET
#define CYAN(S)   "\x1b[0;35m"  S RESET
#define PURPLE(S) "\x1b[0;36m"  S RESET

#define BRED(S)    "\x1b[1;31m" S RESET
#define BGREEN(S)  "\x1b[1;32m" S RESET
#define BYELLOW(S) "\x1b[1;33m" S RESET
#define BBLUE(S)   "\x1b[1;34m" S RESET
#define BCYAN(S)   "\x1b[1;35m" S RESET

#define SIZEKB(X) ((int64)(X)*1024ll)
#define SIZEMB(X) ((int64)(X)*1024ll*1024ll)
#define SIZEGB(X) ((int64)(X)*1024ll*1024ll*1024ll)

#if defined(SIZEOF)
#undef SIZEOF
#endif
#define SIZEOF(X) ((int64)sizeof(X))
#define LENGTH(x) (int64)((sizeof(x) / sizeof(*x)))
#define SWAP(x, y) do { __typeof__(x) SWAP = x; x = y; y = SWAP; } while (0)

#define ALIGN_POWER_OF_2_(SIZE, A) (int64)(((SIZE) + ((A) - 1)) & ~((A) - 1))
#define ALIGN16(x) (((x) + 15) & ~15)

#define BETWEEN(x, a, b)    ((a) <= (x) && (x) <= (b))

#define ALIGN_POWER_OF_2(SIZE, A) \
_Generic((SIZE), \
    ullong: ALIGN_POWER_OF_2_((ullong)SIZE, (ullong)A), \
    ulong:  ALIGN_POWER_OF_2_((ulong)SIZE,  (ulong)A),  \
    uint:   ALIGN_POWER_OF_2_((uint)SIZE,   (uint)A),   \
    llong:  ALIGN_POWER_OF_2_((ullong)SIZE, (ullong)A), \
    long:   ALIGN_POWER_OF_2_((ulong)SIZE,  (ulong)A),  \
    int:    ALIGN_POWER_OF_2_((uint)SIZE,   (uint)A)    \
)

#define ALIGNMENT 16ul
#if defined(ALIGN)
#undef ALIGN
#endif
#define ALIGN(x) ALIGN_POWER_OF_2(x, ALIGNMENT)

#if defined(__GNUC__)
#define ASSUME_ALIGNED(X) do { \
    X = __builtin_assume_aligned(X, ALIGNMENT); \
} while (0)
#else
#define ASSUME_ALIGNED(X) do {} while (0)
#endif

#if !defined(DEBUGGING)
#define DEBUGGING 0
#endif

#if DEBUGGING
  #define INLINE static
#else
  #if defined(__GNUC__)
    #define INLINE static inline __attribute__((always_inline))
  #else
    #define INLINE static inline
  #endif
#endif

#if !defined(__CPROC__) && defined(__has_include)
  #if __has_include(<valgrind/valgrind.h>)
    #include <valgrind/valgrind.h>
  #else
    #define RUNNING_ON_VALGRIND 0
  #endif
#else
    #define RUNNING_ON_VALGRIND 0
#endif

#if !defined(FLAGS_HUGE_PAGES)
#if defined(MAP_HUGETLB) && defined(MAP_HUGE_2MB)
#define FLAGS_HUGE_PAGES MAP_HUGETLB | MAP_HUGE_2MB
#else
#define FLAGS_HUGE_PAGES 0
#endif
#endif

#if !defined(MAP_POPULATE)
#define MAP_POPULATE 0
#endif

#if defined(__GNUC__) || defined(__clang__)
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#define π 3.14159265358979323846264338327950288

#endif /* BASE_MACROS_H */
