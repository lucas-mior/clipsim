// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(CBASE_H)
#define CBASE_H

#if !defined(DEBUGGING)
#define DEBUGGING 0
#endif

#if !defined(TESTING)
#define TESTING 0
#endif

#if !defined(ERROR_NOTIFY)
#define ERROR_NOTIFY 0
#endif

#include "platform_detection.h"
#include "warnings.h"
#include "libc.h"
#include "primitives.h"
#include "base_macros.h"

static char UNUSED *program = __FILE__;
static int32 UNUSED program_len;
static bool UNUSED timezone_initialized = false;
static time_t UNUSED timezone_offset = 0;

#define error(...)  error_impl(__FILE__, __LINE__, FUNC__, __VA_ARGS__)
#define error2(...) fprintf(stderr, __VA_ARGS__)
noreturn void fatal(int32 status);
void error_impl(char *file, int32 line, char *func, char *format, ...)
    ATTR_PRINTF(4, 5);
int memcmp64(void *left, void *right, int64 size);
void *memmem64(void *haystack, int64 haystack_len, void *needle,
               int64 needle_len);
void *memchr64(void *pointer, int32 value, int64 size);
void *memrchr64(void *pointer, int32 value, int64 size);
bool util_glob_match(char *string, int32 string_len, char *glob,
                     int32 glob_len);

int64 ceil64(double x);
int64 floor64(double x);

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

int32 get_directory_entries(char *directory, DirEntry **directory_list);
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

int32 util_copy_file_async(char *destination, char *source, int *dest_fd);
void util_copy_file_async_parsed(UtilCopyFilesAsync *);
void *util_copy_file_async_thread(void *arg);
#endif

bool util_is_integer(char *string);
noreturn void util_segv_handler(int32 signal_number);
int32 itoa2(char *buffer, int32 size, llong num);
int32 parse_integer(char *str, int32 str_len, llong *result);
llong atoi2(char *str, int32 str_len);
llong atoi2sat(char *str, int32 str_len);
char *basename2(char *path, int32 *full_length, int32 *base_len);
char *begins_with(char *string, int32 string_len, char *prefix,
                  int32 prefix_len);
bool byte_matches_any(char byte, void *memory, int64 memory_len);
int32 bytes_pretty(char *buffer, int64 raw);
void catfile(int where, char *file);
double deg2rad(double degrees);
int32 dirname2(char *buffer, char *path, int32 *path_len);
char *ends_with(char *string, int32 string_len, char *suffix,
                int32 suffix_len);
void error_async_safe(char *message);
bool is_ident_char(char c);
bool is_ident_start_char(char c);
void normalize(char *restrict path, int32 *restrict length);
int32 parse_option(char **parsed, char *arg, char *option_name);
char *path_basename(char *path, int32 path_len);
void print_timings(char *file, int32 line, char *func, int64 nitems,
                   struct timespec t0, struct timespec t1);
void qsort64(void *base, int64 n, int64 size,
             int (*compar)(void *a, void *b));
