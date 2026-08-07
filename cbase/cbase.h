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
#if !defined(TESTING_threads)
#define TESTING_threads 0
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

#if !defined(ERROR_NOTIFY)
#define ERROR_NOTIFY 0
#endif

#include "platform_detection.h"
#include "primitives.h"
#include "base_macros.h"

static char *program = __FILE__;
static int32 program_len UNUSED;

#define error(...) \
    error_impl(__FILE__, __LINE__, (char *)__func__, __VA_ARGS__)
#define error2(...) fprintf(stderr, __VA_ARGS__)
CBASE_API_DECL int32 optional_strlen32(char *);
CBASE_API_DECL int32 strlen32(char *);
CBASE_API_DECL void fatal(int32) __attribute__((noreturn));
CBASE_API_DECL void error_impl(char *, int32, char *, char *, ...)
    __attribute__((format(printf, 4, 5)));
CBASE_API_DECL int memcmp64(void *, void *, int64);
CBASE_API_DECL void *memmem64(void *, int64, void *, int64);
CBASE_API_DECL void *memrchr64(void *, int32, int64);

#include "libc.h"
#if defined(ALIGN)
#undef ALIGN
#endif
#define ALIGN(x) ALIGN_POWER_OF_2(x, ALIGNMENT)
#include "i18n.h"
#include "memory.h"
#include "arena.h"

#include "assert.c"
#include "generic.c"

#define UTF_INVALID 0xFFFD

CBASE_API_DECL int32 random_utf8_string(char *, int32, int32);
CBASE_API_DECL int32 utf8_byte_position(char *, int32, int32);
CBASE_API_DECL int32 utf8_capitalize_first_letters(char *, int32, char *, int32);
CBASE_API_DECL int32 utf8_char_width(uint32);
CBASE_API_DECL int32 utf8_characters(char *, int32);
CBASE_API_DECL int32 utf8_cut_width(char *, int32, int32);
CBASE_API_DECL int32 utf8_decode(char *, int32, uint32 *);
CBASE_API_DECL uint32 utf8_decode_byte(char, int32 *);
CBASE_API_DECL int32 utf8_decode_raw(char *, uint32 *, int32);
CBASE_API_DECL int32 utf8_encode(uint32, char *, int32);
CBASE_API_DECL char utf8_encode_byte(uint32, int32);
CBASE_API_DECL int32 utf8_encode_raw(uint32, char *);
CBASE_API_DECL bool utf8_has_bom(char *, int32);
CBASE_API_DECL bool utf8_valid(char *, int32, int32 *);
CBASE_API_DECL int32 utf8_next_position(char *, int32, int32);
CBASE_API_DECL int32 utf8_suffix_width_position(char *, int32, int32);
CBASE_API_DECL int32 utf8_validate(uint32 *, int32);
CBASE_API_DECL int32 utf8_width(char *, int32);

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

CBASE_API_DECL int32 util_copy_file_async(char *, char *, int *);
CBASE_API_DECL void util_copy_file_async_parsed(UtilCopyFilesAsync *);
CBASE_API_DECL void *util_copy_file_async_thread(void *);
#endif

