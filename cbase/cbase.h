// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(CBASE_H)
#define CBASE_H

#if !defined(TESTING_array)
#define TESTING_array 0
#endif
#if !defined(TESTING_arena)
#define TESTING_arena 0
#endif
#if !defined(TESTING_assert)
#define TESTING_assert 0
#endif
#if !defined(TESTING_command)
#define TESTING_command 0
#endif
#if !defined(TESTING_generic)
#define TESTING_generic 0
#endif
#if !defined(TESTING_hash)
#define TESTING_hash 0
#endif
#if !defined(TESTING_memory)
#define TESTING_memory 0
#endif
#if !defined(TESTING_meta_common)
#define TESTING_meta_common 0
#endif
#if !defined(TESTING_meta_generate)
#define TESTING_meta_generate 0
#endif
#if !defined(TESTING_meta_parse)
#define TESTING_meta_parse 0
#endif
#if !defined(TESTING_meta_tokenize)
#define TESTING_meta_tokenize 0
#endif
#if !defined(TESTING_minmax)
#define TESTING_minmax 0
#endif
#if !defined(TESTING_sort)
#define TESTING_sort 0
#endif
#if !defined(TESTING_utf8)
#define TESTING_utf8 0
#endif
#if !defined(TESTING_util)
#define TESTING_util 0
#endif
#if !defined(TESTING_xenums)
#define TESTING_xenums 0
#endif


#if !defined(DEBUGGING)
#define DEBUGGING 0
#endif

#if !defined(RELEASING)
#define RELEASING 0
#endif

#if !defined(ERROR_NOTIFY)
#define ERROR_NOTIFY 0
#endif

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "platform_detection.h"
#include "primitives.h"
#include "base_macros.h"
#include "i18n.h"
#include "memory.h"

#include "generic.c"
#include "minmax.c"
#include "assert.c"

#include "arena.h"
#define UTF_INVALID 0xFFFD

static int32 random_utf8_string(char *, int32, int32);
static int32 utf8_byte_position(char *, int32, int32);
static int32 utf8_capitalize_first_letters(char *, int32, char *, int32);
static int32 utf8_char_width(uint32);
static int32 utf8_characters(char *, int32);
static int32 utf8_cut_width(char *, int32, int32);
static int32 utf8_decode(char *, int32, uint32 *);
static uint32 utf8_decode_byte(char, int32 *);
static int32 utf8_decode_raw(char *, uint32 *, int32);
static int32 utf8_encode(uint32, char *, int32);
static char utf8_encode_byte(uint32, int32);
static int32 utf8_encode_raw(uint32, char *);
static void utf8_functions_sink(void);
static int32 utf8_next_position(char *, int32, int32);
static int32 utf8_suffix_width_position(char *, int32, int32);
static int32 utf8_validate(uint32 *, int32);
static int32 utf8_width(char *, int32);

#if !defined(MAX_FILES_COPY)
#define MAX_FILES_COPY 256
#endif

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

#if OS_UNIX
typedef struct UtilCopyFilesAsync {
    struct pollfd pipes[MAX_FILES_COPY];
    int dests[MAX_FILES_COPY];
    int32 nfds;
    int32 unused;
} UtilCopyFilesAsync;

static int32 util_copy_file_async(char *, char *, int *);
static void util_copy_file_async_parsed(UtilCopyFilesAsync *);
static void *util_copy_file_async_thread(void *);
#endif

static void error_impl(char *, int32, char *, char *, ...)
    __attribute__((format(printf, 4, 5)));