void random_filename_inplace(char *buffer, int32 buffer_len);
void rand_int_seed(uint64 seed);
int32 rand_int(void);
int32 rand_int_range(int32 upper_bound);
void rand_shuffle(void *items, int32 item_count, int32 item_size);
double rad2deg(double radians);
int32 random_ascii_string(char *buffer, int32 capacity, int32 min_len);
bool path_missing(char *path);
int32 read_entire_file(char *path, char **file_bytes);
char *remove_escape_sequences(char *data, int32 *data_len);
void sb_append(StrBuilder *str_builder, char *data, int64 data_len);
void sb_append_byte(StrBuilder *str_builder, char byte);
void sb_append_byte_if_not(StrBuilder *str_builder, char byte);
void sb_clear(StrBuilder *);
int32 sb_copy(StrBuilder *dest, StrBuilder *source);
void sb_free(StrBuilder *);
void sb_itoa(StrBuilder *str_builder, llong num);
void sb_bytes_pretty(StrBuilder *str_builder, llong size);
void sb_move(StrBuilder *dest, StrBuilder *source);
void sb_printf(StrBuilder *str_builder, char *fmt, ...);
void sb_reserve(StrBuilder *str_builder, int64 extra);
int32 sb_set(StrBuilder *str_builder, char *data, int32 data_len);
char *sb_steal(StrBuilder *str_builder, int32 *len, int32 *cap);
char *sb_steal_exact(StrBuilder *str_builder, int32 *len);
char *sb_opt_cstr(StrBuilder *);
void send_signal(char *executable, int32 signal_number);
int32 snprintf2(char *buffer, int64 size, char *format, ...);
StrBuilder *str_builder_array_append(StrBuilderArray *);
int32 str_builder_array_append_copy(StrBuilderArray *array, StrBuilder *item);
void str_builder_array_clear(StrBuilderArray *);
int32 str_builder_array_copy(StrBuilderArray *dest, StrBuilderArray *source);
void str_builder_array_destroy(StrBuilderArray *);
void str_builder_array_move(StrBuilderArray *dest, StrBuilderArray *source);
int32 str_builder_array_reserve(StrBuilderArray *array, int32 extra);
void str_builder_array_swap(StrBuilderArray *left, StrBuilderArray *right);
// Float formatting functions return the formatted byte count, excluding the
// terminating '\0'. Negative return values are errno-style failures:
// -EINVAL for invalid input, -ENOSPC when capacity is insufficient, and
// -ERANGE when the requested precision is unsupported.
//
// This layer exposes shortest round-trip, fixed precision, and scientific
// precision formatting. It intentionally does not expose a %g/general format
// helper: exact %g behavior needs a separate policy layer to choose between
// fixed and scientific output and to handle trailing-zero rules.
int32 format_float32_shortest(char *buffer, int64 capacity, float value);
int32 format_float64_shortest(char *buffer, int64 capacity, double value);
int32 format_float64_fixed(char *buffer, int64 capacity, double value,
                           int32 precision);
int32 format_float64_scientific(char *buffer, int64 capacity, double value,
                                int32 precision);

int32 string_from_strings(char *buffer, int32 size, char *separator,
                          char **array, int32 length);
int32 string_from_doubles(char *buffer, int32 size, char *separator,
                          double *array, int32 length);
double clamp_double(double x, double min, double max);
double square_double(double x);
int64 clamp_int64(int64 x, int64 min, int64 max);
int32 clamp_int32(int32 x, int32 min, int32 max);
int64 square_int64(int64 x);
int32 square_int32(int32 x);

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

// only call on null terminated strings
// avoid this function,
// prefer to always know the length of at least the first string
// and use the STREQUAL macro.
INLINE UNUSED bool32
strequal(char *s1, char *s2) {
    if (DEBUGGING) {
        if ((s1 == NULL) || (s2 == NULL)) {
            TRAP();
        }
    }
    return !strcmp(s1, s2);
}

// don't call directly, use STREQUAL macro instead
INLINE UNUSED bool32
strequal2(char *a, int32 a_len, char *b, int32 b_len) {
    if (a_len != b_len) {
        return false;
    }
    if (a_len == 0) {
        return true;
    }
    if (DEBUGGING) {
        if ((a == NULL) || (b == NULL)) {
            TRAP();
        }
    }

    return !memcmp64(a, b, a_len);
}

INLINE UNUSED bool32
optional_strequal(char *a, int32 a_len, char *b, int32 b_len) {
    if (a && (b == NULL)) {
        return false;
    }
    if (b && (a == NULL)) {
        return false;
    }
    if ((a == NULL) && (b == NULL)) {
        return true;
    }

    return strequal2(a, a_len, b, b_len);
}

bool32 striqual(char *s1, char *s2);
bool32 striqual2(char *a, int32 a_len, char *b, int32 b_len);
int64 strftime2(char *buffer, int64 size, char *format,
                struct tm *time_info);
