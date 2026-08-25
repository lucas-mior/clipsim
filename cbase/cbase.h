// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(CBASE_H)
#define CBASE_H

#if !defined(DEBUGGING)
#define DEBUGGING 0
#endif

#if !defined(ERROR_NOTIFY)
#define ERROR_NOTIFY 0
#endif

#include "platform_detection.h"
#include "libc.h"
#include "primitives.h"
#include "base_macros.h"

static char UNUSED *program = __FILE__;
static int32 UNUSED program_len;
static bool UNUSED timezone_initialized = false;
static time_t UNUSED timezone_offset = 0;
static int64 UNUSED here_counter = 0;

#define error(...)  error_impl(__FILE__, __LINE__, FUNC__, __VA_ARGS__)
#define error2(...) fprintf(stderr, __VA_ARGS__)
noreturn void fatal(int32);
void error_impl(char *, int32, char *, char *, ...)
    ATTR_PRINTF(4, 5);
int memcmp64(void *, void *, int64);
void *memmem64(void *, int64, void *, int64);
void *memchr64(void *, int32, int64);
void *memrchr64(void *, int32, int64);
bool util_glob_match(char *, int32, char *, int32);

int fdtruncate64(int32 fd, int64 len);

INLINE int32
strlen32(char *string) {
    size_t len;

    if (DEBUGGING) {
        if (string == NULL) {
            TRAP();
        }
    }
    len = strlen(string);

    if (DEBUGGING) {
        if (len >= INT32_MAX) {
            error("Error: string (%.*s ...) is too long.\n", 50, string);
            fatal(EXIT_FAILURE);
        }
    }

    return (int32)len;
}

INLINE int32
optional_strlen32(char *string) {
    if (string == NULL) {
        return 0;
    }
    return strlen32(string);
}

#include "i18n.h"
#include "memory.h"
#include "arena.h"

#include "assertions.h"
#include "generic.h"
#include "minmax.c"

#define UTF_INVALID 0xFFFD

typedef struct DirEntry {
    int32 name_len;
    char name[256];
} DirEntry;

int32 get_directory_entries(char *, DirEntry **);
int32 utf8_random_string(char *buffer, int32 capacity, int32 min_len);
int32 utf8_byte_position(char *string, int32 string_len, int32 character);
int32 utf8_capitalize_first_letters(char *string, int32 string_len,
                                    char *buffer, int32 buffer_capacity);
int32 utf8_char_width(uint32 rune);
int32 utf8_characters(char *string, int32 string_len);
int32 utf8_cut_width(char *string, int32 string_len, int32 max_width);
int32 utf8_decode(char *string, int32 string_len, uint32 *rune);
uint32 utf8_decode_byte(char c, int32 *i);
int32 utf8_decode_raw(char *c, uint32 *u, int32 clen);
int32 utf8_encode(uint32 rune, char *buffer, int32 buffer_capacity);
char utf8_encode_byte(uint32 u, int32 i);
int32 utf8_encode_raw(uint32 u, char *c);
bool utf8_has_bom(char *text, int32 text_len);
bool utf8_valid(char *text, int32 text_len, int32 *bad_offset);
int32 utf8_next_position(char *string, int32 string_len, int32 byte);
int32 utf8_suffix_width_position(char *string, int32 string_len,
                                 int32 max_width);
int32 utf8_validate(uint32 *u, int32 i);
int32 utf8_width(char *string, int32 string_len);

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

int32 util_copy_file_async(char *, char *, int *);
void util_copy_file_async_parsed(UtilCopyFilesAsync *);
void *util_copy_file_async_thread(void *);
#endif

bool util_is_integer(char *string);
noreturn void util_segv_handler(int32);
int32 itoa2(char *, int32, llong);
long atoi2(char *);
char *basename2(char *, int32 *, int32 *);
char *begins_with(char *, int32, char *, int32);
bool byte_matches_any(char, void *, int64);
int32 bytes_pretty(char *, int64);
void catfile(int, char *);
double deg2rad(double);
int32 dirname2(char *, char *, int32 *);
char *ends_with(char *, int32, char *, int32);
void error_async_safe(char *);
bool is_ident_char(char);
bool is_ident_start_char(char);
void normalize(char *restrict, int32 *restrict);
int32 parse_option(char **, char *, char *);
char *path_basename(char *, int32);
void print_timings(char *, int32, char *, int64,
                   struct timespec, struct timespec);
