// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#include "base_macros.h"
#include "primitives.h"

#if !defined(TESTING_sfa)
#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_sfa 1
#else
#define TESTING_sfa 0
#endif
#endif

#if 1 == TESTING_sfa
#define SFA_NAME strings
#define SFA_TYPE void *
#define SFA_FORMAT "%p"
#endif

#if !defined(SFA_NAME)
#error "Error: SFA_NAME not defined"
#endif

#if !defined(SFA_TYPE)
#error "Error: SFA_TYPE not defined"
#endif

#if !defined(SFA_FORMAT)
#error "Error: SFA_FORMAT not defined"
#endif

#if !defined(SFA_LINKAGE)
#define SFA_LINKAGE static
#endif


SFA_LINKAGE int32
CAT(string_from_, SFA_NAME)(char *buffer, int32 size,
                            char *sep, SFA_TYPE *array, int32 array_length) {
    int32 n = 0;
    if (array_length <= 0) {
        buffer[0] = '\0';
        return 0;
    }
    for (int32 i = 0; i < (array_length - 1); i += 1) {
        int32 space = size - n;
        int32 m = fmt_sprintf(buffer + n, space, SFA_FORMAT "%s", array[i], sep);
        n += m;
    }
    {
        int32 i = array_length - 1;
        int32 space = size - n;
        n += fmt_sprintf(buffer + n, space, SFA_FORMAT, array[i]);
    }
    return n;
}

#undef SFA_TYPE
#undef SFA_NAME
#undef SFA_FORMAT
#undef SFA_LINKAGE