static void fatal(int32) __attribute__((noreturn));
static void util_segv_handler(int32) __attribute__((noreturn));
static int32 itoa2(char *, int32, llong);
static long atoi2(char *);
static char *basename2(char *, int32 *, int32 *);
static char *begins_with(char *, int32, char *, int32);
static int32 bytes_pretty(char *, int64);
static void catfile(int, char *);
static double deg2rad(double);
static int32 dirname2(char *, char *, int32 *);
static char *ends_with(char *, int32, char *, int32);
static void error_async_safe(char *);
static bool is_ident_char(char);
static bool is_ident_start_char(char);
static void *memchr64(void *, int32, int64);
static int memcmp64(void *, void *, int64);
static void *memmem64(void *, int64, void *, int64);
static void *memrchr64(void *, int32, int64);
static void normalize(char *restrict, int32 *restrict);
static int32 optional_strlen32(char *);
static bool parse_option(char **, char *, char *);
static char *path_basename(char *, int32);
static void print_timings(
    char *,
    int32,
    char *,
    int64,
    struct timespec,
    struct timespec
);
static void qsort64(void *, int64, int64, int (*)(void *, void *));
static double rad2deg(double);
static int32 random_ascii_string(char *, int32, int32);
static char *read_entire_file(char *, int32 *);
static char *remove_escape_sequences(char *, int32 *);
static void sb_append(StrBuilder *, char *, int32);
static void sb_append_byte(StrBuilder *, char);
static void sb_clear(StrBuilder *);
static bool sb_copy(StrBuilder *, StrBuilder *);
static void sb_free(StrBuilder *);
static void sb_init(StrBuilder *);
static void sb_move(StrBuilder *, StrBuilder *);
static void sb_printf(StrBuilder *, char *, ...);
static void sb_reserve(StrBuilder *, int32);
static bool sb_set(StrBuilder *, char *, int32);
static char *sb_steal(StrBuilder *, int32 *, int32 *);
static char *sb_steal_exact(StrBuilder *, int32 *);
static void send_signal(char *, int32);
static int32 snprintf2(char *, int64, char *, ...);
static StrBuilder *str_builder_array_append(StrBuilderArray *);
static bool str_builder_array_append_copy(StrBuilderArray *, StrBuilder *);
static void str_builder_array_clear(StrBuilderArray *);
static bool str_builder_array_copy(StrBuilderArray *, StrBuilderArray *);
static void str_builder_array_destroy(StrBuilderArray *);
static void str_builder_array_init(StrBuilderArray *);
static void str_builder_array_move(StrBuilderArray *, StrBuilderArray *);
static bool str_builder_array_reserve(StrBuilderArray *, int32);
static void str_builder_array_swap(StrBuilderArray *, StrBuilderArray *);
static int32 string_from_strings(char *, int32, char *, char **, int32);
static int32 string_from_doubles(char *, int32, char *, double *, int32);
static double clamp_double(double, double, double);
static double square_double(double);
static int64 clamp_int64(int64, int64, int64);
static int64 square_int64(int64);
static bool strequal(char *, char *);
static bool strequal2(char *, int32, char *, int32);
static int64 strftime2(char *, int64, char *, struct tm *);
static int32 strlen32(char *);
static int strncmp32(char *, char *, int64);
static char *strncpy32(char *, char *, int64);
static double timediff(struct timespec, struct timespec);
static void timezone_init(void);
static int util_command(int, char **);
static int util_command_launch(int, char **);
static int32 util_copy_file_sync(char *, char *);
static void util_die_notify(char *, char *, ...);
static bool util_equal_files(char *, char *);
static bool util_file_exists(char *);
static int32 util_filename_from(char *, int64, int);
static void util_functions_sink(void);
static int32 util_nthreads(void);
static int32 util_string_int32(int32 *, char *);
static void warn(char *, ...);
static int64 read64(int32, void *, int64);
static int64 write64(int32, void *, int64);
static int64 fread64(void *, int64, int64, FILE *);
static int64 fwrite64(void *, int64, int64, FILE *);
static void write_all(int, char *, int64);
static bool write_entire_file(char *, char *, int64);
static int xclose(char *, int, int *, char *, char *);
static int xclosedir(DIR *, char *);
static void xdup2(int, int);
static int xfclose(char *, int32, char *, FILE *, char *);
static FILE *xfopen(char *, int32, char *, char *, char *);
static void xkill(pid_t, int);
static void xpipe(int [2]);
static void xpthread_cond_destroy(pthread_cond_t *);
static void xpthread_create(
    pthread_t *,
    pthread_attr_t *,
    void *(*)(void *),
    void *
);
static void xpthread_join(pthread_t *, void **);
static void xpthread_mutex_destroy(pthread_mutex_t *);
static void xpthread_mutex_lock(pthread_mutex_t *);
static void xpthread_mutex_unlock(pthread_mutex_t *);
static int xunlink(char *);
static void here_impl(char *, int32, char *);