void qsort64(void *, int64, int64, int (*)(void *, void *));
void random_filename_inplace(char *, int32);
void rand_int_seed(uint64);
int32 rand_int(void);
double rad2deg(double);
int32 random_ascii_string(char *, int32, int32);
bool path_missing(char *);
int32 read_entire_file(char *, char **);
char *remove_escape_sequences(char *, int32 *);
void sb_append(StrBuilder *, char *, int32);
void sb_append_byte(StrBuilder *, char);
void sb_append_byte_if_not(StrBuilder *, char);
void sb_clear(StrBuilder *);
bool sb_copy(StrBuilder *, StrBuilder *);
void sb_free(StrBuilder *);
void sb_init(StrBuilder *);
void sb_move(StrBuilder *, StrBuilder *);
void sb_printf(StrBuilder *, char *, ...);
void sb_reserve(StrBuilder *, int32);
bool sb_set(StrBuilder *, char *, int32);
char *sb_steal(StrBuilder *, int32 *, int32 *);
char *sb_steal_exact(StrBuilder *, int32 *);
char *sb_opt_cstr(StrBuilder *buffer);
void send_signal(char *, int32);
int32 snprintf2(char *, int64, char *, ...);
StrBuilder *str_builder_array_append(StrBuilderArray *);
bool str_builder_array_append_copy(StrBuilderArray *, StrBuilder *);
void str_builder_array_clear(StrBuilderArray *);
bool str_builder_array_copy(StrBuilderArray *, StrBuilderArray *);
void str_builder_array_destroy(StrBuilderArray *);
void str_builder_array_init(StrBuilderArray *);
void str_builder_array_move(StrBuilderArray *, StrBuilderArray *);
bool str_builder_array_reserve(StrBuilderArray *, int32);
void str_builder_array_swap(StrBuilderArray *, StrBuilderArray *);
int32 string_from_strings(char *, int32, char *, char **, int32);
int32 string_from_doubles(char *, int32, char *, double *, int32);
double clamp_double(double, double, double);
double square_double(double);
int64 clamp_int64(int64, int64, int64);
int32 clamp_int32(int32, int32, int32);
int64 square_int64(int64);
int32 square_int32(int32);

#define MEM_LITERAL_SHORT_N 2
#include "mem_literal_short.h"
#define MEM_LITERAL_SHORT_N 3
#include "mem_literal_short.h"
#define MEM_LITERAL_SHORT_N 4
#include "mem_literal_short.h"
#define MEM_LITERAL_SHORT_N 5
#include "mem_literal_short.h"
#define MEM_LITERAL_SHORT_N 6
#include "mem_literal_short.h"
#define MEM_LITERAL_SHORT_N 7
#include "mem_literal_short.h"
#define MEM_LITERAL_SHORT_N 8
#include "mem_literal_short.h"
#define MEM_LITERAL_SHORT_N 9
#include "mem_literal_short.h"
#define MEM_LITERAL_SHORT_N 10
#include "mem_literal_short.h"
#define MEM_LITERAL_SHORT_N 11
#include "mem_literal_short.h"
#define MEM_LITERAL_SHORT_N 12
#include "mem_literal_short.h"
#define MEM_LITERAL_SHORT_N 13
#include "mem_literal_short.h"
#define MEM_LITERAL_SHORT_N 14
#include "mem_literal_short.h"
#define MEM_LITERAL_SHORT_N 15
#include "mem_literal_short.h"

INLINE UNUSED bool32
strequal(char *s1, char *s2) {
    return !strcmp(s1, s2);
}

INLINE UNUSED bool32
strequal2(char *a, int32 a_len, char *b, int32 b_len) {
    if (a_len != b_len) {
        return false;
    }

    return !memcmp64(a, b, a_len);
}

INLINE UNUSED bool32
optional_strequal(char *a, int32 a_len, char *b, int32 b_len) {
    if ((a == NULL) || (b == NULL)) {
        return false;
    }

    return strequal2(a, a_len, b, b_len);
}

