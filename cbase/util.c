// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(UTIL_C)
#define UTIL_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_util 1
#elif !defined(TESTING_util)
#define TESTING_util 0
#endif

#include "cbase.h"

void
here_impl(char *file, int32 line, char *func) {
    static llong here_counter = 0;
#if OS_UNIX
    char buffer[4096];
#endif

    error2("\n======== HERE(%lld): %s:%d:%s()\n",
           here_counter++, file, line, func);
#if OS_UNIX
    SNPRINTF(buffer, "%s:%d:%s\n", file, line, func);
    switch (fork()) {
    case -1:
        error("Error forking: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    case 0:
        execlp("dunstify", "dunstify", program, buffer, NULL);
        error("Error executing dunstify: %s.\n", strerror(errno));
        exit(EXIT_FAILURE);
    default:
        break;
    }
#endif
    return;
}

#define CLAMP_TYPE double
#include "clamp.h"

#define CLAMP_TYPE int64
#include "clamp.h"

#define CLAMP_TYPE int32
#include "clamp.h"

static char *notifiers[2] = {"dunstify", "notify-send"};

#if OS_UNIX
int
fdtruncate64(int32 fd, int64 len) {
    off_t len_offt;
    if (len >= MAXOF(len_offt)) {
        error("ftruncate with length bigger than off_t supports.\n");
        fatal(EXIT_FAILURE);
    }
    len_offt = (off_t)len;
    return ftruncate(fd, len_offt);
}
#endif

#if !CBASE_HAS_SYSTEM_MEMMEM
static void *
cbase_memmem_fallback(void *haystack, size_t haystack_len,
                      void *needle, size_t needle_len) {
    uchar *h = haystack;
    uchar *n = needle;
    uchar *end;
    uchar *limit;

    if (needle_len == 0) {
        return haystack;
    }
    if ((haystack == NULL) || (needle == NULL)) {
        return NULL;
    }
    if (haystack_len < needle_len) {
        return NULL;
    }

    end = h + haystack_len;
    limit = end - needle_len + 1;

    while (h < limit) {
        uchar *p;

        if ((p = memchr64(h, n[0], limit - h)) == NULL) {
            return NULL;
        }

        if (memcmp64(p, n, (int64)needle_len) == 0) {
            return (void *)p;
        }
        h = p + 1;
    }

    return NULL;
}
#endif

void *
memrchr64(void *pointer, int32 value, int64 size) {
    uchar *buffer;
    uchar target;

    if (DEBUGGING) {
        if (size < 0) {
            error("Error: Invalid size = %lld\n", size);
            fatal(EXIT_FAILURE);
        }
    }
    if (size == 0) {
        return NULL;
    }

    buffer = pointer;
    target = (uchar)value;
    for (int64 i = size - 1; i >= 0; i -= 1) {
        if (buffer[i] == target) {
            return buffer + i;
        }
    }

    return NULL;
}

void *
memmem64(void *haystack, int64 haystack_len, void *needle, int64 needle_len) {
    void *result;

    if (haystack_len <= 0) {
        return NULL;
    }
    if (needle_len <= 0) {
        return NULL;
    }
    if (needle_len > haystack_len) {
        return NULL;
    }


    ASSERT(haystack != NULL);
    ASSERT(needle != NULL);

#if CBASE_HAS_SYSTEM_MEMMEM
    result = memmem(haystack, (size_t)haystack_len, needle, (size_t)needle_len);
#else
    result = cbase_memmem_fallback(haystack, (size_t)haystack_len,
                                   needle, (size_t)needle_len);
#endif
    return result;
}

#if !HAS_POSIX_WIN_SUBSET
char *optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = 0;

int
getopt_long(
    int argc,
    char **argv,
    char *optstring,
    struct option *longopts,
    int *longindex
) {
    static char *short_position;
    char *arg;
    char *opt;

    optarg = NULL;
    if (short_position && *short_position) {
        opt = memchr64(optstring, *short_position, strlen32(optstring));
        optopt = *short_position++;
        if ((opt == NULL) || (*opt == ':')) {
            return '?';
        }
        if (opt[1] == ':') {
            if (*short_position) {
                optarg = short_position;
                short_position = NULL;
            } else if ((optind + 1) < argc) {
                optarg = argv[++optind];
                short_position = NULL;
            } else {
                short_position = NULL;
                return '?';
            }
        }
        if ((short_position == NULL) || (*short_position == '\0')) {
            optind += 1;
            short_position = NULL;
        }
        return optopt;
    }

    if (optind >= argc) {
        return -1;
    }
    arg = argv[optind];
    if ((arg[0] != '-') || (arg[1] == '\0')) {
        return -1;
    }
    if (strcmp(arg, "--") == 0) {
        optind += 1;
        return -1;
    }

    if (arg[1] == '-') {
        char *name = arg + 2;
        int32 arg_name_len = strlen32(name);
        char *value = memchr64(name, '=', arg_name_len);
        int64 name_len;

        if (value) {
            name_len = value - name;
            value += 1;
        } else {
            name_len = arg_name_len;
        }

        for (int32 i = 0; longopts[i].name; i += 1) {
            if ((strlen32(longopts[i].name) != name_len)
                || memcmp64(longopts[i].name, name, name_len)) {
                continue;
            }
            if (longindex) {
                *longindex = i;
            }
            if (longopts[i].has_arg == required_argument) {
                if (value) {
                    optarg = value;
                } else if ((optind + 1) < argc) {
                    optarg = argv[++optind];
                } else {
                    optopt = longopts[i].val;
                    optind += 1;
                    return '?';
                }
            } else if (longopts[i].has_arg == optional_argument) {
                optarg = value;
            } else if (value) {
                optopt = longopts[i].val;
                optind += 1;
                return '?';
            }
            optind += 1;
            if (longopts[i].flag) {
                *longopts[i].flag = longopts[i].val;
                return 0;
            }
            return longopts[i].val;
        }

        optind += 1;
        return '?';
    }

    short_position = arg + 1;
    return getopt_long(argc, argv, optstring, longopts, longindex);
}
#endif

void *
memchr64(void *pointer, int32 value, int64 size) {
    if (DEBUGGING) {
        if (size < 0) {
            error("Error: Invalid size = %lld\n", size);
            fatal(EXIT_FAILURE);
        }
    }
    if (size == 0) {
        return 0;
    }
    return memchr(pointer, value, (size_t)size);
}

int
memcmp64(void *left, void *right, int64 size) {
    if (size == 0) {
        return 0;
    }
    if (DEBUGGING) {
        if ((left == NULL) || (right == NULL)) {
            TRAP();
        }
        if (size < 0) {
            error("Error: size=%lld < 0.\n", size);
        }
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error: Size (%lld) is bigger than SIZEMAX\n", size);
            fatal(EXIT_FAILURE);
        }
    }
    return memcmp(left, right, (size_t)size);
}

