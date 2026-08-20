// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(FS_C)
#define FS_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_fs 1
#elif !defined(TESTING_fs)
#define TESTING_fs 0
#endif

#include "cbase.h"

void
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

#define RW_FUNCTION_LINKAGE 
#define RW_FUNCTION write
#include "rw_function.h"

#define RW_FUNCTION_LINKAGE 
#define RW_FUNCTION read
#include "rw_function.h"

bool
util_filename_from(char *buffer, int64 size, int fd) {
#if OS_LINUX
    char linkpath[64];
    ssize_t len;

    SNPRINTF(linkpath, "/proc/self/fd/%d", fd);
    if ((len = readlink(linkpath, buffer, (size_t)(size - 1))) < 0) {
        return false;
    }
    buffer[len] = '\0';
    return true;
#elif CBASE_HAS_F_GETPATH
    static char buffer2[MAXPATHLEN];
    int64 len;

    if (fcntl(fd, F_GETPATH, buffer2) < 0) {
        return false;
    }
    len = MIN(strlen32(buffer2), size - 1);
    memcpy64(buffer, buffer2, len + 1);
    buffer[len] = '\0';
    return true;
#elif OS_WINDOWS
    HANDLE h;
    DWORD len;
    intptr h2 = _get_osfhandle(fd);

    if ((h = (HANDLE)h2) == INVALID_HANDLE_VALUE) {
        return false;
    }

    len = GetFinalPathNameByHandleA(h, buffer, (DWORD)size,
                                    FILE_NAME_NORMALIZED);

    if ((len <= 0) || (len >= size)) {
        return false;
    }

    if (strncmp32(buffer, "\\\\?\\", 4) == 0) {
        memmove64(buffer, buffer + 4, len - 3);
    }

    return true;
#else
    (void)size;
    (void)fd;
    (void)buffer;
    return false;
#endif
}

#if CBASE_CRT_MSVC
char *
realpath(char *path, char *resolved_path) {
    char *buffer;
    char *result;

    if (path == NULL) {
        errno = EINVAL;
        return NULL;
    }

    buffer = resolved_path;
    if ((buffer == NULL) && ((buffer = malloc(PATH_MAX)) == NULL)) {
        errno = ENOMEM;
        return NULL;
    }

    result = _fullpath(buffer, path, PATH_MAX);
    if (result != NULL) {
        char long_buffer[PATH_MAX];
        DWORD capacity = (DWORD)SIZEOF(long_buffer);
        DWORD len;

        len = GetLongPathNameA(buffer, long_buffer, capacity);
        if ((len > 0) && (len < capacity)) {
            memcpy64(buffer, long_buffer, len + 1);
        }
    }
    if ((result == NULL) && (resolved_path == NULL)) {
        free(buffer);
    }

    return result;
}
#endif

#if OS_WINDOWS
static int
strerror_r(int errnum, char *buffer, size_t size) {
    char *error_message = strerror(errnum);
    int32 len = strlen32(error_message);

    memcpy64(buffer, error_message, MIN(len + 1, size - 1));
    buffer[size - 1] = '\0';

    return 0;
}
#endif