bool32 striqual(char *, char *);
bool32 striqual2(char *, int32, char *, int32);
int64 strftime2(char *, int64, char *, struct tm *);
int strncmp32(char *, char *, int64);
char *strncpy32(char *, char *, int64);
void sleep_ms(int64);
void sleep_ns(int64);
void sleep_us(int64);
double timediff(struct timespec, struct timespec);
void time_monotonic_coarse(struct timespec *);
void time_monotonic_precise(struct timespec *);
void timezone_init(void);
char *cbase_getcwd(char *, int64);
int32 cbase_mkdir(char *);
int32 cbase_rmdir(char *);
int32 cbase_unlink(char *);
int32 cbase_remove_file(char *);
int32 cbase_remove_empty_dir(char *);
int32 cbase_mkstemps(char *, int32);
int32 cbase_make_temp_file(char *, int32, char *, char *);
int32 util_copy_file_sync(char *, char *);
void util_die_notify(char *, char *, ...);
bool util_equal_files(char *, char *);
bool util_file_exists(char *);
int32 util_filename_from(char *, int64, int);
int32 util_nthreads(void);
int32 util_string_int32(int32 *, char *);
void warn(char *, ...);
int64 read64(int32, void *, int64);
int64 write64(int32, void *, int64);
int64 fread64(void *, int64, int64, FILE *);
int64 fwrite64(void *, int64, int64, FILE *);

#if !defined(PARALLEL_FOR_MAX_THREADS)
#define PARALLEL_FOR_MAX_THREADS 64
#endif

#if !defined(MIN_PARALLEL_ITEMS)
#define MIN_PARALLEL_ITEMS 64
#endif

typedef void ParallelForFunction(int64, int64, int32, void *);

int32 parallel_for(
    int64,
    ParallelForFunction *,
    void *
);
int32 parallel_for_min_items(
    int64,
    int64,
    ParallelForFunction *,
    void *
);
int32 parallel_for_max_threads_min_items(
    int64,
    int32,
    int64,
    ParallelForFunction *,
    void *
);
void write_all(int, char *, int64);
int64 write_entire_file(char *, char *, int64);
int xclose(char *, int, int *, char *, char *);
#if HAS_POSIX_WIN_SUBSET
int xclosedir(DIR *, char *);
#endif
char *cbase_mkdtemp(char *);
int xfclose(char *, int32, char *, FILE *, char *);
FILE *xfopen(char *, int32, char *, char *, char *);
#if OS_WINDOWS
void windows_set_errno(DWORD);
#endif

#if OS_UNIX
void xdup2(int, int);
void xkill(pid_t, int);
void xpipe(int [2]);
void xpthread_cond_destroy(pthread_cond_t *);
void xpthread_create(
    pthread_t *,
    pthread_attr_t *,
    void *(*)(void *),
    void *
);
void xpthread_join(pthread_t *, void **);
void xpthread_mutex_destroy(pthread_mutex_t *);
void xpthread_mutex_init(pthread_mutex_t *, pthread_mutexattr_t *);
void xpthread_mutex_lock(pthread_mutex_t *mutex) ATTR_EXCLUSIVE_LOCK(*mutex);
void xpthread_mutex_unlock(pthread_mutex_t *mutex) ATTR_UNLOCK(*mutex);
#endif

int xunlink(char *);
bool xregular_file_exists(char *);
void test_make_temp_dir(char *, int32, char *);
void test_remove_tree(char *);
void test_join_path(char *, int64, char *, char *);

#if OS_UNIX
bool test_command_exists(char *);
bool test_hardlink_supported(char *);
bool test_symlink_supported(char *);
#endif
void here_impl(char *, int32, char *);

#define STRING_FROM_ARRAY(BUFFER, SEP, ARRAY, LENGTH) \
_Generic((ARRAY), \
    double *: string_from_doubles, \
    char **: string_from_strings \
)(BUFFER, SIZEOF(BUFFER), SEP, ARRAY, LENGTH)

#define CLAMP(VAR, VMIN, VMAX) \
_Generic((VAR), \
    float:   clamp_double, \
    double:  clamp_double, \
    int32:   clamp_int32, \
    default: clamp_int64 \
)(VAR, VMIN, VMAX)

#define SQUARE(VAR) \
_Generic((VAR), \
    float:   square_double, \
    double:  square_double, \
    int32:   square_int32, \
    default: square_int64 \
)(VAR)

#define strequal2_3(A, A_LEN, B)        strequal2(A, A_LEN, B, strlen32(B))
#define strequal2_4(A, A_LEN, B, B_LEN) strequal2(A, A_LEN, B, B_LEN)
#define STREQUAL(...) SELECT_ON_NUM_ARGS(strequal2_, __VA_ARGS__)

#define striqual2_3(A, A_LEN, B)        striqual2(A, A_LEN, B, strlen32(B))
#define striqual2_4(A, A_LEN, B, B_LEN) striqual2(A, A_LEN, B, B_LEN)
#define STRIQUAL(...) SELECT_ON_NUM_ARGS(striqual2_, __VA_ARGS__)