CBASE_API_DECL bool util_is_integer(char *string);
CBASE_API_DECL void util_segv_handler(int32) __attribute__((noreturn));
CBASE_API_DECL int32 itoa2(char *, int32, llong);
CBASE_API_DECL long atoi2(char *);
CBASE_API_DECL char *basename2(char *, int32 *, int32 *);
CBASE_API_DECL char *begins_with(char *, int32, char *, int32);
CBASE_API_DECL int32 bytes_pretty(char *, int64);
CBASE_API_DECL void catfile(int, char *);
CBASE_API_DECL double deg2rad(double);
CBASE_API_DECL int32 dirname2(char *, char *, int32 *);
CBASE_API_DECL char *ends_with(char *, int32, char *, int32);
CBASE_API_DECL void error_async_safe(char *);
CBASE_API_DECL bool is_ident_char(char);
CBASE_API_DECL bool is_ident_start_char(char);
CBASE_API_DECL void *memchr64(void *, int32, int64);
CBASE_API_DECL void normalize(char *restrict, int32 *restrict);
CBASE_API_DECL bool parse_option(char **, char *, char *);
CBASE_API_DECL char *path_basename(char *, int32);
CBASE_API_DECL void print_timings(
    char *,
    int32,
    char *,
    int64,
    struct timespec,
    struct timespec
);
CBASE_API_DECL void qsort64(void *, int64, int64, int (*)(void *, void *));
CBASE_API_DECL double rad2deg(double);
CBASE_API_DECL int32 random_ascii_string(char *, int32, int32);
CBASE_API_DECL bool path_missing(char *);
CBASE_API_DECL bool read_entire_file(char *, char **, int32 *);
CBASE_API_DECL char *remove_escape_sequences(char *, int32 *);
CBASE_API_DECL void sb_append(StrBuilder *, char *, int32);
CBASE_API_DECL void sb_append_byte(StrBuilder *, char);
CBASE_API_DECL void sb_append_byte_if_not(StrBuilder *, char);
CBASE_API_DECL void sb_clear(StrBuilder *);
CBASE_API_DECL bool sb_copy(StrBuilder *, StrBuilder *);
CBASE_API_DECL void sb_free(StrBuilder *);
CBASE_API_DECL void sb_init(StrBuilder *);
CBASE_API_DECL void sb_move(StrBuilder *, StrBuilder *);
CBASE_API_DECL void sb_printf(StrBuilder *, char *, ...);
CBASE_API_DECL void sb_reserve(StrBuilder *, int32);
CBASE_API_DECL bool sb_set(StrBuilder *, char *, int32);
CBASE_API_DECL char *sb_steal(StrBuilder *, int32 *, int32 *);
CBASE_API_DECL char *sb_steal_exact(StrBuilder *, int32 *);
CBASE_API_DECL char *sb_opt_cstr(StrBuilder *buffer);
CBASE_API_DECL void send_signal(char *, int32);
CBASE_API_DECL int32 snprintf2(char *, int64, char *, ...);
CBASE_API_DECL StrBuilder *str_builder_array_append(StrBuilderArray *);
CBASE_API_DECL bool str_builder_array_append_copy(StrBuilderArray *, StrBuilder *);
CBASE_API_DECL void str_builder_array_clear(StrBuilderArray *);
CBASE_API_DECL bool str_builder_array_copy(StrBuilderArray *, StrBuilderArray *);
CBASE_API_DECL void str_builder_array_destroy(StrBuilderArray *);
CBASE_API_DECL void str_builder_array_init(StrBuilderArray *);
CBASE_API_DECL void str_builder_array_move(StrBuilderArray *, StrBuilderArray *);
CBASE_API_DECL bool str_builder_array_reserve(StrBuilderArray *, int32);
CBASE_API_DECL void str_builder_array_swap(StrBuilderArray *, StrBuilderArray *);
CBASE_API_DECL int32 string_from_strings(char *, int32, char *, char **, int32);
CBASE_API_DECL int32 string_from_doubles(char *, int32, char *, double *, int32);
CBASE_API_DECL double clamp_double(double, double, double);
CBASE_API_DECL double square_double(double);
CBASE_API_DECL int64 clamp_int64(int64, int64, int64);
CBASE_API_DECL int64 square_int64(int64);
CBASE_API_DECL bool strequal(char *, char *);
CBASE_API_DECL bool strequal2(char *, int32, char *, int32);
CBASE_API_DECL bool striqual(char *, char *);
CBASE_API_DECL bool striqual2(char *, int32, char *, int32);
CBASE_API_DEF bool optional_strequal(char *a, int32 a_len, char *b, int32 b_len);
CBASE_API_DECL int64 strftime2(char *, int64, char *, struct tm *);
CBASE_API_DECL int strncmp32(char *, char *, int64);
CBASE_API_DECL char *strncpy32(char *, char *, int64);
CBASE_API_DECL double timediff(struct timespec, struct timespec);
CBASE_API_DECL void time_monotonic_coarse(struct timespec *);
CBASE_API_DECL void time_monotonic_precise(struct timespec *);
CBASE_API_DECL void timezone_init(void);
CBASE_API_DECL int32 util_copy_file_sync(char *, char *);
CBASE_API_DECL void util_die_notify(char *, char *, ...);
CBASE_API_DECL bool util_equal_files(char *, char *);
CBASE_API_DECL bool util_file_exists(char *);
CBASE_API_DECL int32 util_filename_from(char *, int64, int);
CBASE_API_DECL int32 util_nthreads(void);
CBASE_API_DECL int32 util_string_int32(int32 *, char *);
CBASE_API_DECL void warn(char *, ...);
CBASE_API_DECL int64 read64(int32, void *, int64);
CBASE_API_DECL int64 write64(int32, void *, int64);
CBASE_API_DECL int64 fread64(void *, int64, int64, FILE *);
CBASE_API_DECL int64 fwrite64(void *, int64, int64, FILE *);

