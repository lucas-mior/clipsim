// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_cbase 1
#elif !defined(TESTING_cbase)
#define TESTING_cbase 0
#endif

// We could simply cc -DCBASE_IMPLEMENT cbase.h -o cbase.o,
// but then we would have to pass -x c flag to clang and gcc
// to inform we don't want a pre compiled header.
// -x c breaks tcc (not to mention other compilers).
// So we use this cbase.c file to avoid complications

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