#define MEMMEM_3(LONG, LONG_LEN, SHORT) \
    memmem64(LONG, LONG_LEN, SHORT, strlen32(SHORT))
#define MEMMEM_4(LONG, LONG_LEN, SHORT, LEN) \
    memmem64(LONG, LONG_LEN, SHORT, LEN)
#define MEMMEM(...) SELECT_ON_NUM_ARGS(MEMMEM_, __VA_ARGS__)

#define STRLIT_ARRAY(LITERAL, SIZE) \
    ((void)SIZEOF(struct { \
        _Static_assert(sizeof(LITERAL) <= ((SIZE) + 1), \
                       "string literal does not fit in STRLIT_ARRAY"); \
        char dummy; \
    }), \
    (char[SIZE]){ LITERAL })

#define MEM_LITERAL_SHORT_LENGTHS(X) \
    X(2), \
    X(3), \
    X(4), \
    X(5), \
    X(6), \
    X(7), \
    X(8), \
    X(9), \
    X(10), \
    X(11), \
    X(12), \
    X(13), \
    X(14), \
    X(15)

#define MEM_LITERAL_SHORT_GENERIC_SLOT(N) \
    char (*)[N]: CAT(mem_literal_short_, N)

#define MEM_LITERAL_SHORT(HAYSTACK, HAYSTACK_LEN, LITERAL) \
_Generic((char (*)[STRLIT_LEN(LITERAL)])0, \
    MEM_LITERAL_SHORT_LENGTHS(MEM_LITERAL_SHORT_GENERIC_SLOT), \
    default: memmem64 \
)(HAYSTACK, HAYSTACK_LEN, LITERAL, STRLIT_LEN(LITERAL))

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

#define BYTE_MATCHES_ANY_2(BYTE, MEMORY) \
    byte_matches_any(BYTE, MEMORY, strlen32(MEMORY))
#define BYTE_MATCHES_ANY_3(BYTE, MEMORY, MEMORY_LEN) \
    byte_matches_any(BYTE, MEMORY, MEMORY_LEN)
#define BYTE_MATCHES_ANY(...) \
    SELECT_ON_NUM_ARGS(BYTE_MATCHES_ANY_, __VA_ARGS__)

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

#define HERE here_impl(__FILE__, __LINE__, FUNC__)

#define NCALLS(INTERVAL) do { \
    static int64 ncalls_ncalls = 1; \
    if ((ncalls_ncalls % (INTERVAL)) == 0) { \
        fprintf(stderr, "%s:%d:%s: called %lld times\n", \
                __FILE__, __LINE__, FUNC__, ncalls_ncalls); \
    } \
    ncalls_ncalls += 1; \
} while (0)