#if !defined(PARALLEL_FOR_MAX_THREADS)
#define PARALLEL_FOR_MAX_THREADS 64
#endif

#if !defined(MIN_PARALLEL_ITEMS)
#define MIN_PARALLEL_ITEMS 64
#endif

typedef void ParallelForFunction(int64, int64, int32, void *);

CBASE_API_DECL int32 parallel_for(
    int64,
    ParallelForFunction *,
    void *
);
CBASE_API_DECL int32 parallel_for_min_items(
    int64,
    int64,
    ParallelForFunction *,
    void *
);
CBASE_API_DECL int32 parallel_for_max_threads_min_items(
    int64,
    int32,
    int64,
    ParallelForFunction *,
    void *
);
CBASE_API_DECL void write_all(int, char *, int64);
CBASE_API_DECL bool write_entire_file(char *, char *, int64);
CBASE_API_DECL int xclose(char *, int, int *, char *, char *);
CBASE_API_DECL int xclosedir(DIR *, char *);
CBASE_API_DECL void xdup2(int, int);
CBASE_API_DECL int xfclose(char *, int32, char *, FILE *, char *);
CBASE_API_DECL FILE *xfopen(char *, int32, char *, char *, char *);
CBASE_API_DECL void xkill(pid_t, int);
CBASE_API_DECL void xpipe(int [2]);
#if OS_UNIX
CBASE_API_DECL void xpthread_cond_destroy(pthread_cond_t *);
CBASE_API_DECL void xpthread_create(
    pthread_t *,
    pthread_attr_t *,
    void *(*)(void *),
    void *
);
CBASE_API_DECL void xpthread_join(pthread_t *, void **);
CBASE_API_DECL void xpthread_mutex_destroy(pthread_mutex_t *);
CBASE_API_DECL void xpthread_mutex_lock(pthread_mutex_t *);
CBASE_API_DECL void xpthread_mutex_unlock(pthread_mutex_t *);
#endif
CBASE_API_DECL int xunlink(char *);
#if TESTING && OS_UNIX
CBASE_API_DECL bool test_command_exists(char *);
CBASE_API_DECL bool test_hardlink_supported(char *);
CBASE_API_DECL bool test_symlink_supported(char *);
CBASE_API_DECL void test_join_path(char *, int64, char *, char *);
CBASE_API_DECL void test_make_temp_dir(char *, int32, char *);
CBASE_API_DECL void test_remove_tree(char *);
#endif
CBASE_API_DECL void here_impl(char *, int32, char *);

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