int strncmp32(char *left, char *right, int64 size);
void sleep_ms(int64 milliseconds);
void sleep_ns(int64 nanoseconds);
void sleep_us(int64 microseconds);
double timediff(struct timespec t0, struct timespec t1);
int64 time_elapsed_ms(int64 start, int64 end);
int64 time_elapsed_ns(int64 start, int64 end);
void time_monotonic_coarse(struct timespec *);
int64 time_monotonic_now(void);
void time_monotonic_precise(struct timespec *);
void timezone_init(void);
char *cbase_getcwd(char *buffer, int64 size);
int32 cbase_mkdir(char *path);
int32 cbase_rmdir(char *path);
int32 cbase_unlink(char *path);
int32 cbase_remove_file(char *path);
int32 cbase_remove_empty_dir(char *path);
int32 cbase_mkstemps(char *template_path, int32 suffix_len);
int32 cbase_make_temp_file(char *buffer, int32 capacity, char *prefix,
                           char *suffix);
int32 fs_copy_file_sync(char *destination, char *source);
void util_die_notify(char *title, char *body, ...);
bool util_equal_files(char *filename_a, char *filename_b);
bool util_file_exists(char *filename);
int32 util_filename_from(char *buffer, int64 size, int fd);
int32 util_nthreads(void);
int32 util_string_int32(int32 *number, char *string);
void warn(char *fmt, ...);
int64 read64(int32 fd, void *buffer, int64 len);
int64 write64(int32 fd, void *buffer, int64 len);
int64 fread64(void *data, int64 item_size, int64 nitems, FILE *file);
int64 fwrite64(void *data, int64 item_size, int64 nitems, FILE *file);

#if OS_UNIX
#define XSIGNAL(NAME) [NAME] = #NAME
static char UNUSED *signal_names[] = {
    XSIGNAL(SIGABRT),
    XSIGNAL(SIGALRM),
    XSIGNAL(SIGVTALRM),
    XSIGNAL(SIGPROF),
    XSIGNAL(SIGBUS),
    XSIGNAL(SIGCHLD),
    XSIGNAL(SIGCONT),
    XSIGNAL(SIGFPE),
    XSIGNAL(SIGHUP),
    XSIGNAL(SIGILL),
    XSIGNAL(SIGINT),
    XSIGNAL(SIGKILL),
    XSIGNAL(SIGPIPE),
#if defined(SIGPOLL)
    XSIGNAL(SIGPOLL),
#endif
    XSIGNAL(SIGQUIT),
    XSIGNAL(SIGSEGV),
    XSIGNAL(SIGSTOP),
    XSIGNAL(SIGSYS),
    XSIGNAL(SIGTERM),
    XSIGNAL(SIGTSTP),
    XSIGNAL(SIGTTIN),
    XSIGNAL(SIGTTOU),
    XSIGNAL(SIGTRAP),
    XSIGNAL(SIGURG),
    XSIGNAL(SIGUSR1),
    XSIGNAL(SIGUSR2),
    XSIGNAL(SIGXCPU),
    XSIGNAL(SIGXFSZ),
};
#undef XSIGNAL
#endif

#if !defined(PARALLEL_FOR_MAX_THREADS)
#define PARALLEL_FOR_MAX_THREADS 64
#endif

#if !defined(MIN_PARALLEL_ITEMS)
#define MIN_PARALLEL_ITEMS 64
#endif

typedef void ParallelForFunction(int64 start, int64 end, int32 worker_id,
                                 void *user_data);

int32 parallel_for(
    int64 length,
    ParallelForFunction *function,
    void *user_data
);
int32 parallel_for_min_items(
    int64 length,
    int64 min_parallel_items,
    ParallelForFunction *function,
    void *user_data
);
int32 parallel_for_max_threads_min_items(
    int64 length,
    int32 max_threads,
    int64 min_parallel_items,
    ParallelForFunction *function,
    void *user_data
);
void write_all(int fd, char *buffer, int64 left);
int64 write_entire_file(char *path, char *text, int64 text_len);
int xclose(char *file, int line, int *fd, char *fd_var_name,
           char *filename);
#if HAS_POSIX_WIN_SUBSET
int xclosedir(DIR *dir, char *dirname);
#endif
char *cbase_mkdtemp(char *template_path);
int xfclose(char *file, int32 line, char *func, FILE *f, char *filename);
FILE *xfopen(char *file, int32 line, char *func, char *filename,
             char *mode);
#if OS_WINDOWS
void windows_set_errno(DWORD error_code);
#endif