int
xclose(char *file, int line, int *fd, char *fd_var_name, char *filename) {
#if DEBUGGING
    char buffer[4096];

    if (filename == NULL) {
        if (util_filename_from(buffer, sizeof(buffer), *fd)) {
            filename = buffer;
        } else {
            filename = fd_var_name;
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

int
xunlink(char *filename) {
    if (unlink(filename) < 0) {
        error2("Error in unlink(%s): %s.\n", filename, strerror(errno));
        return -1;
    }
    return 0;
}

FILE *
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

int
xfclose(char *file, int32 line, char *func, FILE *f, char *filename) {
    if (fclose(f)) {
        error_impl(file, line, func,
                   "Error closing %s: %s.\n", filename, strerror(errno));
        return -1;
    }
    return 0;
}

#if HAS_POSIX_WIN_SUBSET
int
xclosedir(DIR *dir, char *dirname) {
    if (closedir(dir)) {
        error2("Error closing directory %s: %s.\n", dirname, strerror(errno));
        return -1;
    }
    return 0;
}
#endif

#if OS_UNIX
int32
util_copy_file_sync(char *destination, char *source) {
    int32 source_fd;
    int32 destination_fd;
    char buffer[BUFSIZ];
    int64 r = 0;
    int64 w = 0;

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

int32
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

void
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

#if !OS_WINDOWS
bool
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
bool
util_file_exists(char *filename) {
    DWORD attributes;

    attributes = GetFileAttributesA(filename);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    return true;
}
#endif

bool
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
    if ((stat_a.st_ino != 0)
        && (stat_a.st_dev == stat_b.st_dev)
        && (stat_a.st_ino == stat_b.st_ino)) {
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

void
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

char *
basename2(char *path, int32 *full_length, int32 *base_len) {
    int32 left;
    char *end;
    char *fslash = NULL;
    char *bslash = NULL;
    char *p = path;

    normalize(path, full_length);

    left = *full_length;
    ASSERT_POSITIVE(*full_length);
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

        ASSERT(length < MAXOF(left));
        left -= (int32)length;
    }

    if (base_len) {
        *base_len = *full_length;
    }
    return path;
}

char *
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

int32
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

void
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

bool
path_missing(char *path) {
    if (path == NULL) {
        return true;
    }
    if (path[0] == '\0') {
        return true;
    }

    return false;
}

bool
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

bool
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

#if 0 == TESTING_fs
static inline void
fs_functions_sink(void) {
    (void)fs_functions_sink;
    (void)basename2;
    (void)catfile;
    (void)dirname2;
    (void)fread64;
    (void)fwrite64;
    (void)normalize;
    (void)path_basename;
    (void)path_missing;
    (void)read64;
    (void)read_entire_file;
    (void)util_equal_files;
    (void)util_file_exists;
    (void)util_filename_from;
    (void)write64;
    (void)write_all;
    (void)write_entire_file;
    (void)xclose;
    (void)xfclose;
    (void)xfopen;
    (void)xunlink;
#if HAS_POSIX_WIN_SUBSET
    (void)xclosedir;
#endif
#if OS_UNIX
    (void)util_copy_file_async;
    (void)util_copy_file_async_parsed;
    (void)util_copy_file_sync;
#endif
    return;
}
#endif

#if TESTING
void
test_join_path(
    char *buffer,
    int64 buffer_len,
    char *dir,
    char *name
) {
    int32 len;

    len = snprintf2(buffer, buffer_len, "%s/%s", dir, name);
    ASSERT_POSITIVE(len);
    ASSERT(len < buffer_len);

    return;
}

void
test_make_temp_dir(char *buffer, int32 capacity, char *name) {
#if OS_UNIX
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
#elif OS_WINDOWS
    DWORD temp_len;
    int32 prefix_len;

    temp_len = GetTempPathA((DWORD)capacity, buffer);
    if ((temp_len <= 0) || (temp_len >= (DWORD)capacity)) {
        error("Temporary directory path too long.\n");
        fatal(EXIT_FAILURE);
    }

    prefix_len = (int32)temp_len;
    for (int32 i = 0; i < 10000; i += 1) {
        DWORD error_code;
        int32 len;

        len = snprintf2(buffer + prefix_len, capacity - prefix_len,
                        "%s_%lu_%d",
                        name, (ulong)GetCurrentProcessId(), i);
        if ((len <= 0) || (len >= (capacity - prefix_len))) {
            error("Temporary directory path too long.\n");
            fatal(EXIT_FAILURE);
        }
        if (CreateDirectoryA(buffer, NULL)) {
            return;
        }

        error_code = GetLastError();
        if (error_code != ERROR_ALREADY_EXISTS) {
            error("Error creating temporary directory %s: windows error %lu.\n",
                  buffer, (ulong)error_code);
            fatal(EXIT_FAILURE);
        }
    }

    error("Could not create a unique temporary directory.\n");
    fatal(EXIT_FAILURE);
#else
    (void)buffer;
    (void)capacity;
    (void)name;
    error("Temporary test directories are unsupported on this platform.\n");
    fatal(EXIT_FAILURE);
#endif
}

#if OS_UNIX
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
#endif

void
test_remove_tree(char *path) {
#if OS_UNIX
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
#elif OS_WINDOWS
    WIN32_FIND_DATAA find_data;
    HANDLE find_handle;
    DWORD attributes;
    DWORD error_code;

    attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        error_code = GetLastError();
        if ((error_code == ERROR_FILE_NOT_FOUND)
            || (error_code == ERROR_PATH_NOT_FOUND)) {
            return;
        }

        error("Error checking test path %s: windows error %lu.\n",
              path, (ulong)error_code);
        return;
    }

    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        if (!(attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            char pattern[PATH_MAX];
            int32 len;

            len = snprintf2(pattern, SIZEOF(pattern), "%s/*", path);
            if ((len <= 0) || (len >= SIZEOF(pattern))) {
                error("Test path too long below %s.\n", path);
                fatal(EXIT_FAILURE);
            }

            find_handle = FindFirstFileA(pattern, &find_data);
            if (find_handle == INVALID_HANDLE_VALUE) {
                error_code = GetLastError();
                if (error_code != ERROR_FILE_NOT_FOUND) {
                    error("Error reading test directory %s: windows error "
                          "%lu.\n", path, (ulong)error_code);
                    return;
                }
            } else {
                do {
                    char child[PATH_MAX];

                    if (strequal(find_data.cFileName, ".")
                        || strequal(find_data.cFileName, "..")) {
                        continue;
                    }

                    len = snprintf2(child, SIZEOF(child), "%s/%s",
                                    path, find_data.cFileName);
                    if ((len <= 0) || (len >= SIZEOF(child))) {
                        error("Test path too long below %s.\n", path);
                        fatal(EXIT_FAILURE);
                    }
                    test_remove_tree(child);
                } while (FindNextFileA(find_handle, &find_data));

                error_code = GetLastError();
                if ((error_code != ERROR_NO_MORE_FILES)
                    && (error_code != ERROR_SUCCESS)) {
                    error("Error reading test directory %s: windows error "
                          "%lu.\n", path, (ulong)error_code);
                }
                if (!FindClose(find_handle)) {
                    error("Error closing test directory %s: windows error "
                          "%lu.\n", path, (ulong)GetLastError());
                }
            }
        }

        if (RemoveDirectoryA(path)) {
            return;
        }

        error_code = GetLastError();
        if ((error_code == ERROR_FILE_NOT_FOUND)
            || (error_code == ERROR_PATH_NOT_FOUND)) {
            return;
        }

        error("Error removing test directory %s: windows error %lu.\n",
              path, (ulong)error_code);
        return;
    }

    if (DeleteFileA(path)) {
        return;
    }

    error_code = GetLastError();
    if ((error_code == ERROR_FILE_NOT_FOUND)
        || (error_code == ERROR_PATH_NOT_FOUND)) {
        return;
    }

    error("Error removing test path %s: windows error %lu.\n",
          path, (ulong)error_code);
    return;
#else
    (void)path;
    return;
#endif
}

#if OS_UNIX
bool
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

bool
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
#endif

#if OS_WINDOWS
#if !defined(WINDOWS_FUNCTIONS_C)
#define WINDOWS_FUNCTIONS_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_windows_functions 1
#elif !defined(TESTING_windows_functions)
#define TESTING_windows_functions 0
#endif

#include "cbase.h"

#if !OS_WINDOWS
#error "ONLY INCLUDE THIS FILE IF COMPILING FOR WINDOWS"
#endif

#if !defined(S_IFLNK)
#define S_IFLNK 0120000
#endif

#if CBASE_CRT_MSVC
static int
mkstemp(char *template) {
    char characters[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int64 len;

    if (template == NULL) {
        errno = EINVAL;
        return -1;
    }

    len = strlen32(template);
    if ((len < 6) || (memcmp64(template + len - 6, "XXXXXX", 6) != 0)) {
        errno = EINVAL;
        return -1;
    }

    for (int32 attempt = 0; attempt < 100; attempt += 1) {
        uint32 state;

        state = (uint32)GetCurrentProcessId();
        state ^= (uint32)GetCurrentThreadId();
        state ^= (uint32)GetTickCount64();
        state ^= (uint32)(attempt*2654435761u);
        for (int32 i = 0; i < 6; i += 1) {
            state = state*1103515245u + 12345u;
            template[len - 6 + i]
                = characters[state % (SIZEOF(characters) - 1)];
        }

        {
            int fd = open(template, O_RDWR |O_CREAT |O_EXCL |O_BINARY,
                          S_IREAD |S_IWRITE);
            if (fd >= 0) {
                return fd;
            }
            if (errno != EEXIST) {
                return -1;
            }
        }
    }

    errno = EEXIST;
    return -1;
}
#endif

static time_t
filetime_to_time_t(FILETIME *filetime) {
    ULARGE_INTEGER u_large_integer;
    u_large_integer.LowPart = filetime->dwLowDateTime;
    u_large_integer.HighPart = filetime->dwHighDateTime;
    // Convert from Windows epoch (1601) to Unix epoch (1970)
    return (time_t)(
        (u_large_integer.QuadPart - 116444736000000000ULL) / 10000000ULL);
}

static int
lstat(const char *path, struct stat *statbuf) {
    wchar_t *wide_path;
    WIN32_FILE_ATTRIBUTE_DATA fd;
    ULARGE_INTEGER ull_size;
    DWORD error_code;
    int64 wide_path_size;
    int32 wide_path_length;

    if ((path == NULL) || (statbuf == NULL)) {
        errno = EINVAL;
        return -1;
    }

    wide_path_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           path, -1, NULL, 0);
    if (wide_path_length <= 0) {
        windows_set_errno(GetLastError());
        return -1;
    }

    wide_path_size = (int64)wide_path_length*SIZEOF(*wide_path);
    wide_path = malloc2(wide_path_size);
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
                            wide_path, wide_path_length)
        != wide_path_length) {
        error_code = GetLastError();
        free2(wide_path, wide_path_size);
        windows_set_errno(error_code);
        return -1;
    }

    if (!GetFileAttributesExW(wide_path, GetFileExInfoStandard, &fd)) {
        error_code = GetLastError();
        free2(wide_path, wide_path_size);
        windows_set_errno(error_code);
        return -1;
    }

    memset64(statbuf, 0, SIZEOF(*statbuf));

    // File type
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        FILE_ATTRIBUTE_TAG_INFO tag_info;
        HANDLE file_handle;

        file_handle = CreateFileW(
            wide_path,
            0,
            FILE_SHARE_READ |FILE_SHARE_WRITE |FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS |FILE_FLAG_OPEN_REPARSE_POINT,
            NULL);
        if (file_handle == INVALID_HANDLE_VALUE) {
            error_code = GetLastError();
            free2(wide_path, wide_path_size);
            windows_set_errno(error_code);
            return -1;
        }

        if (!GetFileInformationByHandleEx(file_handle,
                                          FileAttributeTagInfo,
                                          &tag_info,
                                          (DWORD)SIZEOF(tag_info))) {
            error_code = GetLastError();
            CloseHandle(file_handle);
            free2(wide_path, wide_path_size);
            windows_set_errno(error_code);
            return -1;
        }

        if (!CloseHandle(file_handle)) {
            error_code = GetLastError();
            free2(wide_path, wide_path_size);
            windows_set_errno(error_code);
            return -1;
        }

        if (tag_info.ReparseTag == IO_REPARSE_TAG_SYMLINK) {
            statbuf->st_mode = S_IFLNK;
        } else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            statbuf->st_mode = S_IFDIR;
        } else {
            statbuf->st_mode = S_IFREG;
        }
    } else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        statbuf->st_mode = S_IFDIR;
    } else {
        statbuf->st_mode = S_IFREG;
    }

    free2(wide_path, wide_path_size);

    ull_size.LowPart = fd.nFileSizeLow;
    ull_size.HighPart = fd.nFileSizeHigh;
    if (ull_size.QuadPart > LONG_MAX) {
        error("Warning: file is too large, size will be wrong.\n");
    }
    statbuf->st_size = (long)ull_size.QuadPart;

    // Convert FILETIME -> time_t
    statbuf->st_mtime = filetime_to_time_t(&fd.ftLastWriteTime);
    statbuf->st_ctime = filetime_to_time_t(&fd.ftCreationTime);
    statbuf->st_atime = filetime_to_time_t(&fd.ftLastAccessTime);

    return 0;
}

