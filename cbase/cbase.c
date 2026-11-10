// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(TESTING_cbase)
#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_cbase 1
#else
#define TESTING_cbase 0
#endif
#endif

// This wrapper builds cbase as C without compiler-specific -x options.

#define CBASE_IMPLEMENT
#include "cbase.h"

#if TESTING_cbase
#define CBASE_IMPLEMENT
#include "cbase.h"

int
main(void) {
    ASSERT(true);
    exit(EXIT_SUCCESS);
}

#endif /* TESTING_cbase */
