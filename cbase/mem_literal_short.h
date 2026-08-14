// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#include "base_macros.h"
#include "primitives.h"

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
  #define MEM_LITERAL_SHORT_N 2
#endif

#if !defined(MEM_LITERAL_SHORT_N)
#error "MEM_LITERAL_SHORT_N is undefined"
#endif

#if (MEM_LITERAL_SHORT_N < 2) || (MEM_LITERAL_SHORT_N > 15)
#error "MEM_LITERAL_SHORT_N must be between 2 and 15"
#endif

#define MEM_LITERAL_SHORT_FUNCTION \
    CAT(mem_literal_short_, MEM_LITERAL_SHORT_N)

#define MEM_LITERAL_SHORT_MATCH(P, L) \
    (memcmp((P) + 1, (L) + 1, MEM_LITERAL_SHORT_N - 1) == 0)

INLINE UNUSED char *
MEM_LITERAL_SHORT_FUNCTION(char *haystack, int64 haystack_len,
                           char *literal, int64 literal_len) {
    char *candidate;
    char *end;

    (void)literal_len;

    if (haystack_len < MEM_LITERAL_SHORT_N) {
        return NULL;
    }
    if ((haystack == NULL) || (literal == NULL)) {
        return NULL;
    }

    candidate = haystack;
    end = haystack + haystack_len - MEM_LITERAL_SHORT_N + 1;
    while (candidate < end) {
        char *p = memchr(candidate, literal[0], end - candidate);

        if (p == NULL) {
            return NULL;
        }
        if (MEM_LITERAL_SHORT_MATCH(p, literal)) {
            return p;
        }
        candidate = p + 1;
    }

    return NULL;
}

#undef MEM_LITERAL_SHORT_MATCH
#undef MEM_LITERAL_SHORT_FUNCTION
#undef MEM_LITERAL_SHORT_N