bool
util_glob_match(char *string, int32 string_len, char *glob, int32 glob_len) {
    int32 string_i = 0;
    int32 glob_i = 0;
    int32 star_glob_i = -1;
    int32 star_string_i = 0;

    if (DEBUGGING) {
        if (string_len < 0) {
            error("Invalid string len = %d\n", string_len);
            fatal(EXIT_FAILURE);
        }
        if (string_len < 0) {
            error("Invalid glob len = %d\n", glob_len);
            fatal(EXIT_FAILURE);
        }

        if ((string == NULL) && (string_len > 0)) {
            error("Error: string is NULL but length is positive.\n");
            fatal(EXIT_FAILURE);
        }
        if ((glob == NULL) && (glob_len > 0)) {
            error("Error: glob is NULL but length is positive.\n");
            fatal(EXIT_FAILURE);
        }
    }

    while (string_i < string_len) {
        if ((glob_i < glob_len) && (glob[glob_i] == '*')) {
            star_glob_i = glob_i;
            glob_i += 1;
            star_string_i = string_i;
            continue;
        }
        if ((glob_i < glob_len) && (glob[glob_i] == string[string_i])) {
            glob_i += 1;
            string_i += 1;
            continue;
        }
        if (star_glob_i >= 0) {
            glob_i = star_glob_i + 1;
            star_string_i += 1;
            string_i = star_string_i;
            continue;
        }
        return false;
    }

    while ((glob_i < glob_len) && (glob[glob_i] == '*')) {
        glob_i += 1;
    }

    return glob_i == glob_len;
}

static uint64 rand_int_state = 0x853c49e6748fea9bull;

void
rand_int_seed(uint64 seed) {
    if (seed == 0) {
        seed = 0x853c49e6748fea9bull;
    }
    rand_int_state = seed;

    return;
}

int32
rand_int(void) {
    uint64 old_state = rand_int_state;
    uint32 xorshifted;
    uint32 rot;
    uint32 result;

    rand_int_state = old_state*6364136223846793005ull
                     + 1442695040888963407ull;
    xorshifted = (uint32)(((old_state >> 18u) ^ old_state) >> 27u);
    rot = (uint32)(old_state >> 59u);
    result = (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));

    return (int32)(result >> 1);
}

int32
rand_int_range(int32 upper_bound) {
    int64 limit;
    int32 value;

    if (upper_bound <= 1) {
        return 0;
    }

    limit = (int64)INT32_MAX + 1;
    limit -= limit % upper_bound;
    do {
        value = rand_int();
    } while ((int64)value >= limit);

    return value % upper_bound;
}

void
rand_shuffle(void *items, int32 item_count, int32 item_size) {
    char *bytes = items;

    if (item_count <= 1) {
        return;
    }

    ASSERT(items != NULL);
    ASSERT_POSITIVE(item_size);

    for (int32 i = item_count - 1; i > 0; i -= 1) {
        int32 j = rand_int_range(i + 1);

        if (j != i) {
            char *left = bytes + i*item_size;
            char *right = bytes + j*item_size;

            for (int32 k = 0; k < item_size; k += 1) {
                char tmp = left[k];

                left[k] = right[k];
                right[k] = tmp;
            }
        }
    }
    return;
}