#if OS_UNIX
void xdup2(int fd1, int fd2);
void xkill(pid_t pid, int signum);
void xpipe(int array[2]);
void xpthread_cond_destroy(pthread_cond_t *);
void xpthread_create(
    pthread_t *thread,
    pthread_attr_t *attr,
    void *(*function)(void *arg),
    void *arg
);
void xpthread_join(pthread_t *thread, void **thread_return);
void xpthread_mutex_destroy(pthread_mutex_t *);
void xpthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr);
void xpthread_mutex_lock(pthread_mutex_t *mutex) ATTR_EXCLUSIVE_LOCK(*mutex);
void xpthread_mutex_unlock(pthread_mutex_t *mutex) ATTR_UNLOCK(*mutex);
#endif

int xunlink(char *filename);
bool xregular_file_exists(char *path);
void test_make_temp_dir(char *buffer, int32 capacity, char *name);
void test_remove_tree(char *path);
void test_join_path(char *buffer, int64 buffer_len, char *dir, char *name);

#if OS_UNIX
bool test_command_exists(char *command);
bool test_hardlink_supported(char *dir);
bool test_symlink_supported(char *dir);
#endif
void here_impl(char *file, int32 line, char *func);

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

#define strequal2_3(A, A_LEN, B)        strequal2(A, A_LEN, B, STRLIT_LEN(B))
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

#define MEM_LITERAL_SHORT_LENGTHS(XX) \
    XX(2),                            \
    XX(3),                            \
    XX(4),                            \
    XX(5),                            \
    XX(6),                            \
    XX(7),                            \
    XX(8),                            \
    XX(9),                            \
    XX(10),                           \
    XX(11),                           \
    XX(12),                           \
    XX(13),                           \
    XX(14),                           \
    XX(15)

#define MEM_LITERAL_SHORT_GENERIC_SLOT(N) \
    char (*)[N]: CAT(mem_literal_short_, N)

#define MEM_LITERAL_SHORT(HAYSTACK, HAYSTACK_LEN, LITERAL) \
_Generic((char (*)[STRLIT_LEN(LITERAL)])0, \
    MEM_LITERAL_SHORT_LENGTHS(MEM_LITERAL_SHORT_GENERIC_SLOT), \
    default: memmem64 \
)(HAYSTACK, HAYSTACK_LEN, LITERAL, STRLIT_LEN(LITERAL))

#define BEGINS_WITH_3(STRING, STRING_LEN, PREFIX) \
    begins_with(STRING, STRING_LEN, PREFIX, STRLIT_LEN(PREFIX))
#define BEGINS_WITH_4(STRING, STRING_LEN, PREFIX, PREFIX_LEN) \
    begins_with(STRING, STRING_LEN, PREFIX, PREFIX_LEN)
#define BEGINS_WITH(...) SELECT_ON_NUM_ARGS(BEGINS_WITH_, __VA_ARGS__)

#define ENDS_WITH_3(STRING, STRING_LEN, SUFFIX) \
    ends_with(STRING, STRING_LEN, SUFFIX, STRLIT_LEN(SUFFIX))
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
    sb_append(BUILDER, STRING, STRLIT_LEN(STRING))
#define SB_APPEND_3(BUILDER, STRING, LEN) \
    sb_append(BUILDER, STRING, LEN)
#define SB_APPEND(...) SELECT_ON_NUM_ARGS(SB_APPEND_, __VA_ARGS__)

#define HERE here_impl(__FILE__, __LINE__, FUNC__)

#define NCALLS(INTERVAL) do {                                            \
    static int64 ncalls_ncalls = 1;                                      \
    if ((ncalls_ncalls % (INTERVAL)) == 0) {                             \
        fprintf(stderr, "%s:%d:%s: called %lld times\n",                 \
                        __FILE__, __LINE__, FUNC__, ncalls_ncalls);      \
    }                                                                    \
    ncalls_ncalls += 1;                                                  \
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
    XX(COMMAND_CAPTURE_STDOUT)      \
    XX(COMMAND_CAPTURE_STDERR)      \
    XX(COMMAND_MERGE_STDERR)        \
    XX(COMMAND_ASYNC)               \
    XX(COMMAND_DETACHED)            \
    XX(COMMAND_NEW_SESSION)         \
    XX(COMMAND_NEW_PROCESS_GROUP)   \
    XX(COMMAND_STDIN_TTY)           \
    XX(COMMAND_CLOSE_STDIN)
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

