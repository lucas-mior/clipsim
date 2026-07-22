#include <stdio.h>
#include <stdlib.h>

#include "primitives.h"
#include "base_macros.h"
#include "util.h"
#include "generic.c"

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define RW_FUNCTION write
#endif

#if !defined(RW_FUNCTION)
#error "RW_FUNCTION is not defined"
#endif

#if !defined(RW_TYPE)
#error "RW_TYPE is not defined"
#endif

static int64
CAT(f, RW_FUNCTION, 64)(void *buffer, int64 size, int64 n, FILE *file) {
    size_t rw;

    if (size <= 0) {
        error("Error: Invalid size = %lld\n", (llong)size);
        fatal(EXIT_FAILURE);
    }
    if (n <= 0) {
        error("Error: Invalid n = %lld\n", (llong)n);
        fatal(EXIT_FAILURE);
    }
    if (size >= (MAXOF(size)/n)) {
        error("Error: Overflow (%lld*%lld)\n", (llong)size, (llong)n);
        fatal(EXIT_FAILURE);
    }
    if ((size_t)size >= MAXOF(rw)) {
        error("Error: size (%lld) is bigger than SIZEMAX\n", (llong)size);
        fatal(EXIT_FAILURE);
    }
    if ((size_t)n >= MAXOF(rw)) {
        error("Error: n (%lld) is bigger than SIZEMAX\n", (llong)n);
        fatal(EXIT_FAILURE);
    }

    rw = CAT(f, RW_FUNCTION)(buffer, (size_t)size, (size_t)n, file);
    return (int64)rw;
}

static int64
CAT(RW_FUNCTION, 64)(int fd, void *buffer, int64 size) {
    RW_TYPE instance = 0;
    ssize_t w;

    (void)instance;

    if (size == 0) {
        return 0;
    }
    if (size < 0) {
        error("Error: Invalid size = %lld\n", (llong)size);
        fatal(EXIT_FAILURE);
    }
    if ((ullong)size >= (ullong)MAXOF(instance)) {
        error("Error: size (%lld) is too big for %s\n",
              (llong)size, QUOTE(RW_FUNCTION));
        fatal(EXIT_FAILURE);
    }

    w = RW_FUNCTION(fd, buffer, (RW_TYPE)size);
    return (int64)w;
}

#undef RW_FUNCTION
