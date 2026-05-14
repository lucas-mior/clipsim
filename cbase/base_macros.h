#if !defined(BASE_MACROS_H)
#define BASE_MACROS_H

#include "platform_detection.h"

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)

#define CAT_(a, b)     a##b
#define CAT3_(a, b, c) a##b##c
#define CAT(a, b)      CAT_(a, b)
#define CAT3(a, b, c)  CAT3_(a, b, c)

#define NUM_ARGS_(_1, _2, _3, _4, _5, _6, _7, _8, n, ...) n
#define NUM_ARGS(...) NUM_ARGS_(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1, x)
#define SELECT_ON_NUM_ARGS(macro, ...) \
  CAT(macro, NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)

#include <stddef.h>
#if !defined(offsetof)
#define OFFSET_OF(STRUCT, FIELD) ((size_t)&(((STRUCT *)0)->FIELD))
#else
#define OFFSET_OF(STRUCT, FIELD) offsetof(STRUCT, FIELD)
#endif

#define RESET     "\x1b[0m"
#define RED(S)    "\x1b[31m"   S RESET
#define GREEN(S)  "\x1b[32m"   S RESET
#define YELLOW(S) "\x1b[33m"   S RESET
#define BLUE(S)   "\x1b[1;34m" S RESET

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

#endif /* BASE_MACROS_H */