#if TESTING_windows_functions
#define CBASE_IMPLEMENT
#include "cbase.h"

static bool
contains(
    char *buffer,
    int64 length,
    DirEntry *directory_entries,
    int32 *number_files
) {
    for (int32 i = 0; i < *number_files; i += 1) {
        char *from_scan = directory_entries[i].name;

        if (strlen32(from_scan) != length) {
            continue;
        }

        if (!memcmp64(buffer, from_scan, length)) {
            printf("%s == %s\n", buffer, from_scan);
            *number_files -= 1;
            if (i < *number_files) {
                memmove64(&directory_entries[i], &directory_entries[i + 1],
                          (*number_files - i)*SIZEOF(*directory_entries));
            }
            return true;
        }
    }
    return false;
}

int
main(void) {
    {
        char *string = "aaa/bbb/ccc";
        int64 length = strlen32(string);

        ASSERT(memmem64(string, length, "aaa", 3) == string);
        ASSERT(memmem64(string, length, "bbb", 3) == string + 4);
        ASSERT(memmem64(string, length, "aaaa", 4) == NULL);
        ASSERT(memmem64(string, length, "bbbb", 4) == NULL);
        ASSERT(memmem64(string, length, "/", 1) == string + 3);
    }

    {
        struct stat stat;
        ASSERT(lstat("LICENSE", &stat) == 0);
        ASSERT(stat.st_size == 34523);
        ASSERT(stat.st_mtime == 1735689600);
        ASSERT(stat.st_ctime == 1735689600);
        error("stat.atime: %lld\n", stat.st_atime);
    }

    {
        DirEntry *dirent;
        FILE *ls_pipe;
        char buffer[1024];
        int32 dirent_capacity;
        int32 nfiles;

        if ((nfiles = get_directory_entries("./", &dirent)) <= 0) {
            error("Error in scandir for windows.\n");
            fatal(EXIT_FAILURE);
        }
        dirent_capacity = nfiles;

        if ((ls_pipe = popen("dir /b", "r")) == NULL) {
            error("Error in popen: %s.\n", strerror(errno));
            fatal(EXIT_FAILURE);
        }
        while (fgets(buffer, SIZEOF(buffer), ls_pipe)) {
            int64 length = strlen32(buffer);

            if ((length > 0) && (buffer[length - 1] == '\n')) {
                length -= 1;
            }
            buffer[length] = '\0';
            ASSERT(contains(buffer, length, dirent, &nfiles));
        }

        free2(dirent, (int64)dirent_capacity*SIZEOF(*dirent));
    }

    exit(EXIT_SUCCESS);
}
#endif

