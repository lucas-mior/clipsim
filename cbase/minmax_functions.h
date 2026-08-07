#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define MINMAX_TYPE int
#endif

#if !defined(MINMAX_TYPE)
#error "MINMAX_TYPE not defined"
#endif

static int CAT(min_, MINMAX_TYPE, _or_smaller(int a, int b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
}

static int max_int_or_smaller(int a, int b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
}

#endif
