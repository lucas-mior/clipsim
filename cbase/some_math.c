#if !defined(SOME_MATH_C)
#define  SOME_MATH_C

#include "cbase.h"

int64
ceil64(double x) {
    double c = ceil(x);
    if (DEBUGGING) {
        if (c <= (double)MINOF(ceil64(0))) {
            error("%f does not fit in int64.\n", c);
            fatal(EXIT_FAILURE);
        }
        if (c >= (double)MAXOF(ceil64(0))) {
            error("%f does not fit in int64.\n", c);
            fatal(EXIT_FAILURE);
        }
    }
    return (int64)c;
}

int64
floor64(double x) {
    double f = floor(x);
    if (DEBUGGING) {
        if (f <= (double)MINOF(floor64(0))) {
            error("%f does not fit in int64.\n", f);
            fatal(EXIT_FAILURE);
        }
        if (f >= (double)MAXOF(floor64(0))) {
            error("%f does not fit in int64.\n", f);
            fatal(EXIT_FAILURE);
        }
    }
    return (int64)f;
}

#endif