#define error(...) \
    error_impl(__FILE__, __LINE__, (char *)__func__, __VA_ARGS__)
#define error2(...) fprintf(stderr, __VA_ARGS__)

#define STRING_FROM_ARRAY(BUFFER, SEP, ARRAY, LENGTH) \
_Generic((ARRAY), \
    double *: string_from_doubles, \
    char **: string_from_strings \
)(BUFFER, SIZEOF(BUFFER), SEP, ARRAY, LENGTH)

#define CLAMP(VAR, VMIN, VMAX) \
_Generic((VAR), \
    float: clamp_double, \
    double: clamp_double, \
    default: clamp_int64 \
)(VAR, VMIN, VMAX)

#define SQUARE(VAR) \
_Generic((VAR), \
    float: square_double, \
    double: square_double, \
    default: square_int64 \
)(VAR)

#define MEMMEM_3(LONG, LONG_LEN, SHORT) \
    memmem64(LONG, LONG_LEN, SHORT, strlen32(SHORT))
#define MEMMEM_4(LONG, LONG_LEN, SHORT, LEN) \
    memmem64(LONG, LONG_LEN, SHORT, LEN)
#define MEMMEM(...) SELECT_ON_NUM_ARGS(MEMMEM_, __VA_ARGS__)

#define BEGINS_WITH_3(STRING, STRING_LEN, PREFIX) \
    begins_with(STRING, STRING_LEN, PREFIX, strlen32(PREFIX))
#define BEGINS_WITH_4(STRING, STRING_LEN, PREFIX, PREFIX_LEN) \
    begins_with(STRING, STRING_LEN, PREFIX, PREFIX_LEN)
#define BEGINS_WITH(...) SELECT_ON_NUM_ARGS(BEGINS_WITH_, __VA_ARGS__)

#define ENDS_WITH_3(STRING, STRING_LEN, SUFFIX) \
    ends_with(STRING, STRING_LEN, SUFFIX, strlen32(SUFFIX))
#define ENDS_WITH_4(STRING, STRING_LEN, SUFFIX, SUFFIX_LEN) \
    ends_with(STRING, STRING_LEN, SUFFIX, SUFFIX_LEN)
#define ENDS_WITH(...) SELECT_ON_NUM_ARGS(ENDS_WITH_, __VA_ARGS__)

#define ITOA(BUFFER, NUM) itoa2(BUFFER, SIZEOF(BUFFER), NUM)

#define SNPRINTF(BUFFER, FORMAT, ...) \
    snprintf2(BUFFER, SIZEOF(BUFFER), FORMAT, __VA_ARGS__)
#define STRFTIME(BUFFER, FORMAT, TIME) \
    strftime2(BUFFER, SIZEOF(BUFFER), FORMAT, TIME)

#define STRUCT_ARRAY_SIZE(STRUCT_OBJECT, ARRAY_TYPE, ARRAY_LENGTH) \
    (SIZEOF(*(STRUCT_OBJECT)) + (ARRAY_LENGTH)*SIZEOF(ARRAY_TYPE))

#define XCLOSE_1(FD) xclose(__FILE__, __LINE__, FD, #FD, NULL)
#define XCLOSE_2(FD, NAME) xclose(__FILE__, __LINE__, FD, #FD, NAME)
#define XCLOSE(...) SELECT_ON_NUM_ARGS(XCLOSE_, __VA_ARGS__)

#define XFOPEN(FILENAME, MODE) \
    xfopen(__FILE__, __LINE__, FUNC__, FILENAME, MODE)
#define XFCLOSE(F, FILENAME) \
    xfclose(__FILE__, __LINE__, FUNC__, F, FILENAME)

#define SB_APPEND_2(BUILDER, STRING) \
    sb_append(BUILDER, STRING, strlen32(STRING))
#define SB_APPEND_3(BUILDER, STRING, LEN) \
    sb_append(BUILDER, STRING, (int32)(LEN))
#define SB_APPEND(...) SELECT_ON_NUM_ARGS(SB_APPEND_, __VA_ARGS__)