void
random_filename_inplace(char *buffer, int32 buffer_len) {
    char allowed[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                     "abcdefghijklmnopqrstuvwxyz"
                     "!@#$%&*()[]-=_+<>,"
                     "0123456789";

    for (int32 i = 0; i < buffer_len; i += 1) {
        int32 j = rand_int() % (SIZEOF(allowed) - 1);
        buffer[i] = allowed[j];
    }

    return;
}

void
qsort64(void *base, int64 n, int64 size, int (*compar)(void *, void *)) {
    int (*compar_consted)(const void *, const void *);
    compar_consted = (int (*)(const void *, const void *)) compar;

    if (n == 0) {
        return;
    }
    if (DEBUGGING) {
        if (size <= 0) {
            error("Error: invalid object size = %lld.\n", size);
            fatal(EXIT_FAILURE);
        }
        if (n < 0) {
            error("Error: invalid object count = %lld.\n", n);
            fatal(EXIT_FAILURE);
        }
        if ((size_t)size >= (SIZE_MAX / (size_t)n)) {
            error("Error: Overflow (size=%lld*%lld=n)\n", size, n);
            fatal(EXIT_FAILURE);
        }
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error: Size (%lld) is bigger than SIZEMAX\n", size);
            fatal(EXIT_FAILURE);
        }
        if ((ullong)n >= (ullong)SIZE_MAX) {
            error("Error: Number (%lld) is bigger than SIZEMAX\n", n);
            fatal(EXIT_FAILURE);
        }
    }

    qsort(base, (size_t)n, (size_t)size, compar_consted);
    return;
}

int32 ATTR_PRINTF(3, 4)
snprintf2(char *buffer, int64 size, char *format, ...) {
    int n;
    va_list args;

    ASSERT_NON_NEGATIVE(size);
    va_start(args, format);
    n = vsnprintf(buffer, (size_t)size, format, args);
    va_end(args);

    if ((n < 0) || (n >= size)) {
        error2("Error in vsnprintf(\"%s\") (n = %d)\n", format, n);
        fatal(EXIT_FAILURE);
    }
    return n;
}

int32
itoa2(char *buffer, int32 size, llong num) {
    ullong magnitude;
    int i = 0;
    bool negative = false;

    ASSERT(size >= 22);

    if (num < 0) {
        negative = true;
        magnitude = (ullong)(-(num + 1)) + 1;
    } else {
        magnitude = (ullong)num;
    }

    do {
        buffer[i] = (char)(magnitude % 10 + '0');
        i += 1;
        magnitude /= 10;
    } while (magnitude > 0);

    if (negative) {
        buffer[i] = '-';
        i += 1;
    }

    buffer[i] = '\0';

    for (long j = 0; j < i / 2; j += 1) {
        char temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }

    // this is here because of gcc -fanalyzer
    ASSERT(i < 22);

    return i;
}

// high level, returns negative on failure
int32
parse_integer(char *str, int32 str_len, llong *result) {
    int32 i = 0;
    llong value = 0;
    llong limit = -LLONG_MAX;
    bool negative = false;
    bool has_digit = false;

    if ((str == NULL) || (result == NULL) || (str_len < 0)) {
        return -EINVAL;
    }

    while ((i < str_len)
           && ((str[i] == ' ') || (str[i] == '\f') || (str[i] == '\n')
               || (str[i] == '\r') || (str[i] == '\t')
               || (str[i] == '\v'))) {
        i += 1;
    }

    if ((i < str_len) && ((str[i] == '-') || (str[i] == '+'))) {
        negative = str[i] == '-';
        if (negative) {
            limit = LLONG_MIN;
        }
        i += 1;
    }

    while ((i < str_len) && (str[i] >= '0') && (str[i] <= '9')) {
        llong digit = str[i] - '0';

        has_digit = true;
        if (value < (limit + digit)/10) {
            return -ERANGE;
        }
        value = value*10 - digit;
        i += 1;
    }

    if (!has_digit) {
        return -EINVAL;
    }

    while ((i < str_len)
           && ((str[i] == ' ') || (str[i] == '\f') || (str[i] == '\n')
               || (str[i] == '\r') || (str[i] == '\t')
               || (str[i] == '\v'))) {
        i += 1;
    }
    if (i < str_len) {
        return -EINVAL;
    }

    if (negative) {
        *result = value;
    } else {
        *result = -value;
    }
    return 0;
}

// low level without error checking, returns 0 on invalid input.
// only to be used in the following situations:
// - when the string was pre-parsed,
//   so we know that it will not get invalid input
// - or when the caller only needs positive values;
//   in this case, zero is used as one of:
//   - "don't use this number"
//   - "do 0 actions of this thing"
//   - "do this forever, don't limit it"
llong
atoi2(char *str, int32 str_len) {
    int32 i = 0;
    llong value = 0;
    llong limit = -MAXOF(value);
    bool negative = false;

    if ((str == NULL) || (str_len <= 0)) {
        return 0;
    }

    if ((i < str_len) && ((str[i] == '-') || (str[i] == '+'))) {
        negative = str[i] == '-';
        if (negative) {
            limit = MINOF(value);
        }
        i += 1;
    }

    (void)limit;

    while ((i < str_len) && (str[i] >= '0') && (str[i] <= '9')) {
        llong digit = str[i] - '0';

        if (DEBUGGING) {
            if (value < (limit + digit)/10) {
                TRAP("overflow");
            }
        }
        value = value*10 - digit;
        i += 1;
    }

    if (negative) {
        return value;
    }
    return -value;
}

// Like atoi2, but saturates on overflow instead of trapping.
llong
atoi2sat(char *str, int32 str_len) {
    int32 i = 0;
    llong value = 0;
    llong limit = -MAXOF(value);
    bool negative = false;

    if ((str == NULL) || (str_len <= 0)) {
        return 0;
    }

    if ((i < str_len) && ((str[i] == '-') || (str[i] == '+'))) {
        negative = str[i] == '-';
        if (negative) {
            limit = MINOF(value);
        }
        i += 1;
    }

    while ((i < str_len) && (str[i] >= '0') && (str[i] <= '9')) {
        llong digit = str[i] - '0';

        if (value < (limit + digit)/10) {
            if (negative) {
                return MINOF(value);
            } else {
                return MAXOF(value);
            }
        }
        value = value*10 - digit;
        i += 1;
    }

    if (negative) {
        return value;
    }
    return -value;
}

