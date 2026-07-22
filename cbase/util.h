#if !defined(UTIL_H)
#define UTIL_H

#include <stdbool.h>
#include <time.h>
#include "primitives.h"

static void __attribute__((format(printf, 4, 5)))
error_impl(char *file, int32 line, char *func, char *format, ...);
#define error(...) error_impl(__FILE__, __LINE__, (char *)__func__, __VA_ARGS__)
static int32 snprintf2(char *buffer, int64 size, char *format, ...);

static int32 random_ascii_string(char *buffer, int32 capacity, int32 min_len);

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

static void *xmalloc(int64 size, bool zero);

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

typedef struct StrBuilder {
    char *data;
    int32 len;
    int32 cap;
} StrBuilder;

typedef struct StrBuilderArray {
    StrBuilder *items;
    int32 len;
    int32 cap;
} StrBuilderArray;

static void sb_init(StrBuilder *str_builder);
static void sb_free(StrBuilder *str_builder);
static void sb_clear(StrBuilder *str_builder);
static bool sb_copy(StrBuilder *dest, StrBuilder *source);
static void sb_move(StrBuilder *dest, StrBuilder *source);
static bool sb_set(StrBuilder *str_builder, char *data, int32 data_len);
static void sb_reserve(StrBuilder *str_builder, int32 extra);
static void sb_append(StrBuilder *str_builder, char *data, int32 data_len);
static void sb_append_byte(StrBuilder *str_builder, char byte);
static void sb_printf(StrBuilder *str_builder, char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
static char *sb_steal(StrBuilder *str_builder, int32 *len, int32 *cap);
static char *sb_steal_exact(StrBuilder *str_builder, int32 *len);

static void str_builder_array_init(StrBuilderArray *array);
static void str_builder_array_clear(StrBuilderArray *array);
static void str_builder_array_destroy(StrBuilderArray *array);
static bool str_builder_array_copy(StrBuilderArray *dest,
                                   StrBuilderArray *source);
static void str_builder_array_move(StrBuilderArray *dest,
                                   StrBuilderArray *source);
static void str_builder_array_swap(StrBuilderArray *left,
                                   StrBuilderArray *right);
static bool str_builder_array_reserve(StrBuilderArray *array, int32 extra);
static StrBuilder *str_builder_array_append(StrBuilderArray *array);
static bool str_builder_array_append_copy(StrBuilderArray *array,
                                          StrBuilder *item);

static void *memmem64(void *haystack, int64 hay_len,
                      void *needle, int64 needle_len);

#endif /* UTIL_H */