#define PRINT_TIMINGS_3(N, T0, T1) \
    print_timings(__FILE__, __LINE__, FUNC__, N, T0, T1)
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
    if (parse_option(&(NAME), ARG, #NAME) >= 0) { \
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
#define XENUMS_NO_TESTS 1
#include "xenums.c"
#undef XENUMS_NO_TESTS

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

void command_argv0_set(Command *, char *);
void command_child_env_apply(Command *);
noreturn void command_child_exec(
    Command *, enum CommandFlag, int [2], int [2], int [2]
);
#if OS_WINDOWS
void command_windows_command_line(Command *, char *, int64);
char *command_windows_argv0(Command *, char *, int32 *);
int32 command_windows_run_process(Command *, enum CommandFlag);
#endif
void command_cwd_clear(Command *);
void command_cwd_set(Command *, char *);
void command_env_clear(Command *);
void command_env_printf(Command *, char *, ...);
void command_env_push(Command *, char *);
void command_env_push_length(Command *, char *, int32);
void command_error_set(Command *, int32);
bool command_flags_capture(enum CommandFlag);
enum CommandFlag command_flags_normalized(enum CommandFlag);
void command_free(Command *);
void command_print(Command *);
void command_printf(Command *, char *, ...);
void command_push_length(Command *, char *, int32);
void command_push_array(Command *, int32, char **);
void command_push_owned_length(
    char ***,
    int32 **,
    int32 *,
    int32 *,
    char *,
    int32
);
void command_push_split(Command *, char *, char *);
int32 command_stdin_buffer_set(Command *, char *, int64);
void command_stdin_buffer_clear(Command *);
void command_reset(Command *);
void command_result_append(
    StrBuilder *,
    StrBuilder *,
    StrBuilder *,
    bool,
    char *,
    int32
);
void command_result_file_descriptors_close(CommandResult *);
void command_result_free(CommandResult *);
void command_result_init(CommandResult *);
void command_result_read_captured(Command *);
void command_result_process_io(Command *, enum CommandFlag);
int32 command_run(Command *, enum CommandFlag);
int32 command_run_async(Command *, enum CommandFlag);
int32 command_run_capture(Command *, enum CommandFlag);
int32 command_run_capture_all(Command *);
int32 command_run_capture_combined(Command *);
int32 command_run_sync(Command *, int *);
int32 command_signal(Command *, int32, bool);
int32 command_start(Command *, enum CommandFlag);
int32 command_status_from_wait(int, CommandResult *);
char *command_str(Command *, int32 *);
void command_vector_reserve(char ***, int32 **, int32 *, int32, int32);
int32 command_wait(Command *);

#define COMMAND_PUSH(CMD, ...) \
    command_push_array(CMD, \
                       (int32)(sizeof((char *[]){__VA_ARGS__}) \
                               /sizeof(char *)), \
                       (char *[]){__VA_ARGS__})

#define COMMAND_ENV_PUSH_2(A, B) command_env_push(A, B)
#define COMMAND_ENV_PUSH_3(A, B, B_LEN) \
    command_env_push_length(A, B, B_LEN)
#define COMMAND_ENV_PUSH(...) \
    SELECT_ON_NUM_ARGS(COMMAND_ENV_PUSH_, __VA_ARGS__)

#if !defined(MAX_NTHREADS)
#define MAX_NTHREADS 64
#endif

// Note: it is fine to typedef union in this case
#if CBASE_CRT_MSVC
#pragma warning(push)
#pragma warning(disable: 4324)
#endif
typedef union GenericArrayHeader {
    struct {
        int32 count;
        int32 cap;
    };
    CbaseMaxAlign alignment;
} GenericArrayHeader;
#if CBASE_CRT_MSVC
#pragma warning(pop)
#endif

void *generic_array_init(int32, int64);
void *generic_array_grow(void *, int64);
bool generic_array_reserve(void **, int32, int64);
int32 generic_array_capacity(void *);
void generic_array_set_count(void *, int32);

#define ARRAY_HEADER(ARRAY) \
    ((GenericArrayHeader *)((void *)(ARRAY)) - 1)
#define ARRAY_LEN(ARRAY) ((ARRAY) ? ARRAY_HEADER(ARRAY)->count : 0)
#define ARRAY_CAPACITY(ARRAY) generic_array_capacity(ARRAY)
#define ARRAY_RESERVE(ARRAY, NEEDED_COUNT) \
    generic_array_reserve((void **)&(ARRAY), \
                          (NEEDED_COUNT), \
                          SIZEOF(*(ARRAY)))
#define ARRAY_SET_COUNT(ARRAY, COUNT) \
    generic_array_set_count((ARRAY), (COUNT))
#define ARRAY_INIT_COUNT(ARRAY, COUNT) do { \
    ARRAY_INIT((ARRAY), (COUNT)); \
    ARRAY_SET_COUNT((ARRAY), (COUNT)); \
} while (0)
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

#if CC_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#endif

// when you need a valid symbol
// to silence clangd warnings in include-based templates.
typedef void ThrowAwayFunction();
void throw_away_function();

#if CC_CLANG
#pragma clang diagnostic pop
#endif

#include "meta.h"

#endif /* CBASE_H */

#if defined(CBASE_IMPLEMENT) && !defined(CBASE_IMPLEMENTED)
#define CBASE_IMPLEMENTED 1

#include "arena.c"
#include "memory.c"
#include "generic.c"
#include "assertions.c"
#include "array.c"
#include "utf8.c"
#include "util.c"
#include "string.c"
#include "time.c"
#include "fs.c"
#if OS_WINDOWS
#include "windows.c"
#endif
#include "directory.c"
#include "threads.c"

#define ENUM_NAME CommandFlag
#define ENUM_BITFLAGS 1
#define ENUM_PREFIX_ COMMAND_
#define ENUM_UNDERLYING_TYPE uint32
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
#define XENUMS_NO_TESTS 1
#include "xenums.c"
#undef XENUMS_NO_TESTS

#include "command.c"
#include "cbase.h"
#include "meta_common.c"
#include "meta_tokenize.c"
#include "meta_parse.c"
#include "meta_generate.c"

#endif