#define SB_APPEND_2(SB, STRING)      sb_append(SB, STRING, strlen32(STRING))
#define SB_APPEND_3(SB, STRING, LEN) sb_append(SB, STRING, (int32)(LEN))
#define SB_APPEND(...) SELECT_ON_NUM_ARGS(SB_APPEND_, __VA_ARGS__)

#define strequal2_3(A, A_LEN, B)        strequal2(A, A_LEN, B, strlen32(B))
#define strequal2_4(A, A_LEN, B, B_LEN) strequal2(A, A_LEN, B, B_LEN)
#define STREQUAL(...) SELECT_ON_NUM_ARGS(strequal2_, __VA_ARGS__)

#define striqual2_3(A, A_LEN, B) striqual2(A, A_LEN, B, strlen32(B))
#define striqual2_4(A, A_LEN, B, B_LEN) striqual2(A, A_LEN, B, B_LEN)
#define STRIQUAL(...) SELECT_ON_NUM_ARGS(striqual2_, __VA_ARGS__)

#define HERE here_impl(__FILE__, __LINE__, (char *)__func__)

#define NCALLS(INTERVAL) do { \
    static int64 ncalls_ncalls = 1; \
    if ((ncalls_ncalls % (INTERVAL)) == 0) { \
        fprintf(stderr, "%s:%d:%s: called %lld times\n", \
                __FILE__, __LINE__, __func__, ncalls_ncalls); \
    } \
    ncalls_ncalls += 1; \
} while (0)

#define PRINT_TIMINGS_3(N, T0, T1) \
    print_timings(__FILE__, __LINE__, (char *)__func__, N, T0, T1)
#define PRINT_TIMINGS_4(N, T0, T1, NAME) \
    print_timings(__FILE__, __LINE__, NAME, N, T0, T1)
#define PRINT_TIMINGS(...) SELECT_ON_NUM_ARGS(PRINT_TIMINGS_, __VA_ARGS__)

