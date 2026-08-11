#if !defined(TRY_INCLUDE_WHICH)
#define TRY_INCLUDE_WHICH <stdio.h>
#endif

#if defined(__has_include)
#define HAS_INCLUDE(header) __has_include(header)
#else
#define HAS_INCLUDE(header) 1
#endif

#if HAS_INCLUDE(TRY_INCLUDE_WHICH)
#include TRY_INCLUDE_WHICH
#endif

#undef TRY_INCLUDE_WHICH
