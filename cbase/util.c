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

static ullong here_counter;

CBASE_API_DEF void
here_impl(char *file, int32 line, char *func) {
#if OS_UNIX
    char buffer[4096];
#endif

    fprintf(stderr, "\n===== HERE(%llu): %s:%d (%s)\n",
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

static bool timezone_initialized = false;
static time_t timezone_offset = 0;

#define SFA_LINKAGE CBASE_API_DEF
#define SFA_SNPRINTF_LINKAGE CBASE_API_DECL
#define SFA_TYPE char *
#define SFA_NAME strings
#define SFA_FORMAT "%s"
#include "sfa.h"

#define SFA_LINKAGE CBASE_API_DEF
#define SFA_SNPRINTF_LINKAGE CBASE_API_DECL
#define SFA_TYPE double
#define SFA_NAME doubles
#define SFA_FORMAT "%f"
#include "sfa.h"

#define CLAMP_LINKAGE CBASE_API_DEF
#define CLAMP_TYPE double
#include "clamp.h"

#define CLAMP_LINKAGE CBASE_API_DEF
#define CLAMP_TYPE int64
#include "clamp.h"

static char *notifiers[2] = {"dunstify", "notify-send"};

#if !CBASE_HAS_SYSTEM_MEMMEM
static void *
cbase_memmem_fallback(
    void *haystack,
    size_t hay_len,
    void *needle,
    size_t needle_len
) {
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
    if (hay_len < needle_len) {
        return NULL;
    }

    end = h + hay_len;
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

CBASE_API_DEF void *
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

CBASE_API_DEF void *
memmem64(void *haystack, int64 hay_len, void *needle, int64 needle_len) {
    void *result;

    if (hay_len <= 0) {
        return NULL;
    }
    if (needle_len <= 0) {
        return NULL;
    }

#if CBASE_HAS_SYSTEM_MEMMEM
    result = memmem(haystack, (size_t)hay_len, needle, (size_t)needle_len);
#else
    result = cbase_memmem_fallback(haystack, (size_t)hay_len,
                                   needle, (size_t)needle_len);
#endif
    return result;
}

CBASE_API_DEF bool
strequal(char *s1, char *s2) {
    return !strcmp(s1, s2);
}

static void
striqual_validate_ascii_utf8(char *string, int32 string_len) {
    int32 bad_offset = 0;

    if (string_len < 0) {
        error("Error: Invalid string length = %d.\n", string_len);
        fatal(EXIT_FAILURE);
    }
    if ((string == NULL) && (string_len > 0)) {
        error("Error: NULL string with length = %d.\n", string_len);
        fatal(EXIT_FAILURE);
    }
    if (!utf8_valid(string, string_len, &bad_offset)) {
        error("Error: String is invalid UTF-8 at byte %d.\n", bad_offset);
        fatal(EXIT_FAILURE);
    }
    for (int32 i = 0; i < string_len; i += 1) {
        if ((uchar)string[i] > 0x7f) {
            error("Error: String contains non-ASCII UTF-8 at byte %d.\n", i);
            fatal(EXIT_FAILURE);
        }
    }

    return;
}

static char
striqual_ascii_lower(char c) {
    if ((c >= 'A') && (c <= 'Z')) {
        c = (char)(c - 'A' + 'a');
    }

    return c;
}

CBASE_API_DEF bool
striqual(char *s1, char *s2) {
    return striqual2(s1, strlen32(s1), s2, strlen32(s2));
}

CBASE_API_DEF bool
optional_strequal(char *a, int32 a_len, char *b, int32 b_len) {
    if ((a == NULL) || (b == NULL)) {
        return false;
    }

    return strequal2(a, a_len, b, b_len);
}

CBASE_API_DEF bool
strequal2(char *a, int32 a_len, char *b, int32 b_len) {
    if (a_len != b_len) {
        return false;
    }
    if (memcmp64(a, b, a_len)) {
        return false;
    }

    return true;
}

CBASE_API_DEF bool
striqual2(char *a, int32 a_len, char *b, int32 b_len) {
    if (DEBUGGING) {
        striqual_validate_ascii_utf8(a, a_len);
        striqual_validate_ascii_utf8(b, b_len);
    }

    if (a_len != b_len) {
        return false;
    }
    for (int32 i = 0; i < a_len; i += 1) {
        if (striqual_ascii_lower(a[i]) != striqual_ascii_lower(b[i])) {
            return false;
        }
    }

    return true;
}

CBASE_API_DEF void *
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

CBASE_API_DEF int32
optional_strlen32(char *string) {
    if (string == NULL) {
        return 0;
    }
    return strlen32(string);
}

CBASE_API_DEF int32
strlen32(char *string) {
    int32 length;
    size_t len;

    ASSERT(string);
    len = strlen(string);

    if (DEBUGGING) {
        if (len >= MAXOF(length)) {
            error("Error: string (%.*s ...) is too long.\n", 50, string);
            fatal(EXIT_FAILURE);
        }
    }

    length = (int32)len;
    return length;
}

CBASE_API_DEF char *
strncpy32(char *dest, char *source, int64 space) {
    if (DEBUGGING) {
        if (space <= 0) {
            error("Error: string (%.*s ...) is too long.\n", 50, source);
            fatal(EXIT_FAILURE);
        }
        if ((ullong)space >= SIZE_MAX) {
            error("Error: space is too large.\n");
            fatal(EXIT_FAILURE);
        }
    }

    return strncpy(dest, source, (size_t)space);
}

CBASE_API_DEF int
strncmp32(char *left, char *right, int64 size) {
    int result;
    if (size == 0) {
        return 0;
    }
    if (DEBUGGING) {
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error: Size (%lld) is bigger than SIZEMAX\n", size);
            fatal(EXIT_FAILURE);
        }
    }
    result = strncmp(left, right, (size_t)size);
    return result;
}

CBASE_API_DEF char *
begins_with(char *string, int32 string_len, char *literal, int32 length) {
    if (string_len < length) {
        return NULL;
    }
    if (!memcmp64(string, literal, length)) {
        return string + length;
    } else {
        return NULL;
    }
}

CBASE_API_DEF char *
ends_with(char *string, int32 string_len, char *literal, int32 length) {
    if (string_len < length) {
        return NULL;
    }
    string += (string_len - length);
    if (!memcmp64(string, literal, length)) {
        return string;
    } else {
        return NULL;
    }
}

CBASE_API_DEF int
memcmp64(void *left, void *right, int64 size) {
    if (size == 0) {
        return 0;
    }
    if (DEBUGGING) {
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

CBASE_API_DEF char *
remove_escape_sequences(char *data, int32 *data_len) {
    int32 old_len = *data_len;
    int32 read_index = 0;
    int32 write_index = 0;

    while (read_index < old_len) {
        if (data[read_index] != '\033') {
            data[write_index++] = data[read_index++];
            continue;
        }

        read_index += 1;

        if (read_index >= old_len) {
            break;
        }

        if (data[read_index] == '[') {
            read_index += 1;

            while (read_index < old_len) {
                uchar c = (uchar)data[read_index++];

                if ((c >= 0x40) && (c <= 0x7e)) {
                    break;
                }
            }
        } else {
            read_index += 1;
        }
    }

    data[write_index] = '\0';
    *data_len = write_index;
    data = realloc2(data, old_len + 1, *data_len + 1, SIZEOF(*data));

    return data;
}

CBASE_API_DEF int32
random_ascii_string(char *buffer, int32 capacity, int32 min_len) {
    int32 max_len = capacity - 1;
    int32 len = min_len;
    int32 range;

    if (capacity <= 0) {
        return 0;
    }

    if (len > max_len) {
        len = max_len;
    }

    range = max_len - len + 1;
    if (range > 1) {
        len = len + (rand() % range);
    }

    for (int32 i = 0; i < len; i += 1) {
        int32 ascii_val = 32 + (rand() % 95);
        buffer[i] = (char)ascii_val;
    }
    buffer[len] = '\0';

    return len;
}

CBASE_API_DEF void
write_all(int fd, char *buffer, int64 left) {
    int64 written = 0;
    int64 w;

    while (left > 0) {
        if ((w = write(fd, buffer + written, (RW_TYPE)left)) <= 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "Error writing: %s.\n", strerror(errno));
            fatal(EXIT_FAILURE);
        }
        left -= w;
        written += w;
    }
    return;
}

#define RW_FUNCTION_LINKAGE CBASE_API_DEF
#define RW_FUNCTION write
#include "rw_function.h"

#define RW_FUNCTION_LINKAGE CBASE_API_DEF
#define RW_FUNCTION read
#include "rw_function.h"

CBASE_API_DEF void
qsort64(void *base, int64 n, int64 size, int (*compar)(void *, void *)) {
    int (*compar_consted)(const void *, const void *);
    compar_consted = (int (*)(const void *, const void *)) compar;

    if (DEBUGGING) {
        if ((size <= 0) || (n <= 0)) {
            error("Error: Invalid size(%lld) or n(%lld)\n", size, n);
            fatal(EXIT_FAILURE);
        }
        if ((size_t)size >= (SIZE_MAX / (size_t)n)) {
            error("Error: Overflow (%lld*%lld)\n", size, n);
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

CBASE_API_DEF int32 __attribute__((format(printf, 3, 4)))
snprintf2(char *buffer, int64 size, char *format, ...) {
    int n;
    va_list args;

    ASSERT_MORE_EQUAL(size, 0);
    va_start(args, format);
    n = vsnprintf(buffer, (size_t)size, format, args);
    va_end(args);

    if ((n < 0) || (n >= size)) {
        fprintf(stderr, "Error in vsnprintf(\"%s\") (n = %d)\n", format, n);
        fatal(EXIT_FAILURE);
    }
    return n;
}

CBASE_API_DEF int32
itoa2(char *str, int32 size, llong num) {
    ullong magnitude;
    int i = 0;
    bool negative = false;

    if (size < 22) {
        error("Error in itoa2: buffer is too small.\n");
        fatal(EXIT_FAILURE);
    }

    if (num < 0) {
        negative = true;
        magnitude = (ullong)(-(num + 1)) + 1;
    } else {
        magnitude = (ullong)num;
    }

    do {
        str[i] = (char)(magnitude % 10 + '0');
        i += 1;
        magnitude /= 10;
    } while (magnitude > 0);

    if (negative) {
        str[i] = '-';
        i += 1;
    }

    str[i] = '\0';

    for (long j = 0; j < i / 2; j += 1) {
        char temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }

    // this is here because of gcc -fanalyzer
    ASSERT(i < 22);

    return i;
}

CBASE_API_DEF long
atoi2(char *str) {
    return atoi(str);
}

CBASE_API_DEF int64
strftime2(char *buffer, int64 size, char *format, struct tm *time_info) {
    int64 n;

    n = (int64)strftime(buffer, (size_t)size, format, time_info);
    if ((n <= 0) || (n >= size)) {
        error("Error in strftime(\"%s\") (n = %lld).\n", format, n);
        fatal(EXIT_FAILURE);
    }
    return n;
}

CBASE_API_DEF int32
util_filename_from(char *buffer, int64 size, int fd) {
#if OS_LINUX
    char linkpath[64];
    ssize_t len;

    SNPRINTF(linkpath, "/proc/self/fd/%d", fd);
    if ((len = readlink(linkpath, buffer, (size_t)(size - 1))) < 0) {
        return -1;
    }
    buffer[len] = '\0';
    return 0;
#elif CBASE_HAS_F_GETPATH
    static char buffer2[MAXPATHLEN];
    int64 len;

    if (fcntl(fd, F_GETPATH, buffer2) < 0) {
        return -1;
    }
    len = MIN(strlen32(buffer2), size - 1);
    memcpy64(buffer, buffer2, len + 1);
    buffer[len] = '\0';
    return 0;
#elif OS_WINDOWS
    HANDLE h;
    DWORD len;
    intptr h2 = _get_osfhandle(fd);

    if ((h = (HANDLE)h2) == INVALID_HANDLE_VALUE) {
        return -1;
    }

    len = GetFinalPathNameByHandleA(h, buffer, (DWORD)size,
                                    FILE_NAME_NORMALIZED);

    if ((len <= 0) || (len >= size)) {
        return -1;
    }

    if (strncmp32(buffer, "\\\\?\\", 4) == 0) {
        memmove64(buffer, buffer + 4, len - 3);
    }

    return 0;
#else
    (void)size;
    (void)fd;
    (void)buffer;
    return -1;
#endif
}

#if OS_WINDOWS
static int
strerror_r(int errnum, char *buffer, size_t size) {
    char *error_message = strerror(errnum);
    int32 len = strlen32(error_message);

    ASSERT_MORE(size, 0);
    ASSERT_LESS(size, MAXOF(len));

    if (len >= (int32)size) {
        len = (int32)size - 1;
    }

    memcpy64(buffer, error_message, len);
    buffer[len] = '\0';

    return 0;
}
#endif

CBASE_API_DEF int
xclose(char *file, int line, int *fd, char *fd_var_name, char *filename) {
#if DEBUGGING
    char buffer[4096];

    if (filename == NULL) {
        if (util_filename_from(buffer, sizeof(buffer), *fd) < 0) {
            filename = fd_var_name;
        } else {
            filename = buffer;
        }
    }
#else
    if (filename == NULL) {
        filename = fd_var_name;
    }
#endif

    if (*fd < 0) {
        return 0;
    }

    if (close(*fd) < 0) {
        char error_buffer[4096];
        char itoa_buffer[32];
        ITOA(itoa_buffer, line);

        strerror_r(errno, error_buffer, sizeof(error_buffer));

        error_async_safe(file);
        error_async_safe(":");
        error_async_safe(itoa_buffer);
        error_async_safe(" Error closing ");
        error_async_safe(filename);
        error_async_safe(": ");
        error_async_safe(error_buffer);
        error_async_safe(".\n");

        *fd = -1;
        return -1;
    }
    *fd = -1;
    return 0;
}

CBASE_API_DEF int
xunlink(char *filename) {
    if (unlink(filename) < 0) {
        error2("Error in unlink(%s): %s.\n", filename, strerror(errno));
        return -1;
    }
    return 0;
}

CBASE_API_DEF FILE *
xfopen(char *file, int32 line, char *func, char *filename, char *mode) {
    FILE *f;
    char *mode_long = "what";

    if (strequal(mode, "w")) {
        mode_long = "writing";
    }
    if (strequal(mode, "r")) {
        mode_long = "reading";
    }
    if (strequal(mode, "r+")) {
        mode_long = "reading and writing";
    }
    if (strequal(mode, "w+")) {
        mode_long = "reading and writing";
    }
    if (strequal(mode, "a")) {
        mode_long = "appending";
    }
    if (strequal(mode, "a+")) {
        mode_long = "reading and appending";
    }

    if ((f = fopen(filename, mode)) == NULL) {
        error_impl(file, line, func,
                   "Error opening %s for %s: %s.\n",
                   filename, mode_long, strerror(errno));
        return NULL;
    }
    return f;
}

CBASE_API_DEF int
xfclose(char *file, int32 line, char *func, FILE *f, char *filename) {
    if (fclose(f)) {
        error_impl(file, line, func,
                   "Error closing %s: %s.\n", filename, strerror(errno));
        return -1;
    }
    return 0;
}

CBASE_API_DEF int
xclosedir(DIR *dir, char *dirname) {
    if (closedir(dir)) {
        error2("Error closing directory %s: %s.\n", dirname, strerror(errno));
        return -1;
    }
    return 0;
}

CBASE_API_DEF void __attribute__((format(printf, 4, 5)))
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
        fprintf(stderr,
                "%s:%d %s(): Error in vsnprintf(\"%s\") (n = %d).\n",
                file, line, func, format, n);
        fatal(EXIT_FAILURE);
    }

    if (!DEBUGGING) {
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
        fprintf(stderr, "Error executing notifier: %s.\n", strerror(errno));
        exit(EXIT_FAILURE);
    case -1:
        fprintf(stderr, "Error forking: %s.\n", strerror(errno));
        break;
    default:
        break;
    }
#endif

    free2(big_buffer, m);
    return;
}

CBASE_API_DEF void
error_async_safe(char *message) {
    int32 len = strlen32(message);
    write_all(STDERR_FILENO, message, len);
    return;
}

CBASE_API_DEF void __attribute((noreturn))
fatal(int status) {
#if defined(__EMSCRIPTEN__)
    char stack[8192];
    int32 flags = EM_LOG_C_STACK
                  |EM_LOG_JS_STACK
                  |EM_LOG_DEMANGLE
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

CBASE_API_DEF void
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

CBASE_API_DEF int32
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

CBASE_API_DEF void __attribute__((noreturn))
util_die_notify(char *program_name, char *format, ...) {
    int32 n;
    va_list args;
    char buffer[BUFSIZ];

    va_start(args, format);
    n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if ((n < 0) || (n >= SIZEOF(buffer))) {
        fatal(EXIT_FAILURE);
    }

    buffer[n] = '\0';
    write64(STDERR_FILENO, buffer, (uint32)n + 1);
    for (uint32 i = 0; i < LENGTH(notifiers); i += 1) {
        execlp(notifiers[i], notifiers[i], "-u", "critical", program_name,
               buffer, NULL);
    }
    fatal(EXIT_FAILURE);
}

#if OS_UNIX
CBASE_API_DEF int32
util_copy_file_sync(char *destination, char *source) {
    int32 source_fd;
    int32 destination_fd;
    char buffer[BUFSIZ];
    ssize_t r = 0;
    ssize_t w = 0;

    if ((source_fd = open(source, O_RDONLY)) < 0) {
        error("Error opening %s for reading: %s.\n", source, strerror(errno));
        return -1;
    }

    if ((destination_fd
         = open(destination,
                O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
        error("Error opening %s for writing: %s.\n",
              destination, strerror(errno));
        XCLOSE(&source_fd, source);
        return -1;
    }

    errno = 0;
    while ((r = read64(source_fd, buffer, BUFSIZ)) > 0) {
        int saved_errno;

        while (((w = write64(destination_fd, buffer, r)) < 0)
                && (errno == EINTR)) {
            continue;
        }
        saved_errno = errno;

        if (w != r) {
            fprintf(stderr, "Error writing data to %s", destination);
            if (r < 0) {
                fprintf(stderr, ": %s", strerror(saved_errno));
            }
            fprintf(stderr, ".\n");

            XCLOSE(&source_fd, source);
            XCLOSE(&destination_fd, destination);
            return -1;
        }
    }

    if (r < 0) {
        error("Error reading data from %s: %s.\n", source, strerror(errno));
        XCLOSE(&source_fd, source);
        XCLOSE(&destination_fd, destination);
        return -1;
    }

    XCLOSE(&source_fd, source);
    XCLOSE(&destination_fd, destination);
    return 0;
}

CBASE_API_DEF int32
util_copy_file_async(char *destination, char *source, int *dest_fd) {
    int32 source_fd;

    if ((source_fd = open(source, O_RDONLY)) < 0) {
        error("Error opening %s for reading: %s.\n", source, strerror(errno));
        return -1;
    }

    if ((*dest_fd = open(destination,
                         O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)) < 0) {
        error("Error opening %s for writing: %s.\n",
              destination, strerror(errno));
        XCLOSE(&source_fd, source);
        return -1;
    }

#if 0
    int32 fadvise_err;
    if ((fadvise_err = posix_fadvise(source_fd,
                                     0, 0,
                                     POSIX_FADV_WILLNEED)) < 0) {
        error("Error in posix_fadvise(POSIX_FADV_WILLNEED): %s.\n",
              strerror(fadvise_err));
    }
#endif

    return source_fd;
}

CBASE_API_DEF void
util_copy_file_async_parsed(UtilCopyFilesAsync *copy_files) {
    struct pollfd *pipes = copy_files->pipes;
    int *dests = copy_files->dests;
    int32 left = copy_files->nfds;

    if (copy_files->nfds > LENGTH(copy_files->pipes)) {
        error("Error too many files for UtilCopyFilesAsync definition.\n");
        fatal(EXIT_FAILURE);
    }

    while (left > 0) {
        char buffer[BUFSIZ];
        int64 r;
        int64 w;
        int64 n;

        n = poll(pipes, (nfds_t)copy_files->nfds, 1000);
        if (n == 0) {
            continue;
        }
        if (n < 0) {
            error("Error in poll(nfds=%d): %s.\n",
                  copy_files->nfds, strerror(errno));
            break;
        }
        for (int32 i = 0; i < copy_files->nfds; i += 1) {
            if (n <= 0) {
                break;
            }
            if (!(pipes[i].revents & POLLIN)) {
                pipes[i].revents = 0;
                continue;
            }
            n -= 1;
            while ((r = read64(pipes[i].fd, buffer, sizeof(buffer))) > 0) {
                if ((w = write64(dests[i], buffer, r)) != r) {
                    if (w < 0) {
                        error("Error writing: %s.\n", strerror(errno));
                    }
                    break;
                }
            }
            if (r < 0) {
                error("Error reading: %s.\n", strerror(errno));
            }
            XCLOSE(&dests[i]);
            XCLOSE(&pipes[i].fd);

            left -= 1;
            pipes[i].revents = 0;
        }
    }
    free2(copy_files, sizeof(*copy_files));
    return;
}

#endif

#if CBASE_HAS_PROCFS
CBASE_API_DEF void
send_signal(char *executable, int32 signal_number) {
    DIR *processes;
    struct dirent *process;
    int64 len = strlen32(executable);

    if ((processes = opendir("/proc")) == NULL) {
        error("Error opening /proc: %s\n", strerror(errno));
        return;
    }

    while ((process = readdir(processes))) {
        char buffer[256];
        char command[256];
        int32 pid;
        int32 cmdline;
        ssize_t r;
        char *last;

#if CBASE_DIRENT_HAS_D_TYPE
        if ((process->d_type != DT_DIR) && (process->d_type != DT_UNKNOWN)) {
            continue;
        }
#endif
        if ((pid = atoi(process->d_name)) <= 0) {
            continue;
        }

        SNPRINTF(buffer, "/proc/%s/cmdline", process->d_name);

        if ((cmdline = open(buffer, O_RDONLY)) < 0) {
            continue;
        }

        errno = 0;
        if ((r = read64(cmdline, command, sizeof(command))) <= 0) {
            (void)r;
            XCLOSE(&cmdline, buffer);
            continue;
        }
        XCLOSE(&cmdline, buffer);

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

    closedir(processes);
    return;
}
#elif OS_UNIX
CBASE_API_DEF void
send_signal(char *executable, int32 signal_number) {
    char signal_string[14];
    SNPRINTF(signal_string, "%d", signal_number);
    pid_t child;

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
            fprintf(stderr, "Error waiting for child: %s.\n", strerror(errno));
            fatal(EXIT_FAILURE);
        }
    }
    return;
}
#endif

#if !OS_WINDOWS
CBASE_API_DEF bool
util_file_exists(char *filename) {
#if defined(O_PATH) && defined(O_NOFOLLOW)
    // this should be faster than lstat()
    {
        int32 fd;
        if (((fd = open(filename, O_PATH | O_NOFOLLOW)) < 0)
                && (errno == ENOENT)) {
            return false;
        } else {
            close(fd);
            return true;
        }
    }
#else
    {
        struct stat statbuf;
        if ((lstat(filename, &statbuf) < 0) && (errno == ENOENT)) {
            return false;
        } else {
            return true;
        }
    }
#endif
}
#else
CBASE_API_DEF bool
util_file_exists(char *filename) {
    DWORD attributes;

    attributes = GetFileAttributesA(filename);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    return true;
}
#endif

CBASE_API_DEF bool
util_equal_files(char *filename_a, char *filename_b) {
    int fd_a;
    int fd_b = -1;
    char buffer_a[BUFSIZ];
    char buffer_b[BUFSIZ];
    int64 total_r = 0;
    int64 ra;
    struct stat stat_a;
    struct stat stat_b;
    bool equal = false;

    if ((fd_a = open(filename_a, O_RDONLY)) < 0) {
        error("Error opening %s: %s.\n", filename_a, strerror(errno));
        equal = false;
        goto out;
    }
    if ((fd_b = open(filename_b, O_RDONLY)) < 0) {
        error("Error opening %s: %s.\n", filename_b, strerror(errno));
        equal = false;
        goto out;
    }

    if (fstat(fd_a, &stat_a) < 0) {
        error("Error in stat(%s): %s.\n", filename_a, strerror(errno));
        equal = false;
        goto out;
    }
    if (fstat(fd_b, &stat_b) < 0) {
        error("Error in stat(%s): %s.\n", filename_b, strerror(errno));
        equal = false;
        goto out;
    }
    if (stat_a.st_size != stat_b.st_size) {
        equal = false;
        goto out;
    }
    if ((stat_a.st_dev == stat_b.st_dev) && (stat_a.st_ino == stat_b.st_ino)) {
        equal = true;
        goto out;
    }

#if OS_UNIX
    do {
        if (stat_a.st_size > 0) {
            void *map_a;
            void *map_b;

            map_a = mmap(NULL, (size_t)stat_a.st_size,
                         PROT_READ, MAP_PRIVATE, fd_a, 0);
            if (map_a == MAP_FAILED) {
                error("Error in mmap(%s): %s\n", filename_a, strerror(errno));
                break;
            }
            map_b = mmap(NULL, (size_t)stat_a.st_size,
                         PROT_READ, MAP_PRIVATE, fd_b, 0);
            if (map_b == MAP_FAILED) {
                error("Error in mmap(%s): %s\n", filename_b, strerror(errno));
                xmunmap(map_a, stat_a.st_size);
                break;
            }

            if (memcmp64(map_a, map_b, stat_a.st_size)) {
                equal = false;
            } else {
                equal = true;
            }

            xmunmap(map_a, stat_a.st_size);
            xmunmap(map_b, stat_b.st_size);
            goto out;
        } else {
            equal = true;
            goto out;
        }
    } while (0);
#endif
    while ((ra = read64(fd_a, buffer_a, sizeof(buffer_a))) > 0) {
        int64 rb;
        if ((rb = read64(fd_b, buffer_b, sizeof(buffer_b))) != ra) {
            if (rb < 0) {
                error("Error reading from %s: %s", filename_b, strerror(errno));
            }
            equal = false;
            goto out;
        }
        if (memcmp64(buffer_a, buffer_b, ra)) {
            equal = false;
            goto out;
        }
        total_r += ra;
    }
    if (ra < 0) {
        error("Error reading from %s: %s", filename_a, strerror(errno));
    }
    if (total_r == stat_a.st_size) {
        equal = true;
        goto out;
    }
out:
    XCLOSE(&fd_a, filename_a);
    XCLOSE(&fd_b, filename_b);
    return equal;
}

CBASE_API_DEF double
rad2deg(double radians) {
    double RAD2DEG = 180.0 / 3.141592653589793;
    return radians*RAD2DEG;
}

CBASE_API_DEF double
deg2rad(double degrees) {
    double DEG2RAD = 3.141592653589793 / 180.0;
    return degrees*DEG2RAD;
}

CBASE_API_DEF int32
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

CBASE_API_DEF void
normalize(char *restrict path, int32 *restrict length) {
    char *p;
    int64 off = 0;

    if (*length < 0) {
        *length = strlen32(path);
    }

    while ((p = memmem64(path + off, *length - off, "//", 2))) {
        off = p - path;

        memmove64(&p[0], &p[1], *length - off);
        *length -= 1;
    }

    while ((path[0] == '.') && (path[1] == '/') && (*length > 2)) {
        memmove64(&path[0], &path[2], *length - 1);
        *length -= 2;
    }

    while ((*length >= 2)
           && (path[*length - 2] == '/')
           && (path[*length - 1] == '.')) {
        path[*length - 1] = '\0';
        *length -= 1;
    }

    off = 0;
    while ((p = memmem64(path + off, *length - off, "/./", 3))) {
        off = p - path;

        memmove64(&p[1], &p[3], *length - off - 2);
        *length -= 2;
    }

    return;
}

CBASE_API_DEF char *
basename2(char *path, int32 *full_length, int32 *base_len) {
    int32 left;
    char *end;
    char *fslash = NULL;
    char *bslash = NULL;
    char *p = path;

    normalize(path, full_length);

    left = *full_length;
    ASSERT_MORE(*full_length, 0);
    end = path + left - 1;

    if (left == 1) {
        if (base_len) {
            *base_len = 1;
        }
        return p;
    }

    while (left > 0) {
        int64 length;

        fslash = memchr64(p, '/', left);
        if (OS_WINDOWS) {
            bslash = memchr64(p, '\\', left);
        }

        if ((fslash == NULL) && (bslash == NULL)) {
            if (base_len) {
                *base_len = *full_length - (int32)(p - path);
            }
            return p;
        }
        if ((fslash == end) || (bslash == end)) {
            if (base_len) {
                *base_len = *full_length - (int32)(p - path);
            }
            return p;
        }
        if ((uintptr)fslash > (uintptr)bslash) {
            length = fslash - p + 1;
            p = fslash + 1;
        } else {
            length = bslash - p + 1;
            p = bslash + 1;
        }

        left -= length;
    }

    if (base_len) {
        *base_len = *full_length;
    }
    return path;
}

CBASE_API_DEF char *
path_basename(char *path, int32 path_len) {
    int32 slash = -1;
    int32 start;

    for (int32 i = 0; i < path_len; i += 1) {
        if (path[i] == '/') {
            slash = i;
        }
    }

    start = slash + 1;
    return xstrndup(path + start, path_len - start);
}

CBASE_API_DEF int32
dirname2(char *buffer, char *path, int32 *path_len) {
    char *last_slash;
    int32 dir_length;
    if (*path_len < 0) {
        *path_len = strlen32(path);
    }

    normalize(path, path_len);

    if (*path_len == 1) {
        if (*path == '/') {
            sprintf(buffer, "/");
        } else {
            sprintf(buffer, ".");
        }
        return 1;
    }

    if ((last_slash = memrchr64(path, '/', *path_len - 1)) == NULL) {
        sprintf(buffer, ".");
        return 1;
    }

    dir_length = (int32)(last_slash - path);
    if (dir_length == 0) {
        dir_length = 1;
    }

    if (buffer != path) {
        memcpy64(buffer, path, dir_length);
    }

    buffer[dir_length] = '\0';
    return dir_length;
}

CBASE_API_DEF void
print_timings(char *file, int32 line, char *func,
              int64 nitems, struct timespec t0, struct timespec t1) {
    llong seconds = t1.tv_sec - t0.tv_sec;
    llong nanos = t1.tv_nsec - t0.tv_nsec;

    double total_seconds = (double)seconds + (double)nanos / 1.0e9;
    double micros_per = 1e6*(total_seconds / (double)nitems);

    printf("\ntime elapsed %s:%d:%s\n", file, line, func);
    printf("%gs = %gus per item.\n", total_seconds, micros_per);
    return;
}

CBASE_API_DEF double
timediff(struct timespec t0, struct timespec t1) {
    llong sec = t1.tv_sec - t0.tv_sec;
    llong nsec = t1.tv_nsec - t0.tv_nsec;
    double diff = (double)sec + (double)nsec*1e-9;
    return diff;
}

CBASE_API_DEF void
time_monotonic_precise(struct timespec *time) {
    int32 status;

#if defined(CLOCK_MONOTONIC_RAW)
    status = clock_gettime(CLOCK_MONOTONIC_RAW, time);
#elif defined(CLOCK_MONOTONIC)
    status = clock_gettime(CLOCK_MONOTONIC, time);
#else
    struct timeval timeval;

    status = gettimeofday(&timeval, NULL);
    if (status == 0) {
        time->tv_sec = timeval.tv_sec;
        time->tv_nsec = timeval.tv_usec*1000;
    }
#endif

    if (status < 0) {
        error("Error reading precise monotonic clock: %s.\n",
              strerror(errno));
        fatal(EXIT_FAILURE);
    }
    return;
}

CBASE_API_DEF void
time_monotonic_coarse(struct timespec *time) {
    int32 status;

#if defined(CLOCK_MONOTONIC_COARSE)
    status = clock_gettime(CLOCK_MONOTONIC_COARSE, time);
#elif defined(CLOCK_MONOTONIC)
    status = clock_gettime(CLOCK_MONOTONIC, time);
#elif defined(CLOCK_MONOTONIC_RAW)
    status = clock_gettime(CLOCK_MONOTONIC_RAW, time);
#else
    struct timeval timeval;

    status = gettimeofday(&timeval, NULL);
    if (status == 0) {
        time->tv_sec = timeval.tv_sec;
        time->tv_nsec = timeval.tv_usec*1000;
    }
#endif

    if (status < 0) {
        error("Error reading coarse monotonic clock: %s.\n",
              strerror(errno));
        fatal(EXIT_FAILURE);
    }
    return;
}

CBASE_API_DEF void
catfile(int where, char *file) {
    int fd;
    char buffer[4096];
    int64 r;

    if ((fd = open(file, O_RDONLY)) < 0) {
        error("Error opening %s: %s.\n", file, strerror(errno));
        fatal(EXIT_FAILURE);
    }

    printf("\n");
    while ((r = read64(fd, buffer, SIZEOF(buffer))) > 0) {
        write_all(where, buffer, r);
    }
    if (r < 0) {
        error("Error reading %s: %s.\n", file, strerror(errno));
        fatal(EXIT_FAILURE);
    }

    XCLOSE(&fd, file);
    return;
}

#if !OS_WINDOWS

#define XSIGNAL(NAME) [NAME] = #NAME
static char *signal_names[] = {
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

CBASE_API_DEF void
xpipe(int array[2]) {
    if (pipe(array) < 0) {
        error("Error creating pipe: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    }
    return;
}

CBASE_API_DEF void
xdup2(int fd1, int fd2) {
    if (dup2(fd1, fd2) < 0) {
        error("Error in dup2: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    }
    return;
}

CBASE_API_DEF void
xkill(pid_t pid, int signum) {
    if (kill(pid, signum) < 0) {
        error("Error sending signal %d=%s to %d: %s.\n",
              signum, signal_names[signum], pid, strerror(errno));
    }
    return;
}

#endif /* !OS_WINDOWS */

#if OS_UNIX
CBASE_API_DEF void
timezone_init(void) {
    time_t current_time;
    struct tm local_tm;
    struct tm gm_tm;

    current_time = time(NULL);
    localtime_r(&current_time, &local_tm);
    gmtime_r(&current_time, &gm_tm);

    timezone_offset = (local_tm.tm_hour - gm_tm.tm_hour)*3600;
    timezone_offset += (local_tm.tm_min - gm_tm.tm_min)*60;

    if (local_tm.tm_year < gm_tm.tm_year) {
        timezone_offset -= 24*3600;
    } else if (local_tm.tm_year > gm_tm.tm_year) {
        timezone_offset += 24*3600;
    } else if (local_tm.tm_yday < gm_tm.tm_yday) {
        timezone_offset -= 24*3600;
    } else if (local_tm.tm_yday > gm_tm.tm_yday) {
        timezone_offset += 24*3600;
    }

    timezone_initialized = true;
    return;
}
#endif

CBASE_API_DEF bool
path_missing(char *path) {
    if (path == NULL) {
        return true;
    }
    if (path[0] == '\0') {
        return true;
    }

    return false;
}

CBASE_API_DEF bool
read_entire_file(char *path, char **file_bytes, int32 *file_len) {
    FILE *file;
    int64 len;
    int64 read_len;
    char *bytes;

    if (file_bytes) {
        *file_bytes = NULL;
    }
    if (file_len) {
        *file_len = 0;
    }
    if (path_missing(path)
        || (file_bytes == NULL)
        || (file_len == NULL)) {
        error("Error reading file: invalid arguments.\n");
        return false;
    }

    if ((file = fopen(path, "rb")) == NULL) {
        error("Error opening "RED("%s")" for reading: %s",
              path, strerror(errno));
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        error("Error seeking end of %s: %s.\n", path, strerror(errno));
        XFCLOSE(file, path);
        return false;
    }
    if ((len = ftell(file)) < 0) {
        error("Error in ftell(%s): %s.\n", path, strerror(errno));
        XFCLOSE(file, path);
        return false;
    }
    if (len >= MAXOF(*file_len)) {
        error("Only files up to 2GB are supported.\n");
        XFCLOSE(file, path);
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        error("Error rewinding %s: %s.\n", path, strerror(errno));
        XFCLOSE(file, path);
        return false;
    }

    bytes = malloc2(len + 1);
    read_len = 0;
    if (len > 0) {
        read_len = fread64(bytes, 1, len, file);
    }
    if (read_len != len) {
        error("Error reading "RED("%s")": %s.\n", path, strerror(errno));
        free2(bytes, (len + 1)*SIZEOF(*bytes));
        XFCLOSE(file, path);
        return false;
    }
    bytes[read_len] = '\0';
    if (XFCLOSE(file, path) != 0) {
        free2(bytes, (len + 1)*SIZEOF(*bytes));
        return false;
    }

    *file_bytes = bytes;
    *file_len = (int32)read_len;
    return true;
}

CBASE_API_DEF bool
write_entire_file(char *path, char *text, int64 text_len) {
    FILE *file;

    if (text_len < 0) {
        error("Error writing negative length %lld to %s.",
              text_len, path);
        return false;
    }

    if ((file = fopen(path, "wb")) == NULL) {
        error("Error opening %s for writing: %s", path, strerror(errno));
        return false;
    }

    if ((text_len > 0) && (fwrite64(text, 1, text_len, file) != text_len)) {
        error("Error writing %lld bytes to %s: %s.",
              text_len, path, strerror(errno));
        XFCLOSE(file, path);
        return false;
    }

    XFCLOSE(file, path);
    return true;
}

#define STR_BUILDER_INITIAL_CAPACITY 16

CBASE_API_DEF char *
sb_opt_cstr(StrBuilder *buffer) {
    if (buffer == NULL) {
        return "";
    }
    if (buffer->data == NULL) {
        return "";
    }

    return buffer->data;
}

CBASE_API_DEF void
sb_init(StrBuilder *str_builder) {
    str_builder->data = NULL;
    str_builder->len = 0;
    str_builder->cap = 0;
    return;
}

CBASE_API_DEF void
sb_free(StrBuilder *str_builder) {
    free2(str_builder->data, str_builder->cap);
    sb_init(str_builder);
    return;
}

CBASE_API_DEF void
sb_clear(StrBuilder *str_builder) {
    str_builder->len = 0;
    if (str_builder->data) {
        str_builder->data[0] = '\0';
    }
    return;
}

CBASE_API_DEF bool
sb_copy(StrBuilder *dest, StrBuilder *source) {
    if (dest == NULL) {
        return false;
    }
    if (dest == source) {
        return true;
    }
    if (source == NULL) {
        sb_free(dest);
        return true;
    }

    sb_clear(dest);
    sb_append(dest, source->data, source->len);
    return true;
}

CBASE_API_DEF void
sb_move(StrBuilder *dest, StrBuilder *source) {
    if (dest == NULL) {
        return;
    }
    if (dest == source) {
        return;
    }

    sb_free(dest);
    if (source == NULL) {
        sb_init(dest);
        return;
    }

    *dest = *source;
    sb_init(source);
    return;
}

CBASE_API_DEF bool
sb_set(StrBuilder *str_builder, char *data, int32 data_len) {
    if (str_builder == NULL) {
        return false;
    }
    if (data_len < 0) {
        return false;
    }
    if ((data == NULL) && (data_len > 0)) {
        return false;
    }
    if ((data == str_builder->data) && str_builder->data) {
        if (data_len > str_builder->len) {
            return false;
        }
        str_builder->len = data_len;
        str_builder->data[data_len] = '\0';
        return true;
    }

    sb_clear(str_builder);
    sb_append(str_builder, data, data_len);
    return true;
}

CBASE_API_DEF void
sb_reserve(StrBuilder *str_builder, int32 extra) {
    int64 needed;
    int64 new_cap;
    int32 old_cap;

    if (extra <= 0) {
        return;
    }

    needed = (int64)str_builder->len + extra + 1;
    if (str_builder->data && (needed <= str_builder->cap)) {
        return;
    }
    if (needed >= MAXOF(str_builder->cap)) {
        error("StrBuilder only supports strings shorter than 2GB.\n");
        fatal(EXIT_FAILURE);
    }

    old_cap = str_builder->cap;
    if (str_builder->data == NULL) {
        old_cap = 0;
    }

    new_cap = str_builder->cap;
    if (new_cap <= 0) {
        new_cap = STR_BUILDER_INITIAL_CAPACITY;
    }
    while (new_cap < needed) {
        new_cap *= 2;
    }
    if (new_cap >= MAXOF(str_builder->cap)) {
        new_cap = needed;
    }

    str_builder->data = realloc2(str_builder->data, old_cap, new_cap,
                                 SIZEOF(*str_builder->data));
    str_builder->cap = (int32)new_cap;
    return;
}

CBASE_API_DEF void
sb_append(StrBuilder *str_builder, char *data, int32 data_len) {
    bool aliases = false;
    int32 data_offset = 0;

    if ((data_len <= 0) || (data == NULL)) {
        if (str_builder->data == NULL) {
            sb_reserve(str_builder, 8);
            str_builder->data[0] = '\0';
        }
        return;
    }

    if (data == str_builder->data) {
        aliases = true;
    } else if (str_builder->data) {
        uintptr data_address = (uintptr)data;
        uintptr start = (uintptr)str_builder->data;

        if (data_address >= start) {
            uintptr offset = data_address - start;

            if (offset < (uint32)str_builder->cap) {
                aliases = true;
                data_offset = (int32)offset;
            }
        }
    }

    sb_reserve(str_builder, data_len);
    if (aliases) {
        data = str_builder->data + data_offset;
        memmove64(str_builder->data + str_builder->len, data, data_len);
    } else {
        memcpy64(str_builder->data + str_builder->len, data, data_len);
    }
    str_builder->len += data_len;
    str_builder->data[str_builder->len] = '\0';

    return;
}

CBASE_API_DEF void
sb_append_byte(StrBuilder *str_builder, char byte) {
    if (byte == '\0') {
        return;
    }
    sb_reserve(str_builder, 1);
    str_builder->data[str_builder->len] = byte;
    str_builder->len += 1;
    str_builder->data[str_builder->len] = '\0';
    return;
}

CBASE_API_DEF void
sb_append_byte_if_not(StrBuilder *str_builder, char byte) {
    if ((str_builder->len > 0)
        && (str_builder->data[str_builder->len - 1] == byte)) {
        return;
    }
    sb_append_byte(str_builder, byte);
    return;
}

CBASE_API_DEF void
sb_printf(StrBuilder *str_builder, char *fmt, ...) {
    va_list ap;
    va_list ap2;
    int32 n;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0) {
        va_end(ap2);
        error("Error formatting \"%s\".", fmt);
        fatal(EXIT_FAILURE);
    }
    if (n == 0) {
        va_end(ap2);
        return;
    }

    sb_reserve(str_builder, n);
    vsnprintf(str_builder->data + str_builder->len, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    str_builder->len += n;
    return;
}

CBASE_API_DEF char *
sb_steal(StrBuilder *str_builder, int32 *len, int32 *cap) {
    char *data = str_builder->data;

    if (len) {
        *len = str_builder->len;
    }
    if (cap) {
        *cap = str_builder->cap;
    }

    sb_init(str_builder);
    return data;
}

CBASE_API_DEF char *
sb_steal_exact(StrBuilder *str_builder, int32 *len) {
    char *data;
    int32 data_len;
    int32 cap;

    data = sb_steal(str_builder, &data_len, &cap);
    if (cap != data_len + 1) {
        data = realloc2(data, cap, data_len + 1, SIZEOF(*data));
    }
    data[data_len] = '\0';

    if (len) {
        *len = data_len;
    }
    return data;
}

CBASE_API_DEF void
str_builder_array_init(StrBuilderArray *array) {
    array->items = NULL;
    array->len = 0;
    array->cap = 0;
    return;
}

CBASE_API_DEF void
str_builder_array_clear(StrBuilderArray *array) {
    if (array == NULL) {
        return;
    }

    for (int32 i = 0; i < array->len; i += 1) {
        sb_free(&array->items[i]);
    }
    array->len = 0;
    return;
}

CBASE_API_DEF void
str_builder_array_destroy(StrBuilderArray *array) {
    if (array == NULL) {
        return;
    }

    str_builder_array_clear(array);
    free2(array->items, array->cap*SIZEOF(*array->items));
    str_builder_array_init(array);
    return;
}

CBASE_API_DEF bool
str_builder_array_copy(StrBuilderArray *dest, StrBuilderArray *source) {
    StrBuilderArray replacement;

    if (dest == NULL) {
        return false;
    }
    if (dest == source) {
        return true;
    }

    str_builder_array_init(&replacement);
    if (source) {
        if (!str_builder_array_reserve(&replacement, source->len)) {
            str_builder_array_destroy(&replacement);
            return false;
        }
        for (int32 i = 0; i < source->len; i += 1) {
            if (!str_builder_array_append_copy(&replacement,
                                               &source->items[i])) {
                str_builder_array_destroy(&replacement);
                return false;
            }
        }
    }

    str_builder_array_destroy(dest);
    *dest = replacement;
    return true;
}

CBASE_API_DEF void
str_builder_array_move(StrBuilderArray *dest, StrBuilderArray *source) {
    if (dest == NULL) {
        return;
    }
    if (dest == source) {
        return;
    }

    str_builder_array_destroy(dest);
    if (source == NULL) {
        str_builder_array_init(dest);
        return;
    }
    *dest = *source;
    str_builder_array_init(source);
    return;
}

CBASE_API_DEF void
str_builder_array_swap(StrBuilderArray *left, StrBuilderArray *right) {
    StrBuilderArray temp;

    if (left == NULL) {
        return;
    }
    if (right == NULL) {
        return;
    }

    temp = *left;
    *left = *right;
    *right = temp;
    return;
}

CBASE_API_DEF bool
str_builder_array_reserve(StrBuilderArray *array, int32 extra) {
    int64 needed;
    int32 old_cap;
    int32 new_cap;

    if (array == NULL) {
        return false;
    }
    if (extra <= 0) {
        return true;
    }

    needed = (int64)array->len + extra;
    if (needed <= array->cap) {
        return true;
    }
    if (needed >= MAXOF(array->cap)) {
        error("StrBuilderArray only supports fewer than 2GB items.\n");
        fatal(EXIT_FAILURE);
    }

    old_cap = array->cap;
    new_cap = array->cap;
    if (new_cap <= 0) {
        new_cap = 8;
    }

    if (needed >= (MAXOF(new_cap)/2)) {
        new_cap = (int32)needed;
    } else {
        while (new_cap < needed) {
            new_cap *= 2;
        }
    }

    array->items = realloc2(array->items, old_cap, new_cap,
                            SIZEOF(*array->items));
    array->cap = new_cap;
    return true;
}

CBASE_API_DEF StrBuilder *
str_builder_array_append(StrBuilderArray *array) {
    StrBuilder *item;

    if (!str_builder_array_reserve(array, 1)) {
        return NULL;
    }

    item = &array->items[array->len];
    array->len += 1;
    sb_init(item);
    return item;
}

CBASE_API_DEF bool
str_builder_array_append_copy(StrBuilderArray *array, StrBuilder *item) {
    StrBuilder *dest;

    if (item == NULL) {
        return false;
    }

    dest = str_builder_array_append(array);
    if (dest == NULL) {
        return false;
    }
    if (!sb_copy(dest, item)) {
        array->len -= 1;
        sb_free(dest);
        return false;
    }
    return true;
}

CBASE_API_DEF bool
parse_option(char **parsed, char *arg, char *option_name) {
    char name_equal[256];
    char *tmp;
    int32 length = SNPRINTF(name_equal, "%s=", option_name);
    int32 arg_len;
    if (arg == NULL) {
        return false;
    }
    arg_len = strlen32(arg);

    if ((tmp = BEGINS_WITH(arg, arg_len, name_equal, length))) {
        *parsed = tmp;
        return true;
    }
    return false;
}

CBASE_API_DEF bool
is_ident_start_char(char c) {
    return isalpha((uint8)c) || c == '_';
}

CBASE_API_DEF bool
is_ident_char(char c) {
    return isalnum((uint8)c) || c == '_';
}

CBASE_API_DEF void
warn(char *fmt, ...) {
    va_list ap;

    fprintf(stderr, "%s: "RED ("warning:"), program);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    return;
}

CBASE_API_DEF bool
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
    (void)util_is_integer;
    (void)is_ident_start_char;
    (void)sb_append_byte_if_not;
    (void)sb_move;
    (void)sb_opt_cstr;
    (void)str_builder_array_copy;
    (void)str_builder_array_move;
    (void)str_builder_array_swap;
    (void)optional_strequal;
    (void)warn;
    (void)xfopen;
    (void)here_impl;
    (void)here_counter;
    (void)strequal;
    (void)path_missing;
    (void)read_entire_file;
    (void)write_entire_file;
    (void)sb_printf;
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
    (void)util_segv_handler;
    (void)util_filename_from;
    (void)util_string_int32;
    (void)util_die_notify;
    (void)remove_escape_sequences;
    (void)xfclose;
    (void)xclosedir;
#if OS_UNIX
    (void)util_copy_file_sync;
    (void)util_copy_file_async;
    (void)send_signal;
#endif
    (void)util_equal_files;

    (void)malloc_debug;
    (void)realloc_debug;
    (void)free_debug;

    (void)atoi2;
#if OS_UNIX
    (void)command_run_capture;
    (void)command_run_sync;
    (void)command_result_read_captured;
    (void)command_signal;
    (void)command_wait;
    (void)timezone_init;
#endif
    (void)dirname2;
    (void)basename2;
    (void)string_from_doubles;
    (void)string_from_strings;
    (void)strftime2;
    (void)bytes_pretty;
    (void)qsort64;
    (void)print_timings;
    (void)strncmp32;

    (void)xmmap_commit;
    (void)xstrdup;
#if OS_UNIX
    (void)xkill;
    (void)xdup2;
    (void)xpipe;
#endif
    (void)xmemdup;
    (void)xunlink;

    (void)random_ascii_string;
    (void)strncpy32;
    (void)ends_with;
    (void)rad2deg;
    (void)deg2rad;
    (void)path_basename;
    (void)timediff;
    (void)time_monotonic_coarse;
    (void)time_monotonic_precise;
    (void)catfile;
    (void)parse_option;
    (void)command_print;
    return;
}
#endif

#if TESTING && OS_UNIX
CBASE_API_DEF bool
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

CBASE_API_DEF void
test_join_path(
    char *buffer,
    int64 buffer_len,
    char *dir,
    char *name
) {
    int32 len;

    len = snprintf2(buffer, buffer_len, "%s/%s", dir, name);
    ASSERT(len > 0);
    ASSERT(len < buffer_len);

    return;
}

CBASE_API_DEF void
test_make_temp_dir(char *buffer, int32 capacity, char *name) {
    char *tmpdir;
    int32 len;

    if ((tmpdir = getenv("TMPDIR")) == NULL) {
        tmpdir = "/tmp";
    }

    len = snprintf2(buffer, capacity, "%s/%s_XXXXXX", tmpdir, name);
    if ((len <= 0) || (len >= capacity)) {
        error("Temporary directory path too long.\n");
        fatal(EXIT_FAILURE);
    }
    if (mkdtemp(buffer) == NULL) {
        error("Error creating temporary directory %s: %s.\n",
              buffer, strerror(errno));
        fatal(EXIT_FAILURE);
    }

    return;
}

static void
test_remove_tree_children(char *path) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(path);
    if (dir == NULL) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char child[PATH_MAX];
        int32 len;

        if (strequal(entry->d_name, ".") || strequal(entry->d_name, "..")) {
            continue;
        }

        len = snprintf2(child, SIZEOF(child), "%s/%s", path, entry->d_name);
        if ((len <= 0) || (len >= SIZEOF(child))) {
            error("Test path too long below %s.\n", path);
            fatal(EXIT_FAILURE);
        }
        test_remove_tree(child);
    }

    xclosedir(dir, path);
    return;
}

CBASE_API_DEF void
test_remove_tree(char *path) {
    struct stat statbuf;

    if (lstat(path, &statbuf) < 0) {
        if (errno == ENOENT) {
            return;
        }
        error("Error checking test path %s: %s.\n", path, strerror(errno));
        return;
    }

    if (S_ISDIR(statbuf.st_mode) && !S_ISLNK(statbuf.st_mode)) {
        test_remove_tree_children(path);
        if (rmdir(path) < 0) {
            error("Error removing test directory %s: %s.\n",
                  path, strerror(errno));
        }
    } else if (unlink(path) < 0) {
        error("Error removing test path %s: %s.\n", path, strerror(errno));
    }

    return;
}

CBASE_API_DEF bool
test_symlink_supported(char *dir) {
    char link_path[PATH_MAX];
    int32 len;
    bool supported;

    len = snprintf2(link_path, SIZEOF(link_path), "%s/symlink_probe", dir);
    if ((len <= 0) || (len >= SIZEOF(link_path))) {
        return false;
    }

    unlink(link_path);
    supported = symlink("target", link_path) == 0;
    if (supported) {
        unlink(link_path);
    }

    return supported;
}

CBASE_API_DEF bool
test_hardlink_supported(char *dir) {
    char link_path[PATH_MAX];
    char src_path[PATH_MAX];
    int32 len;
    bool supported;
    int32 fd;

    len = snprintf2(src_path, SIZEOF(src_path), "%s/hardlink_probe_src", dir);
    if ((len <= 0) || (len >= SIZEOF(src_path))) {
        return false;
    }
    len = snprintf2(link_path, SIZEOF(link_path),
                    "%s/hardlink_probe_dst", dir);
    if ((len <= 0) || (len >= SIZEOF(link_path))) {
        return false;
    }

    unlink(src_path);
    unlink(link_path);
    fd = open(src_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        return false;
    }
    write64(fd, "x", 1);
    XCLOSE(&fd, src_path);

    supported = link(src_path, link_path) == 0;
    unlink(link_path);
    unlink(src_path);

    return supported;
}
#endif

#if TESTING_util
#define CBASE_IMPLEMENT
#include "cbase.h"

#define ENUM_NAME WeekDay
#define ENUM_BITFLAGS 0
#define ENUM_PREFIX_ WEEK_DAY_
#define ENUM_FIELDS \
    X(WEEK_DAY_SUNDAY, sunday)    \
    X(WEEK_DAY_MONDAY, monday)    \
    X(WEEK_DAY_TUESDAY, tuesday)  \
    X(WEEK_DAY_WEDNESDAY)         \
    X(WEEK_DAY_THURSDAY)          \
    X(WEEK_DAY_FRIDAY, friday)    \
    X(WEEK_DAY_SATURDAY, saturday)
#include "xenums.c"

#define ENUM_NAME PowerOfTwo
#define ENUM_BITFLAGS 1
#define ENUM_PREFIX_ POWER_OF2_
#define ENUM_FIELDS \
    X(POWER_OF2_ONE)     \
    X(POWER_OF2_TWO)     \
    X(POWER_OF2_FOUR)    \
    X(POWER_OF2_EIGHT)   \
    X(POWER_OF2_SIXTEEN) \
    X(POWER_OF2_THIRTY2)
#include "xenums.c"

static void
write_file(char *path, void *data, int64 len) {
    int fd;

    if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
        error("Error opening %s: %s.\n", path, strerror(errno));
        fatal(EXIT_FAILURE);
    }
    if (write64(fd, data, len) != len) {
        error("Error in write: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    }
    XCLOSE(&fd, path);
    return;
}

#define WRITE_FILE(PATH, STRING) \
    write_file(PATH, STRING, strlen32(STRING))

static sig_atomic_t received_signal = false;
static void
signal_handler(int signal_number) {
    (void)signal_number;
    received_signal = true;
    return;
}

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

int
main(int argc, char **argv) {
    char *s1 = "aaaabbbb";
    struct timespec t0;
    struct timespec t1;
    char temp_dir[PATH_MAX];

    (void)argc;
    (void)argv;
    (void)here_counter;

#if TESTING && OS_UNIX
    test_make_temp_dir(temp_dir, SIZEOF(temp_dir), "util");
#endif

    ASSERT(BEGINS_WITH(s1, strlen32(s1), "aaaa"));
    ASSERT(BEGINS_WITH(s1, strlen32(s1), "aaaabbbb"));
    ASSERT(!BEGINS_WITH(s1, strlen32(s1), "bbbb"));
    ASSERT(!BEGINS_WITH(s1, strlen32(s1), "aaaabbbbb"));

    ASSERT(ENDS_WITH(s1, strlen32(s1), "bbbb"));
    ASSERT(ENDS_WITH(s1, strlen32(s1), "aaaabbbb"));
    ASSERT(!ENDS_WITH(s1, strlen32(s1), "aaaa"));
    ASSERT(!ENDS_WITH(s1, strlen32(s1), "aaaaabbbbb"));

    ASSERT(striqual("abc", "ABC"));
    ASSERT(striqual("ASCII 123 _-", "ascii 123 _-"));
    ASSERT(!striqual("abc", "abd"));
    ASSERT(!striqual("abc", "abcd"));

    ASSERT(STRIQUAL(s1, strlen32(s1), "AAAABBBB"));
    ASSERT(STRIQUAL(s1 + 4, 4, "BBBB"));
    ASSERT(STRIQUAL("MiXeD", 5, "mixed", 5));
    ASSERT(!STRIQUAL("MiXeD", 4, "mixed", 5));
    ASSERT(!STRIQUAL("MiXeD", 5, "match", 5));

    {
        StrBuilder builder = {0};
        int32 old_cap;

        SB_APPEND(&builder, "0123456789abcde");
        old_cap = builder.cap;
        sb_append(&builder, builder.data + 1, builder.len - 1);
        ASSERT_MORE(builder.cap, old_cap);
        ASSERT_EQUAL(builder.data,
                     "0123456789abcde123456789abcde");
        sb_free(&builder);
    }

    time_monotonic_precise(&t0);
#if OS_UNIX
    timezone_init();
#endif

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

    for (enum WeekDay day = WEEK_DAY_MONDAY; day <= WEEK_DAY_LAST; day += 1) {
        printf("enum[%u] = %s\n", day, WEEK_DAY_str(day));
    }

    printf("\n");

    for (uint x = 0; x < POWER_OF2_LAST; x += 1) {
        char *value_name = POWER_OF2_str((enum PowerOfTwo)x);
        printf("enum[%u] = %s\n", x, value_name);
        POWER_OF2_str_free(value_name);
    }

    if (OS_LINUX && !DEBUGGING) {
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

    srand((uint)time(NULL));
    for (int i = 0; i < 10; i += 1) {
        int n = rand() - RAND_MAX / 2;
        char itoa_buffer[32];
        ITOA(itoa_buffer, n);
        ASSERT_EQUAL(atoi2(itoa_buffer), n);
    }

    {
        int32 n;
        ASSERT_EQUAL(util_string_int32(&n, "12345"), 0);
        ASSERT_EQUAL(n, 12345);
        ASSERT_EQUAL(util_string_int32(&n, "-54321"), 0);
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
        char b[64];
        char *strs[] = {"one", "two", "three"};
        double dbls[] = {1.1, 2.2};
        string_from_strings(b, sizeof(b), "|", strs, 3);
        ASSERT_EQUAL(b, "one|two|three");
        string_from_doubles(b, sizeof(b), ",", dbls, 2);
        ASSERT_NOT_EQUAL(strlen32(b), 0);
    }

    {
        char *src = "memdup_test";
        char *dup = xmemdup(src, 12);
        ASSERT_EQUAL(src, dup);
        ASSERT_NOT_EQUAL((void *)src, (void *)dup);
        free2(dup, 12);
    }

    {
        char b[128];
        struct tm fixed_time;
        fixed_time.tm_year = 126; // 2026
        fixed_time.tm_mon = 2;   // March
        fixed_time.tm_mday = 25;
        fixed_time.tm_hour = 12;
        fixed_time.tm_min = 0;
        fixed_time.tm_sec = 0;
        strftime2(b, sizeof(b), "%Y-%m-%d", &fixed_time);
        ASSERT_EQUAL(b, "2026-03-25");
    }

    {
        char paths[][30] = {
            "/aaaa/bbbb/cccc", "/aa/bb/cc",  "/a/b/c",    "a/b//c",
            "a/b/cccc",        "a/bb/cccc", "aaaa//cccc", "/aaaa",
            "/",               "//",          "//a/",        "/a/b///",
            "./",              "..",          "././",      "./a/",
        };
        char *bases[20] = {
            "cccc",            "cc",          "c",         "c",
            "cccc",            "cccc",        "cccc",      "aaaa",
            "/",               "/",           "a/",        "b/",
            "./",              "..",          "./",        "a/",
        };
        char *dirs[20] = {
            "/aaaa/bbbb",      "/aa/bb",      "/a/b",      "a/b",
            "a/b",              "a/bb",        "aaaa",      "/",
            "/",               "/",           "/",         "/a",
            ".",               ".",           ".",         ".",
        };
        char *normalized[20] = {
            "/aaaa/bbbb/cccc", "/aa/bb/cc",   "/a/b/c",    "a/b/c",
            "a/b/cccc",        "a/bb/cccc",   "aaaa/cccc", "/aaaa",
            "/",               "/",           "/a/",        "/a/b/",
            "./",              "..",          "./",        "a/",
        };
        for (int64 i = 0; i < LENGTH(paths); i += 1) {
            int32 path_len0 = strlen32(paths[i]);
            char *path = xstrdup(paths[i]);
            char *base = bases[i];
            int32 path_len = strlen32(path);
            ASSERT_EQUAL(basename2(path, &path_len, NULL), base);
            free2(path, path_len0 + 1);
        }
        for (int64 i = 0; i < LENGTH(paths); i += 1) {
            char *copy = xstrdup(paths[i]);
            int len = strlen32(copy);
            int len0 = strlen32(copy);
            normalize(copy, &len);
            ASSERT_EQUAL(copy, normalized[i]);
            free2(copy, len0 + 1);
        }

        for (int64 i = 0; i < LENGTH(paths); i += 1) {
            char dir_buffer[4096];
            int32 path_len = strlen32(paths[i]);
            dirname2(dir_buffer, paths[i], &path_len);
            ASSERT_EQUAL(dir_buffer, dirs[i]);
        }
        {
            char dir_buffer[128] = "a/b/c";
            int32 path_len = strlen32(dir_buffer);
            dirname2(dir_buffer, dir_buffer, &path_len);
            ASSERT_EQUAL(dir_buffer, "a/b");
        }
    }

    if (OS_WINDOWS) {
        char *path2 = "aa\\cc";
        int32 path_len;
        ASSERT_EQUAL(basename2(path2, &path_len, NULL), "cc");
    }

    {
        char a[PATH_MAX];
        char b[PATH_MAX];

        SNPRINTF(a, "%s/afile", temp_dir);
        SNPRINTF(b, "%s/bfile", temp_dir);

        WRITE_FILE(a, "hello world");
        WRITE_FILE(b, "hello world");
        ASSERT(util_equal_files(a, b));

        WRITE_FILE(a, "hello world");
        WRITE_FILE(b, "hello worlx");
        ASSERT(!util_equal_files(a, b));

        WRITE_FILE(a, "short");
        WRITE_FILE(b, "shorter");
        ASSERT(!util_equal_files(a, b));

        WRITE_FILE(a, "");
        WRITE_FILE(b, "");
        ASSERT(util_equal_files(a, b));
    }

    {
        char characters[] = "abcdefghijklmnopqrstuvwxyz1234567890";
        char buffer2[4096];
        char name2[256];
        char buffer3[4096];
        char buffer4[4096];
        char name[PATH_MAX];
        int fd;

        SNPRINTF(name, "%s/test", temp_dir);

        if ((fd = open(name,
                       O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
            error("Error opening %s: %s.\n", name, strerror(errno));
            fatal(EXIT_FAILURE);
        }

        if (util_filename_from(buffer2, sizeof(buffer2), fd) == 0) {
            ASSERT(realpath(name, buffer3) != NULL);
            ASSERT_EQUAL(buffer3, buffer2);
        }
        xunlink(name);

        XCLOSE(&fd);

        for (int32 i = 0; i < (SIZEOF(name2) - 1); i += 1) {
            uint32 c = (uint32)rand() % (sizeof(characters) - 1);
            name2[i] = characters[c];
        }
        name2[SIZEOF(name2) - 1] = '\0';

        SNPRINTF(buffer2, "%s/%s", temp_dir, name2);

        if ((fd = open(buffer2,
                       O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
            error("Error opening %s: %s.\n", buffer2, strerror(errno));
            fatal(EXIT_FAILURE);
        }

        if (util_filename_from(buffer4, sizeof(buffer4), fd) == 0) {
            ASSERT(realpath(buffer2, buffer3) != NULL);
            ASSERT_EQUAL(buffer3, buffer4);
        }
        XCLOSE(&fd);
        xunlink(buffer2);
    }

    ASSERT_EQUAL(deg2rad(180.0), 3.141592653589793);
    ASSERT_EQUAL(rad2deg(3.141592653589793), 180.0);
    ASSERT_MORE(util_nthreads(), 0);

    ASSERT_EQUAL(CLAMP(0.0, -0.1, 0.1),   0.0);
    ASSERT_EQUAL(CLAMP(0.2, -0.1, 0.1),   0.1);
    ASSERT_EQUAL(CLAMP(-0.2, -0.1, 0.1), -0.1);

    ASSERT_EQUAL(CLAMP(+0, -1, +1), +0);
    ASSERT_EQUAL(CLAMP(+2, -1, +1), +1);
    ASSERT_EQUAL(CLAMP(-2, -1, +1), -1);

    NCALLS(1);

    (void)util_segv_handler;
    (void)util_die_notify;
#if OS_UNIX
    (void)util_copy_file_sync;
    (void)util_copy_file_async;
#endif

    (void)malloc_debug;
    (void)realloc_debug;
    (void)free_debug;
    (void)free2_;

    (void)xmmap_commit;
    (void)xkill;
    (void)xdup2;
    (void)xpipe;
    (void)xunlink;

    (void)fwrite64;
    (void)fread64;

#if TESTING && OS_UNIX
    test_remove_tree(temp_dir);
#endif

    time_monotonic_precise(&t1);
    PRINT_TIMINGS(1, t0, t1);
    exit(EXIT_SUCCESS);
}

#endif

#endif /* UTIL_C */