void command_argv0_set(Command *command, char *argument);
void command_child_env_apply(Command *);
noreturn void command_child_exec(
    Command *command,
    enum CommandFlag flags,
    int stdin_pipe[2],
    int stdout_pipe[2],
    int stderr_pipe[2]
);
#if OS_WINDOWS
void command_windows_command_line(Command *command, char *cmdline,
                                  int64 cmdline_len);
char *command_windows_argv0(Command *command, char *argv0_windows,
                            int32 *argv0_len);
int32 command_windows_run_process(Command *command, enum CommandFlag flags);
#endif
void command_cwd_clear(Command *);
void command_cwd_set(Command *command, char *cwd);
void command_env_clear(Command *);
void command_env_printf(Command *command, char *fmt, ...);
void command_env_push(Command *command, char *assignment);
void command_env_push_length(Command *command, char *assignment,
                             int32 assignment_len);
void command_error_set(Command *command, int32 error_status);
bool command_flags_capture(enum CommandFlag);
enum CommandFlag command_flags_normalized(enum CommandFlag);
void command_free(Command *);
void command_print(Command *);
void command_printf(Command *command, char *fmt, ...);
void command_push_length(Command *command, char *argument,
                         int32 argument_len);
void command_push_array(Command *command, int32 argc, char **argv);
void command_push_owned_length(
    char ***items,
    int32 **item_lens,
    int32 *len,
    int32 *cap,
    char *argument,
    int32 argument_len
);
void command_push_split(Command *command, char *arguments, char *delimiters);
int32 command_stdin_buffer_set(Command *command, char *data, int64 data_len);
void command_stdin_buffer_clear(Command *);
void command_reset(Command *);
void command_result_append(
    StrBuilder *output,
    StrBuilder *stdout_output,
    StrBuilder *stderr_output,
    bool is_stderr,
    char *data,
    int32 data_len
);
void command_result_file_descriptors_close(CommandResult *);
void command_result_free(CommandResult *);
void command_result_init(CommandResult *);
void command_result_read_captured(Command *);
void command_result_process_io(Command *command, enum CommandFlag flags);
int32 command_run(Command *command, enum CommandFlag flags);
int32 command_run_async(Command *command, enum CommandFlag flags);
int32 command_run_capture(Command *command, enum CommandFlag flags);
int32 command_run_capture_all(Command *);
int32 command_run_capture_combined(Command *);
int32 command_run_sync(Command *command, int *exit_status);
int32 command_signal(Command *command, int32 signal_number,
                     bool process_group);
int32 command_start(Command *command, enum CommandFlag flags);
int32 command_status_from_wait(int status, CommandResult *result);
char *command_str(Command *command, int32 *len);
void command_vector_reserve(char ***items, int32 **item_lens, int32 *cap,
                            int32 len, int32 extra);
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

void *generic_array_init(int32 cap, int64 item_size);
void *generic_array_grow(void *array, int64 item_size);
bool generic_array_reserve(void **array, int32 needed_count,
                           int64 item_size);
int32 generic_array_capacity(void *array);
void generic_array_set_count(void *array, int32 count);

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

#include "some_math.c"
#include "format.c"

#define ENUM_NAME CommandFlag
#define ENUM_BITFLAGS 1
#define ENUM_PREFIX_ COMMAND_
#define ENUM_UNDERLYING_TYPE uint32
#define ENUM_FIELDS \
    XX(COMMAND_CAPTURE_STDOUT)      \
    XX(COMMAND_CAPTURE_STDERR)      \
    XX(COMMAND_MERGE_STDERR)        \
    XX(COMMAND_ASYNC)               \
    XX(COMMAND_DETACHED)            \
    XX(COMMAND_NEW_SESSION)         \
    XX(COMMAND_NEW_PROCESS_GROUP)   \
    XX(COMMAND_STDIN_TTY)           \
    XX(COMMAND_CLOSE_STDIN)
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
