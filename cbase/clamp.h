#include "base_macros.h"
#include "primitives.h"

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define CLAMP_TYPE double
#endif

#if !defined(CLAMP_TYPE)
#error "Error: CLAMP_TYPE not defined"
#endif

static CLAMP_TYPE
CAT(clamp_, CLAMP_TYPE)(CLAMP_TYPE var, CLAMP_TYPE min, CLAMP_TYPE max) {
    if (var < min) {
        return min;
    }
    if (var > max) {
        return max;
    }
    return var;
}

#undef CLAMP_TYPE