#endif
#endif

#if TESTING_fs
#define CBASE_IMPLEMENT
#include "cbase.h"

static void
fs_test_write_file(char *path, void *data, int64 len) {
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
    fs_test_write_file(PATH, STRING, strlen32(STRING))

int
main(void) {
    char temp_dir[PATH_MAX];

    test_make_temp_dir(temp_dir, SIZEOF(temp_dir), "fs");

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
        char path2[] = "aa\\cc";
        int32 path_len = strlen32(path2);
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
        char path[PATH_MAX];
        char *contents;
        int32 contents_len;

        SNPRINTF(path, "%s/whole_file", temp_dir);
        ASSERT(path_missing(NULL));
        ASSERT(path_missing(""));
        ASSERT(!path_missing(path));
        ASSERT(!util_file_exists(path));

        ASSERT(write_entire_file(path, "abcdef", 6));
        ASSERT(util_file_exists(path));
        ASSERT(read_entire_file(path, &contents, &contents_len));
        ASSERT_EQUAL(contents_len, 6);
        ASSERT_EQUAL(contents, "abcdef");
        free2(contents, contents_len + 1);
    }

#if OS_LINUX || CBASE_HAS_F_GETPATH || CBASE_CRT_MSVC
    {
        char characters[] = "abcdefghijklmnopqrstuvwxyz1234567890";
        char buffer2[4096];
        char name2[256];
        char buffer3[4096];
        char buffer4[4096];
        char name[PATH_MAX];
        int32 name2_len;
        int fd;

        SNPRINTF(name, "%s/test", temp_dir);

        if ((fd = open(name,
                       O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
            error("Error opening %s: %s.\n", name, strerror(errno));
            fatal(EXIT_FAILURE);
        }

        util_filename_from(buffer2, sizeof(buffer2), fd);
        ASSERT_EQUAL(realpath(name, buffer3), buffer2);
        XCLOSE(&fd);
        xunlink(name);

        name2_len = SIZEOF(name2) - 1;
#if OS_WINDOWS
        name2_len = MIN(name2_len, MAX_PATH - strlen32(temp_dir) - 2);
        ASSERT_POSITIVE(name2_len);
#endif
        for (int32 i = 0; i < name2_len; i += 1) {
            uint32 c = (uint32)rand_int() % (sizeof(characters) - 1);
            name2[i] = characters[c];
        }
        name2[name2_len] = '\0';

        SNPRINTF(buffer2, "%s/%s", temp_dir, name2);

        if ((fd = open(buffer2,
                       O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
            error("Error opening %s: %s.\n", buffer2, strerror(errno));
            fatal(EXIT_FAILURE);
        }

        util_filename_from(buffer4, sizeof(buffer4), fd);
        ASSERT_EQUAL(realpath(buffer2, buffer3), buffer4);
        XCLOSE(&fd);
        xunlink(buffer2);
    }
#endif

#if OS_UNIX
    (void)test_hardlink_supported;
    (void)test_symlink_supported;
    (void)util_copy_file_sync;
    (void)util_copy_file_async;
#endif

    (void)catfile;
    (void)fread64;
    (void)fwrite64;
    (void)path_basename;
    (void)write_all;

    test_remove_tree(temp_dir);

    exit(EXIT_SUCCESS);
}

#endif

#endif /* FS_C */
