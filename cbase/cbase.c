// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_cbase 1
#elif !defined(TESTING_cbase)
#define TESTING_cbase 0
#endif

#if TESTING_cbase
#define CBASE_IMPLEMENT
#include "cbase.h"

int
main(void) {
	ASSERT(true);
	exit(EXIT_SUCCESS);
}

#endif /* TESTING_cbase */
