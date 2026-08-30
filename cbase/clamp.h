// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#include "platform_detection.h"
#include "warnings.h"
#include "base_macros.h"
#include "primitives.h"

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define CLAMP_TYPE double
#endif

#if !defined(CLAMP_TYPE)
#error "Error: CLAMP_TYPE not defined"
#endif

CLAMP_TYPE
CAT(clamp_, CLAMP_TYPE)(CLAMP_TYPE var, CLAMP_TYPE min, CLAMP_TYPE max) {
    if (var < min) {
        return min;
    }
    if (var > max) {
        return max;
    }
    return var;
}

CLAMP_TYPE
CAT(square_, CLAMP_TYPE)(CLAMP_TYPE var) {
    return var*var;
}

static inline void
CAT(CLAMP_TYPE, _sink)(void) {
    (void)CAT(square_, CLAMP_TYPE);
    (void)CAT(clamp_, CLAMP_TYPE);
}

#undef CLAMP_TYPE