void ATTR_PRINTF(4, 5)
error_impl(char *file, int32 line, char *func, char *format, ...) {
    char buffer[BUFSIZ];
    char *big_buffer = NULL;
    char *pbuffer = buffer;
    va_list args;
    va_list args_copy;
    int32 n;
    int32 m = SIZEOF(buffer);
    int32 p;
    char fileline[256];
    char file2[4096];
    int32 file_len = strlen32(file);
    int32 base_len;

    if (file_len >= SIZEOF(file2)) {
        error2("File name is too long.\n");
        fatal(EXIT_FAILURE);
    }
    memcpy(file2, file, (size_t)(file_len + 1));

    file = basename2(file2, &file_len, &base_len);

    va_start(args, format);
    va_copy(args_copy, args);
    n = vsnprintf(buffer, (size_t)m, format, args_copy);
    va_end(args_copy);

    if (n >= m) {
        m = n + 1;
        big_buffer = xmalloc(m, false);
        n = vsnprintf(big_buffer, (size_t)m, format, args);
        pbuffer = big_buffer;
    }

    va_end(args);

    if ((n < 0) || (n >= m)) {
        error2("%s:%d:%s(): Error in vsnprintf(\"%s\") (n = %d).\n",
               file, line, func, format, n);
        fatal(EXIT_FAILURE);
    }

    if (DEBUGGING) {
        p = SNPRINTF(fileline, "%s:%d %s():", file, line, func);
    } else {
        p = 0;
    }

    if (p) {
        write_all(STDERR_FILENO, fileline, p);
    }
    write_all(STDERR_FILENO, pbuffer, n);
#if OS_UNIX
    fsync(STDERR_FILENO);
    fsync(STDOUT_FILENO);
#endif

#if ERROR_NOTIFY
#if OS_WINDOWS
#error "ERROR_NOTIFY is defined but unsupported for windows."
#endif
    switch (fork()) {
    case 0:
        for (uint32 i = 0; i < LENGTH(notifiers); i += 1) {
            execlp(notifiers[i],
                   notifiers[i], "-u", "critical", program, pbuffer, NULL);
        }
        error2("Error executing notifier: %s.\n", strerror(errno));
        exit(EXIT_FAILURE);
    case -1:
        error2("Error forking: %s.\n", strerror(errno));
        break;
    default:
        break;
    }
#endif

    free2(big_buffer, m);
    return;
}

void
error_async_safe(char *message) {
    int32 len = strlen32(message);
    write_all(STDERR_FILENO, message, len);
    return;
}

noreturn void
fatal(int status) {
#if defined(__EMSCRIPTEN__)
    char stack[8192];
    int32 flags = EM_LOG_C_STACK
                  |EM_LOG_JS_STACK
                  |EM_LOG_NO_PATHS;

    emscripten_get_callstack(flags, stack, SIZEOF(stack));
    error2("fatal(%d) call stack:\n%s\n", status, stack);
#endif
    if (DEBUGGING) {
        (void)status;
        raise(SIGILL);
        exit(status);
    } else {
        exit(status);
    }
}

#if OS_UNIX
void
util_segv_handler(int32 unused) {
    char *message = "Memory error. Please send a bug report.\n";
    (void)unused;

    write64(STDERR_FILENO, message, strlen32(message));
    for (uint32 i = 0; i < LENGTH(notifiers); i += 1) {
        execlp(notifiers[i],
               notifiers[i], "-u", "critical", program, message, NULL);
    }
    _exit(EXIT_FAILURE);
}

#endif

int32
util_string_int32(int32 *number, char *string) {
    char *endptr;
    long x;
    errno = 0;
    x = strtol(string, &endptr, 10);
    if ((errno != 0) || (string == endptr) || (*endptr != 0)) {
        return -1;
    } else if ((x > INT32_MAX) || (x < INT32_MIN)) {
        return -1;
    } else {
        *number = (int32)x;
        return 0;
    }
}