#define strequal2_3(A, A_LEN, B) strequal2(A, A_LEN, B, strlen32(B))
#define strequal2_4(A, A_LEN, B, B_LEN) strequal2(A, A_LEN, B, B_LEN)
#define STREQUAL(...) SELECT_ON_NUM_ARGS(strequal2_, __VA_ARGS__)

#define HERE here_impl(__FILE__, __LINE__, (char *)__func__)

#define NCALLS(INTERVAL) do { \
    static int64 ncalls_ncalls = 1; \
    if ((ncalls_ncalls % (INTERVAL)) == 0) { \
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

#define GETENV(VAR) do { \
    if (((VAR) = getenv(#VAR)) == NULL) { \
        if (DEBUGGING) { \
            error_impl(__FILE__, __LINE__, FUNC__, \
                       RED("%s") " is not defined.", #VAR); \
        } \
    } else { \
        int32 getenv_len_ = strlen32(VAR); \
        char *getenv_copy_ = malloc2(getenv_len_ + 1); \
        memcpy64(getenv_copy_, VAR, getenv_len_ + 1); \
        (VAR) = getenv_copy_; \
    } \
} while (0)

#define PARSE_OPTION(ARG, NAME) \
    if (parse_option(&(NAME), ARG, #NAME)) { \
        continue; \
    }

#define ENUM_NAME CommandFlag
#define ENUM_BITFLAGS 1
#define ENUM_PREFIX_ COMMAND_FLAG_
#define ENUM_FIELDS \
    X(COMMAND_FLAG_CAPTURE_STDOUT)      \
    X(COMMAND_FLAG_CAPTURE_STDERR)      \
    X(COMMAND_FLAG_MERGE_STDERR)        \
    X(COMMAND_FLAG_ASYNC)               \
    X(COMMAND_FLAG_DETACHED)            \
    X(COMMAND_FLAG_NEW_SESSION)         \
    X(COMMAND_FLAG_NEW_PROCESS_GROUP)   \
    X(COMMAND_FLAG_STDIN_TTY)           \
    X(COMMAND_FLAG_CLOSE_STDIN)
#define XENUMS_NO_TESTS 1
#include "xenums.c"
#undef XENUMS_NO_TESTS

typedef struct CommandResult {
    char *output;
    char *stdout_output;
    char *stderr_output;

    int64 pid;

    int32 output_len;
    int32 stdout_len;
    int32 stderr_len;
    int32 status;
    int32 error_status;
    int32 exit_status;
    int32 term_signal;
    int32 stdout_fd;
    int32 stderr_fd;

    bool exited;
    bool signaled;
    bool stdout_fd_open;
    bool stderr_fd_open;
} CommandResult;

typedef struct Command {
    char **argv;
    char **env;
    char *cwd;

    int32 *argvs_lens;
    int32 *env_lens;
    int32 cwd_len;
    int32 argc;
    int32 env_len;
    int32 cap;
    int32 env_cap;
    int32 error_status;

    CommandResult result;
} Command;

static void command_argv0_set(Command *, char *);
static void command_child_env_apply(Command *);
static void command_child_exec(
    Command *, enum CommandFlag, int [2], int [2]
) __attribute__((noreturn));
static void command_cwd_clear(Command *);
static void command_cwd_set(Command *, char *);
static void command_env_clear(Command *);
static void command_env_printf(Command *, char *, ...);
static void command_env_push(Command *, char *);
static void command_env_push_length(Command *, char *, int32);
static void command_error_set(Command *, int32);
static bool command_flags_capture(enum CommandFlag);
static bool command_flags_capture_stderr(enum CommandFlag);
static bool command_flags_capture_stdout(enum CommandFlag);
static enum CommandFlag command_flags_normalized(enum CommandFlag);
static void command_free(Command *);
static void command_print(Command *);
static void command_printf(Command *, char *, ...);
static void command_push(Command *, char *);
static void command_push_length(Command *, char *, int32);
static void command_push_owned_length(
    char ***,
    int32 **,
    int32 *,
    int32 *,
    char *,
    int32
);
static void command_push_split(Command *, char *, char *);
static void command_reset(Command *);
static void command_result_append(
    StrBuilder *,
    StrBuilder *,
    StrBuilder *,
    bool,
    char *,
    int32
);
static void command_result_file_descriptors_close(CommandResult *);
static void command_result_free(CommandResult *);
static void command_result_init(CommandResult *);
static void command_result_read_captured(Command *);
static bool command_run(Command *, enum CommandFlag);
static bool command_run_async(Command *, enum CommandFlag);
static bool command_run_capture(Command *, enum CommandFlag);
static bool command_run_capture_all(Command *);
static bool command_run_capture_combined(Command *);
static bool command_run_sync(Command *, int *);
static bool command_signal(Command *, int32, bool);
static bool command_start(Command *, enum CommandFlag);
static int32 command_status_from_wait(int, CommandResult *);
static char *command_str(Command *, int32 *);
static void command_vector_reserve(char ***, int32 **, int32 *, int32, int32);
static bool command_wait(Command *);

#define COMMAND_PUSH_2(A, B) command_push(A, B)
#define COMMAND_PUSH_3(A, B, B_LEN) command_push_length(A, B, B_LEN)
#define COMMAND_PUSH(...) SELECT_ON_NUM_ARGS(COMMAND_PUSH_, __VA_ARGS__)

#define COMMAND_ENV_PUSH_2(A, B) command_env_push(A, B)
#define COMMAND_ENV_PUSH_3(A, B, B_LEN) \
    command_env_push_length(A, B, B_LEN)
#define COMMAND_ENV_PUSH(...) \
    SELECT_ON_NUM_ARGS(COMMAND_ENV_PUSH_, __VA_ARGS__)

#if !defined(SORT_COMPARE)
#define SORT_COMPARE(A, B) compare_func(A, B)
#endif

#if !defined(MAX_NTHREADS)
#define MAX_NTHREADS 64
#endif

typedef struct HeapNode {
    void *value;
    int32 p_index;
    int32 unused;
} HeapNode;

static void sort_functions_sink(void);
static void sort_heapify(HeapNode *, int32, int32, int32 (*)(void *, void *));
static void sort_merge_subsorted(
    void *,
    int32,
    int32,
    int64,
    int32 (*)(void *, void *)
);
static void sort_shuffle(void *, int64, int64);

typedef struct GenericArrayHeader {
    ldouble alignment;
    int32 count;
    int32 cap;
    int64 padding;
} GenericArrayHeader;

static void *generic_array_init(int32, int64);
static void *generic_array_grow(void *, int64);
static void array_sink(void);

#define ARRAY_HEADER(ARRAY) ((GenericArrayHeader *)(ARRAY) - 1)
#define ARRAY_LEN(ARRAY) ((ARRAY) ? ARRAY_HEADER(ARRAY)->count : 0)
#define ARRAY_CLEAR(ARRAY) do { \
    if (ARRAY) { \
        ARRAY_HEADER(ARRAY)->count = 0; \
    } \
} while (0)
#define ARRAY_FREE(ARRAY) do { \
    if (ARRAY) { \
        GenericArrayHeader *array_header_ = ARRAY_HEADER(ARRAY); \
        free2(array_header_, SIZEOF(*array_header_) \
              + array_header_->cap*SIZEOF(*(ARRAY))); \
        (ARRAY) = NULL; \
    } \
} while (0)
#define ARRAY_PUSH(ARRAY, ...) \
    ((ARRAY) = generic_array_grow((ARRAY), SIZEOF(*(ARRAY))), \
     (ARRAY)[ARRAY_HEADER(ARRAY)->count++] = (__VA_ARGS__))
#define ARRAY_INIT(ARRAY, CAPACITY) \
    ((ARRAY) = generic_array_init((CAPACITY), SIZEOF(*(ARRAY))))

#include "meta.h"

#endif /* CBASE_H */

#if defined(CBASE_IMPLEMENT) && !defined(CBASE_IMPLEMENTED)
#define CBASE_IMPLEMENTED 1

#include "arena.c"
#include "memory.c"
#include "utf8.c"
#include "util.c"
#include "command.c"
#include "sort.c"
#include "array.c"
#include "meta_common.c"
#include "meta_tokenize.c"
#include "meta_parse.c"
#include "meta_generate.c"

#endif /* CBASE_IMPLEMENT && !CBASE_IMPLEMENTED */
