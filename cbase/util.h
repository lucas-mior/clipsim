#if !defined(UTIL_H)
#define UTIL_H

#include "primitives.h"

static void __attribute__((format(printf, 4, 5)))
    error_impl(char *file, int32 line, char *func, char *format, ...);
#define error(...) error_impl(__FILE__, __LINE__, (char *)__func__, __VA_ARGS__)
static int32 snprintf2(char *buffer, int64 size, char *format, ...);

static void fatal(int status);

static void memset64(void *buffer, int value, int64 size);
static int32 strlen32(char *string);
static char *xstrdup(char *string);
static void *xmmap_commit(int64 *size);
static void xmunmap(void *p, int64 size);
static int memcmp64(void *left, void *right, int64 size);
static void
print_timings(char *file, int32 line, char *func,
              int64 nitems, struct timespec t0, struct timespec t1);

static void *xmalloc(int64 size);

#define SNPRINTF(BUFFER, FORMAT, ...) \
    snprintf2(BUFFER, sizeof(BUFFER), FORMAT, __VA_ARGS__)
#define STRFTIME(BUFFER, FORMAT, TIME) \
    strftime2(BUFFER, sizeof(BUFFER), FORMAT, TIME)

#define STRUCT_ARRAY_SIZE(struct_object, ArrayType, array_length) \
    (int64)(SIZEOF(*(struct_object)) + ((array_length)*SIZEOF(ArrayType)))

static void memcpy64(void *dest, void *source, int64 n);
static void memmove64(void *dest, void *source, int64 n);

static ullong here_counter = 0;

#define HERE do { \
    char HEREbuffer[4096]; \
    fprintf(stderr, "\n===== HERE(%llu): %s:%d (%s)\n", \
                    here_counter++, __FILE__, __LINE__, __func__); \
    SNPRINTF(HEREbuffer, "%s:%d:%s\n", __FILE__, __LINE__, __func__); \
    switch (fork()) { \
    case -1: \
        error("Error forking: %s.\n", strerror(errno)); \
        fatal(EXIT_FAILURE); \
    case 0: \
        execlp("dunstify", "dunstify", program, HEREbuffer, NULL); \
        error("Error executing dunstify: %s.\n", strerror(errno)); \
        exit(EXIT_FAILURE); \
    default: \
        break; \
    } \
} while (0)

#define NCALLS(INTERVAL) do { \
    static int64 ncalls_ncalls = 1; \
    if ((ncalls_ncalls % INTERVAL) == 0) { \
        fprintf(stderr, "%s:%d:%s: called %lld times\n", \
                        __FILE__, __LINE__, __func__, (llong)ncalls_ncalls); \
    } \
    ncalls_ncalls += 1; \
} while (0)

#define MEM_FREED 0xDC
#define MEM_MALLOCED_UNINITIALIZED 0xCD
#define MEM_DONT_READ 0xBD

#define PRINT_TIMINGS_3(N, T0, T1) \
        print_timings(__FILE__, __LINE__, (char *)__func__, N, T0, T1)
#define PRINT_TIMINGS_4(N, T0, T1, NAME) \
        print_timings(__FILE__, __LINE__, NAME, N, T0, T1)
#define PRINT_TIMINGS(...) SELECT_ON_NUM_ARGS(PRINT_TIMINGS_, __VA_ARGS__)

#endif /* UTIL_H */