#if CBASE_HAS_PROCFS
void
send_signal(char *executable, int32 signal_number) {
    DIR *processes;
    struct dirent *process;
    int64 len = strlen32(executable);
    StrBuilder buffer = {0};
    sb_reserve(&buffer, 256);

    if ((processes = opendir("/proc")) == NULL) {
        error("Error opening /proc: %s\n", strerror(errno));
        sb_free(&buffer);
        return;
    }

    while ((process = readdir(processes))) {
        char command[256];
        int32 pid;
        int32 cmdline;
        int64 r;
        char *last;
        int32 d_name_len;

#if CBASE_DIRENT_HAS_D_TYPE
        if ((process->d_type != DT_DIR) && (process->d_type != DT_UNKNOWN)) {
            continue;
        }
#endif
        if ((pid = atoi(process->d_name)) <= 0) {
            continue;
        }

        sb_clear(&buffer);
        d_name_len = strlen32(process->d_name);

        SB_APPEND(&buffer, "/proc/");
        SB_APPEND(&buffer, process->d_name, d_name_len);
        SB_APPEND(&buffer, "/cmdline");

        if ((cmdline = open(buffer.data, O_RDONLY)) < 0) {
            continue;
        }

        errno = 0;
        if ((r = read64(cmdline, command, sizeof(command))) <= 0) {
            XCLOSE(&cmdline, buffer.data);
            continue;
        }
        XCLOSE(&cmdline, buffer.data);

        if (memmem64(command, r, executable, len)) {
            if ((last = memchr64(command, '\0', r))) {
                r = last - command;
                if (!memmem64(command, r, executable, len)) {
                    continue;
                }
            }
            if (kill(pid, signal_number) < 0) {
                error("Error sending signal %d to program %s (pid %d): %s.\n",
                      signal_number, executable, pid, strerror(errno));
            } else {
                if (DEBUGGING) {
                    error("Sended signal %d to program %s (pid %d).\n",
                          signal_number, executable, pid);
                }
            }
        }
    }

    sb_free(&buffer);
    xclosedir(processes, "/proc");
    return;
}
#elif OS_UNIX
void
send_signal(char *executable, int32 signal_number) {
    pid_t child;
    char signal_string[14];
    SNPRINTF(signal_string, "%d", signal_number);

    switch (child = fork()) {
    case -1:
        error("Error forking: %s\n", strerror(errno));
        return;
    case 0:
        execlp("pkill", "pkill", signal_string, executable, NULL);
        error("Error executing pkill: %s\n", strerror(errno));
        fatal(EXIT_FAILURE);
    default:
        while (waitpid(child, NULL, 0) < 0) {
            if (errno == EINTR) {
                continue;
            }
            error2("Error waiting for child: %s.\n", strerror(errno));
            fatal(EXIT_FAILURE);
        }
    }
    return;
}
#endif

double
rad2deg(double radians) {
    double RAD2DEG = 180.0 / 3.141592653589793;
    return radians*RAD2DEG;
}

double
deg2rad(double degrees) {
    double DEG2RAD = 3.141592653589793 / 180.0;
    return degrees*DEG2RAD;
}