#define GETENV(VAR) do {                                    \
    if (((VAR) = getenv(#VAR)) == NULL) {                   \
        if (DEBUGGING) {                                    \
            error_impl(__FILE__, __LINE__, FUNC__,          \
                       RED("%s") " is not defined.", #VAR); \
        }                                                   \
    }                                                       \
} while (0)

#define PARSE_OPTION(ARG, NAME) \
    if (parse_option(&(NAME), ARG, #NAME)) { \
        continue; \
    }

#define ENUM_NAME CommandFlag
#define ENUM_BITFLAGS 1
#define ENUM_PREFIX_ COMMAND_
#define ENUM_FIELDS \
    X(COMMAND_CAPTURE_STDOUT)      \
    X(COMMAND_CAPTURE_STDERR)      \
    X(COMMAND_MERGE_STDERR)        \
    X(COMMAND_ASYNC)               \
    X(COMMAND_DETACHED)            \
    X(COMMAND_NEW_SESSION)         \
    X(COMMAND_NEW_PROCESS_GROUP)   \
    X(COMMAND_STDIN_TTY)           \
    X(COMMAND_CLOSE_STDIN)
#define XENUMS_DECLARE_ONLY 1
#define XENUMS_LINKAGE CBASE_API_DECL
#define XENUMS_NO_TESTS 1
#include "xenums.c"
#undef XENUMS_NO_TESTS
#undef XENUMS_LINKAGE

typedef struct CommandResult {
    int64 pid;

    char *output;
    char *stdout_output;
    char *stderr_output;

    int32 output_len;
    int32 stdout_len;
    int32 stderr_len;

    int32 stdin_fd;
    int32 stdout_fd;
    int32 stderr_fd;

    int32 status;
    int32 error_status;
    int32 exit_status;
    int32 term_signal;

    bool exited;
    bool signaled;
    uint8 padding[6];
} CommandResult;

typedef struct Command {
    char **argv;
    char **env;
    char *cwd;
    char *stdin_buffer;

    int32 *argvs_lens;
    int32 *env_lens;
    int32 cwd_len;
    int32 argc;
    int32 env_len;
    int32 cap;
    int32 env_cap;
    int32 error_status;
    int64 stdin_buffer_len;

    CommandResult result;
} Command;

CBASE_API_DECL void command_argv0_set(Command *, char *);
CBASE_API_DECL void command_child_env_apply(Command *);
CBASE_API_DECL void command_child_exec(
    Command *, enum CommandFlag, int [2], int [2], int [2]
) __attribute__((noreturn));
#if OS_WINDOWS
CBASE_API_DECL void command_windows_command_line(Command *, char *, int64);
CBASE_API_DECL char *command_windows_argv0(Command *, char *, int32 *);
CBASE_API_DECL int32 command_windows_run_process(Command *, enum CommandFlag);
#endif
CBASE_API_DECL void command_cwd_clear(Command *);
CBASE_API_DECL void command_cwd_set(Command *, char *);
CBASE_API_DECL void command_env_clear(Command *);
CBASE_API_DECL void command_env_printf(Command *, char *, ...);
CBASE_API_DECL void command_env_push(Command *, char *);
CBASE_API_DECL void command_env_push_length(Command *, char *, int32);
CBASE_API_DECL void command_error_set(Command *, int32);
CBASE_API_DECL bool command_flags_capture(enum CommandFlag);
CBASE_API_DECL enum CommandFlag command_flags_normalized(enum CommandFlag);
CBASE_API_DECL void command_free(Command *);
CBASE_API_DECL void command_print(Command *);
CBASE_API_DECL void command_printf(Command *, char *, ...);
CBASE_API_DECL void command_push_length(Command *, char *, int32);
CBASE_API_DECL void command_push_array(Command *, int32, char **);
CBASE_API_DECL void command_push_owned_length(
    char ***,
    int32 **,
    int32 *,
    int32 *,
    char *,
    int32
);
CBASE_API_DECL void command_push_split(Command *, char *, char *);
CBASE_API_DECL bool command_stdin_buffer_set(Command *, char *, int64);
CBASE_API_DECL void command_stdin_buffer_clear(Command *);
CBASE_API_DECL void command_reset(Command *);
CBASE_API_DECL void command_result_append(
    StrBuilder *,
    StrBuilder *,
    StrBuilder *,
    bool,
    char *,
    int32
);
CBASE_API_DECL void command_result_file_descriptors_close(CommandResult *);
CBASE_API_DECL void command_result_free(CommandResult *);
CBASE_API_DECL void command_result_init(CommandResult *);
CBASE_API_DECL void command_result_read_captured(Command *);
CBASE_API_DECL void command_result_process_io(Command *, enum CommandFlag);
CBASE_API_DECL bool command_run(Command *, enum CommandFlag);
CBASE_API_DECL bool command_run_async(Command *, enum CommandFlag);
CBASE_API_DECL bool command_run_capture(Command *, enum CommandFlag);
CBASE_API_DECL bool command_run_capture_all(Command *);
CBASE_API_DECL bool command_run_capture_combined(Command *);
CBASE_API_DECL bool command_run_sync(Command *, int *);
CBASE_API_DECL bool command_signal(Command *, int32, bool);
CBASE_API_DECL bool command_start(Command *, enum CommandFlag);
CBASE_API_DECL int32 command_status_from_wait(int, CommandResult *);
CBASE_API_DECL char *command_str(Command *, int32 *);
CBASE_API_DECL void command_vector_reserve(char ***, int32 **, int32 *, int32, int32);
CBASE_API_DECL bool command_wait(Command *);

#define COMMAND_PUSH(CMD, ...)                                             \
    command_push_array(CMD,                                                \
                       (SIZEOF((char *[]){__VA_ARGS__}) / SIZEOF(char *)), \
                       (char *[]){__VA_ARGS__})

#define COMMAND_ENV_PUSH_2(A, B) command_env_push(A, B)
#define COMMAND_ENV_PUSH_3(A, B, B_LEN) \
    command_env_push_length(A, B, B_LEN)
#define COMMAND_ENV_PUSH(...) \
    SELECT_ON_NUM_ARGS(COMMAND_ENV_PUSH_, __VA_ARGS__)

#if !defined(MAX_NTHREADS)
#define MAX_NTHREADS 64
#endif

// Note: it is ok to typedef union here
typedef union GenericArrayHeader {
    struct {
        int32 count;
        int32 cap;
    };
    uchar padding[ALIGNMENT];
    max_align_t alignment;
} GenericArrayHeader;

CBASE_API_DECL void *generic_array_init(int32, int64);
CBASE_API_DECL void *generic_array_grow(void *, int64);
CBASE_API_DECL bool generic_array_reserve(void **, int32, int64);
CBASE_API_DECL int32 generic_array_capacity(void *);
CBASE_API_DECL void generic_array_set_count(void *, int32);

#define ARRAY_HEADER(ARRAY) \
    ((GenericArrayHeader *)ASSUME_ALIGNED_EXPR((void *)(ARRAY)) - 1)

#define ARRAY_LEN(ARRAY) ((ARRAY) ? ARRAY_HEADER(ARRAY)->count : 0)

#define ARRAY_CAPACITY(ARRAY) generic_array_capacity(ARRAY)

#define ARRAY_RESERVE(ARRAY, NEEDED_COUNT) \
    generic_array_reserve((void **)&(ARRAY), \
                          (NEEDED_COUNT), \
                          SIZEOF(*(ARRAY)))

#define ARRAY_SET_COUNT(ARRAY, COUNT) \
    generic_array_set_count((ARRAY), (COUNT))

#define ARRAY_INIT_COUNT(ARRAY, COUNT) do { \
    ARRAY_INIT((ARRAY), (COUNT));           \
    ARRAY_SET_COUNT((ARRAY), (COUNT));      \
} while (0)

#define ARRAY_CLEAR(ARRAY) do {            \
    if (ARRAY) {                           \
        ARRAY_HEADER(ARRAY)->count = 0;    \
    }                                      \
} while (0)

#define ARRAY_FREE(ARRAY) do {                                               \
    if (ARRAY) {                                                             \
        GenericArrayHeader *array_header_ = ARRAY_HEADER(ARRAY);             \
        free2(array_header_,                                                 \
              SIZEOF(*array_header_) + array_header_->cap*SIZEOF(*(ARRAY))); \
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
#include "array.c"
#include "utf8.c"
#include "util.c"
#include "threads.c"

#define ENUM_NAME CommandFlag
#define ENUM_BITFLAGS 1
#define ENUM_PREFIX_ COMMAND_
#define ENUM_FIELDS \
    X(COMMAND_CAPTURE_STDOUT)      \
    X(COMMAND_CAPTURE_STDERR)      \
    X(COMMAND_MERGE_STDERR)        \
    X(COMMAND_ASYNC)               \
    X(COMMAND_DETACHED)            \
    X(COMMAND_NEW_SESSION)         \
    X(COMMAND_NEW_PROCESS_GROUP)   \
    X(COMMAND_STDIN_TTY)           \
    X(COMMAND_CLOSE_STDIN)
#define XENUMS_FUNCTIONS_ONLY 1
#define XENUMS_LINKAGE CBASE_API_DEF
#define XENUMS_NO_TESTS 1
#include "xenums.c"
#undef XENUMS_NO_TESTS
#undef XENUMS_LINKAGE

#include "command.c"
#include "cbase.h"
#include "meta_common.c"
#include "meta_tokenize.c"
#include "meta_parse.c"
#include "meta_generate.c"

#endif /* CBASE_IMPLEMENT && !CBASE_IMPLEMENTED */