int32
bytes_pretty(char *buffer, int64 raw) {
    char *suffixes[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    double aux_pretty;
    int64 i;
    int32 n;
    char *comma;

    if (raw < 0) {
        *buffer = '\0';
        return 0;
    }

    if (raw <= 1023) {
        n = snprintf2(buffer, 16, "%lldB", raw);
        return n;
    }

    aux_pretty = (double)raw;
    i = 0;
    while ((aux_pretty >= 1024.0) && (i < LENGTH(suffixes))) {
        aux_pretty /= 1024.0;
        i += 1;
    }

    if (aux_pretty >= 1000) {
        n = snprintf2(buffer, 16, "%.1f%s", aux_pretty, suffixes[i]);
    } else if (aux_pretty >= 100) {
        n = snprintf2(buffer, 16, "%.2f%s", aux_pretty, suffixes[i]);
    } else if (aux_pretty >= 10) {
        n = snprintf2(buffer, 16, "%.3f%s", aux_pretty, suffixes[i]);
    } else {
        n = snprintf2(buffer, 16, "%.4f%s", aux_pretty, suffixes[i]);
    }

    if ((comma = memchr64(buffer, ',', n))) {
        *comma = '.';
    }

    return n;
}

#if !OS_WINDOWS
#if OS_UNIX

void
xpipe(int array[2]) {
    if (pipe(array) < 0) {
        error("Error creating pipe: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    }
    return;
}

void
xdup2(int fd1, int fd2) {
    if (dup2(fd1, fd2) < 0) {
        error("Error in dup2: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    }
    return;
}

void
xkill(pid_t pid, int signum) {
    if (kill(pid, signum) < 0) {
        error("Error sending signal %d=%s to %d: %s.\n",
              signum, signal_names[signum], pid, strerror(errno));
    }
    return;
}

#endif /* OS_UNIX */
#endif /* !OS_WINDOWS */

int32
parse_option(char **parsed, char *arg, char *option_name) {
    char name_equal[256];
    char *tmp;
    int32 length = SNPRINTF(name_equal, "%s=", option_name);
    int32 arg_len;
    if (arg == NULL) {
        return -1;
    }
    arg_len = strlen32(arg);

    if ((tmp = BEGINS_WITH(arg, arg_len, name_equal, length))) {
        *parsed = tmp;
        return 0;
    }
    return -1;
}

bool
is_ident_start_char(char c) {
    return isalpha((uint8)c) || c == '_';
}

bool
is_ident_char(char c) {
    return isalnum((uint8)c) || c == '_';
}

void
warn(char *fmt, ...) {
    va_list ap;

    error2("%s: "RED ("warning:"), program);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    error2( "\n");

    return;
}

bool
util_is_integer(char *string) {
    char c;

    while ((c = *string)) {
        if (!isdigit(c)) {
            return false;
        }
        string += 1;
    }

    return true;
}

#if 0 == TESTING_util
static inline void
util_functions_sink(void) {
    (void)util_functions_sink;
    (void)rand_int_seed;
    (void)rand_int;
    (void)rand_int_range;
    (void)rand_shuffle;
    (void)random_filename_inplace;
    (void)util_is_integer;
    (void)util_glob_match;
    (void)is_ident_start_char;
    (void)warn;
    (void)here_impl;
    (void)command_result_free;
    (void)command_argv0_set;
    (void)command_free;
    (void)command_printf;
    (void)command_push_length;
    (void)command_push_array;
    (void)command_push_split;
    (void)command_env_push;
    (void)command_env_push_length;
    (void)command_env_printf;
    (void)command_env_clear;
    (void)command_cwd_set;
    (void)command_cwd_clear;
    (void)command_run;
    (void)command_run_async;
    (void)command_run_capture_all;
    (void)command_run_capture_combined;
    (void)util_string_int32;
#if OS_UNIX
    (void)util_segv_handler;
    (void)send_signal;
#endif

    (void)malloc_debug;
    (void)realloc_debug;
    (void)free_debug;

    (void)atoi2;
    (void)atoi2sat;
#if OS_UNIX
    (void)command_run_capture;
    (void)command_run_sync;
    (void)command_result_read_captured;
    (void)command_signal;
    (void)command_wait;
#endif
    (void)bytes_pretty;
    (void)qsort64;

    (void)xmmap_commit;
    (void)xstrdup;
#if OS_UNIX
    (void)xkill;
    (void)xdup2;
    (void)xpipe;
#endif
    (void)xmemdup;

    (void)rad2deg;
    (void)deg2rad;
    (void)parse_option;
    (void)command_print;
    return;
}
#endif

#if TESTING
#if OS_UNIX
bool
test_command_exists(char *command) {
    char *path;
    int32 command_len;
    int32 path_len;

    if ((command == NULL) || (command[0] == '\0')) {
        return false;
    }

    command_len = strlen32(command);
    if (memchr64(command, '/', command_len) != NULL) {
        return access(command, X_OK) == 0;
    }

    path = getenv("PATH");
    if (path == NULL) {
        return false;
    }

    path_len = strlen32(path);
    for (int32 start = 0; start <= path_len; start += 1) {
        char candidate[PATH_MAX];
        int32 len;
        int32 end;

        end = start;
        while ((end < path_len) && (path[end] != ':')) {
            end += 1;
        }

        if (end == start) {
            len = snprintf2(candidate, SIZEOF(candidate), "./%s", command);
        } else {
            len = snprintf2(candidate, SIZEOF(candidate), "%.*s/%s",
                            end - start, path + start, command);
        }
        if ((len > 0) && (len < SIZEOF(candidate))
            && (access(candidate, X_OK) == 0)) {
            return true;
        }

        start = end;
    }

    return false;
}
#endif

#endif

#if TESTING_util
#define CBASE_IMPLEMENT
#include "cbase.h"

#define ENUM_NAME WeekDay
#define ENUM_BITFLAGS 0
#define ENUM_PREFIX_ WEEK_DAY_
#define ENUM_FIELDS \
    XX(WEEK_DAY_SUNDAY, sunday)    \
    XX(WEEK_DAY_MONDAY, monday)    \
    XX(WEEK_DAY_TUESDAY, tuesday)  \
    XX(WEEK_DAY_WEDNESDAY)         \
    XX(WEEK_DAY_THURSDAY)          \
    XX(WEEK_DAY_FRIDAY, friday)    \
    XX(WEEK_DAY_SATURDAY, saturday)
#include "xenums.c"

#define ENUM_NAME PowerOfTwo
#define ENUM_BITFLAGS 1
#define ENUM_PREFIX_ POWER_OF2_
#define ENUM_FIELDS \
    XX(POWER_OF2_ONE)     \
    XX(POWER_OF2_TWO)     \
    XX(POWER_OF2_FOUR)    \
    XX(POWER_OF2_EIGHT)   \
    XX(POWER_OF2_SIXTEEN)
#include "xenums.c"

#if OS_LINUX
static sig_atomic_t received_signal = false;
static void
signal_handler(int signal_number) {
    (void)signal_number;
    received_signal = true;
    return;
}
#endif

static int
util_test_qsort_cmp(void *a, void *b) {
    int32 va = *(const int32 *)a;
    int32 vb = *(const int32 *)b;
    if (va < vb) {
        return -1;
    }
    if (va > vb) {
        return 1;
    }
    return 0;
}

#define ASSERT_MEM_LITERAL_OFFSET(HAYSTACK, HAYSTACK_LEN, LITERAL, OFFSET)     \
    do {                                                                       \
        char *mem_literal_haystack = (HAYSTACK);                               \
        char *mem_literal_actual;                                              \
        char *mem_literal_expected;                                            \
        int64 mem_literal_offset = (OFFSET);                                   \
        int64 mem_literal_haystack_len = (HAYSTACK_LEN);                       \
        if (mem_literal_offset < 0) {                                          \
            mem_literal_expected = NULL;                                       \
        } else {                                                               \
            mem_literal_expected = mem_literal_haystack + mem_literal_offset;  \
        }                                                                      \
        mem_literal_actual = MEM_LITERAL_SHORT(mem_literal_haystack,           \
                                               mem_literal_haystack_len,       \
                                               LITERAL);                       \
        ASSERT_EQUAL((void *)mem_literal_actual,                               \
                     (void *)mem_literal_expected);                            \
    } while (0)

#define ASSERT_MEM_LITERAL(HAYSTACK, LITERAL, OFFSET)                          \
    ASSERT_MEM_LITERAL_OFFSET(HAYSTACK, strlen32(HAYSTACK), LITERAL, OFFSET)

static void
util_test_mem_literal_short(void) {
    char binary_haystack[] = {'x', 'a', '\0', 'b', 'y'};

    ASSERT_NULL(MEM_LITERAL_SHORT(NULL, 2, "ab"));
    ASSERT_NULL(MEM_LITERAL_SHORT("", 0, "ab"));

    ASSERT_MEM_LITERAL("zzabyy", "ab", 2);
    ASSERT_MEM_LITERAL("zzabcyy", "abc", 2);
    ASSERT_MEM_LITERAL("zzabcdyy", "abcd", 2);
    ASSERT_MEM_LITERAL("zzabcdeyy", "abcde", 2);
    ASSERT_MEM_LITERAL("zzabcdefyy", "abcdef", 2);
    ASSERT_MEM_LITERAL("zzabcdefgyy", "abcdefg", 2);
    ASSERT_MEM_LITERAL("zzabcdefghyy", "abcdefgh", 2);
    ASSERT_MEM_LITERAL("zzabcdefghiyy", "abcdefghi", 2);
    ASSERT_MEM_LITERAL("zzabcdefghijyy", "abcdefghij", 2);
    ASSERT_MEM_LITERAL("zzabcdefghijkyy", "abcdefghijk", 2);
    ASSERT_MEM_LITERAL("zzabcdefghijklyy", "abcdefghijkl", 2);
    ASSERT_MEM_LITERAL("zzabcdefghijklmyy", "abcdefghijklm", 2);
    ASSERT_MEM_LITERAL("zzabcdefghijklmnyy", "abcdefghijklmn", 2);
    ASSERT_MEM_LITERAL("zzabcdefghijklmnoyy", "abcdefghijklmno", 2);

    ASSERT_MEM_LITERAL("abcd", "ab", 0);
    ASSERT_MEM_LITERAL("xxabcd", "abcd", 2);
    ASSERT_MEM_LITERAL("xxabcd", "cd", 4);
    ASSERT_MEM_LITERAL("abxabzabc", "abc", 6);

    ASSERT_MEM_LITERAL("xxxx", "ab", -1);
    ASSERT_MEM_LITERAL("xxab", "abc", -1);
    ASSERT_MEM_LITERAL_OFFSET("xxabc", 4, "abc", -1);

    ASSERT_MEM_LITERAL_OFFSET(binary_haystack, SIZEOF(binary_haystack),
                              "a\0b", 1);
    ASSERT_MEM_LITERAL_OFFSET(binary_haystack, 3, "a\0b", -1);

    ASSERT_MEM_LITERAL("zzabcdefghijklmnopqq", "abcdefghijklmnop", 2);
    ASSERT_MEM_LITERAL("zzabcdefghijklmnoxqq", "abcdefghijklmnop", -1);
    ASSERT_MEM_LITERAL_OFFSET("zzabcdefghijklmno",
                              STRLIT_LEN("zzabcdefghijklmno"),
                              "abcdefghijklmnop", -1);

    return;
}

#undef ASSERT_MEM_LITERAL
#undef ASSERT_MEM_LITERAL_OFFSET

int
main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    util_test_mem_literal_short();

    {
        ASSERT_GLOB_MATCH("", "");
        ASSERT_GLOB_MATCH("", "*");
        ASSERT_GLOB_MATCH("abc", "abc");
        ASSERT_GLOB_MATCH("abc", "*");
        ASSERT_GLOB_MATCH("abc", "a*");
        ASSERT_GLOB_MATCH("abc", "*c");
        ASSERT_GLOB_MATCH("abc", "a*c");
        ASSERT_GLOB_MATCH("abc", "a**c");
        ASSERT_GLOB_MATCH("abc", "*b*");
        ASSERT_GLOB_MATCH("abc", "a*b*c");
        ASSERT_GLOB_MATCH("abbc", "a*bc");
        ASSERT_GLOB_MATCH("abc", "abc*");
        ASSERT_GLOB_MATCH("abc", "*abc");

        ASSERT_GLOB_NO_MATCH("", "a");
        ASSERT_GLOB_NO_MATCH("abc", "");
        ASSERT_GLOB_NO_MATCH("abc", "abd");
        ASSERT_GLOB_NO_MATCH("abc", "a*d");
        ASSERT_GLOB_NO_MATCH("abc", "ab*d");
        ASSERT_GLOB_NO_MATCH("abc", "*d");
        ASSERT_GLOB_NO_MATCH("abc", "a*c*d");
    }

    {
        int a = 10;
        int b = 20;

        SWAP(a, b);
        ASSERT_EQUAL(a, 20);
        ASSERT_EQUAL(b, 10);

        ASSERT_EQUAL(ALIGN_POWER_OF_2(7, 16), 16);
        ASSERT_EQUAL(ALIGN_POWER_OF_2(16, 16), 16);
        ASSERT_EQUAL(ALIGN_POWER_OF_2(17, 16), 32);
        ASSERT_EQUAL(ALIGN16(7), 16);
    }

    for (enum WeekDay day = WEEK_DAY_MONDAY; day < WEEK_DAY_COUNT; day += 1) {
        printf("enum[%u] = %s\n", day, WEEK_DAY_str(day));
    }

    printf("\n");

    for (uint x = 0; x < POWER_OF2_LAST; x += 1) {
        char *value_name = POWER_OF2_str((enum PowerOfTwo)x);
        printf("enum[%u] = %s\n", x, value_name);
        POWER_OF2_str_free(value_name);
    }

#if OS_LINUX
    if (!DEBUGGING) {
        struct sigaction signal_action;
        signal_action.sa_handler = signal_handler;
        sigemptyset(&signal_action.sa_mask);
        signal_action.sa_flags = SA_RESTART;
        if (sigaction(SIGUSR1, &signal_action, NULL) != 0) {
            error2("Error in sigaction: %s.\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        send_signal(argv[0], SIGUSR1);
        ASSERT(received_signal);
    }
#endif

    rand_int_seed((uint64)time(NULL));
    for (int i = 0; i < 10; i += 1) {
        int n = rand_int() - INT32_MAX / 2;
        char itoa_buffer[32];
        int32 itoa_len = ITOA(itoa_buffer, n);
        ASSERT_EQUAL(atoi2(itoa_buffer, itoa_len), n);
    }

    {
        int32 values[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        bool seen[10] = {0};

        rand_int_seed(1234);
        for (int32 i = 0; i < 100; i += 1) {
            int32 value = rand_int_range(7);

            ASSERT_NON_NEGATIVE(value);
            ASSERT_LESS(value, 7);
        }

        rand_shuffle(values, LENGTH(values), SIZEOF(*values));
        for (int32 i = 0; i < LENGTH(values); i += 1) {
            ASSERT_NON_NEGATIVE(values[i]);
            ASSERT_LESS(values[i], LENGTH(values));
            ASSERT(!seen[values[i]]);
            seen[values[i]] = true;
        }
    }

    ASSERT_EQUAL(atoi2("-123x", 4), -123);
    ASSERT_EQUAL(atoi2("99", 1), 9);
    ASSERT_EQUAL(atoi2("42", 0), 0);
    ASSERT_EQUAL(atoi2(STRLIT("9223372036854775807")), LLONG_MAX);
    ASSERT_EQUAL(atoi2(STRLIT("-9223372036854775808")), LLONG_MIN);
#if OS_UNIX
    ASSERT_TRAPS(atoi2(STRLIT("9223372036854775808")));
    ASSERT_TRAPS(atoi2(STRLIT("-9223372036854775809")));
    ASSERT_TRAPS(atoi2(STRLIT("99999999999999999999999999")));
    ASSERT_TRAPS(atoi2(STRLIT("-1111111111111111111111111")));
#endif

    ASSERT_EQUAL(atoi2sat("-123x", 4), -123);
    ASSERT_EQUAL(atoi2sat("99", 1), 9);
    ASSERT_EQUAL(atoi2sat("42", 0), 0);
    ASSERT_EQUAL(atoi2sat(STRLIT("9223372036854775807")), LLONG_MAX);
    ASSERT_EQUAL(atoi2sat(STRLIT("-9223372036854775808")), LLONG_MIN);
    ASSERT_EQUAL(atoi2sat(STRLIT("9223372036854775808")), LLONG_MAX);
    ASSERT_EQUAL(atoi2sat(STRLIT("-9223372036854775809")), LLONG_MIN);
    ASSERT_EQUAL(atoi2sat(STRLIT("999999999999999999999999999999")),
                 LLONG_MAX);
    ASSERT_EQUAL(atoi2sat(STRLIT("-999999999999999999999999999999")),
                 LLONG_MIN);

    {
        int32 n;
        ASSERT_ZERO(util_string_int32(&n, "12345"));
        ASSERT_EQUAL(n, 12345);
        ASSERT_ZERO(util_string_int32(&n, "-54321"));
        ASSERT_EQUAL(n, -54321);
        ASSERT_EQUAL(util_string_int32(&n, "2147483648"), -1);
        ASSERT_EQUAL(util_string_int32(&n, "notanumber"), -1);
    }

    {
        char b[32];
        bytes_pretty(b, 512);
        ASSERT_EQUAL((char *)b, "512B");
        bytes_pretty(b, 1024);
        ASSERT_EQUAL((char *)b, "1.0000kB");
        bytes_pretty(b, SIZEMB(2));
        ASSERT_EQUAL((char *)b, "2.0000MB");
    }

    {
        int32 arr[] = {10, 5, 20, 1};
        qsort64(arr, 4, sizeof(int32), util_test_qsort_cmp);
        ASSERT_EQUAL(arr[0], 1);
        ASSERT_EQUAL(arr[1], 5);
        ASSERT_EQUAL(arr[2], 10);
        ASSERT_EQUAL(arr[3], 20);
    }

    {
        char *src = "memdup_test";
        char *dup = xmemdup(src, 12);
        ASSERT_EQUAL(src, dup);
        ASSERT_NOT_EQUAL((void *)src, (void *)dup);
        free2(dup, 12);
    }

    ASSERT_EQUAL(deg2rad(180.0), 3.141592653589793);
    ASSERT_EQUAL(rad2deg(3.141592653589793), 180.0);
    ASSERT_POSITIVE(util_nthreads());

    ASSERT_EQUAL(CLAMP(2.0, -2.1, 2.1),   2.0);
    ASSERT_EQUAL(CLAMP(0.2, -0.1, 0.1),   0.1);
    ASSERT_EQUAL(CLAMP(-0.2, -0.1, 0.1), -0.1);

    ASSERT_ZERO(CLAMP(+0, -1, +1));
    ASSERT_EQUAL(CLAMP(+2, -1, +1), +1);
    ASSERT_EQUAL(CLAMP(-2, -1, +1), -1);

    NCALLS(1);

#if OS_UNIX
    (void)util_segv_handler;
#endif

    (void)malloc_debug;
    (void)realloc_debug;
    (void)free_debug;
    (void)free2_;

    (void)xmmap_commit;
#if OS_UNIX
    (void)xkill;
    (void)xdup2;
    (void)xpipe;
#endif
    exit(EXIT_SUCCESS);
}

#endif

#endif /* UTIL_C */
