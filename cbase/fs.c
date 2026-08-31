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

#if OS_WINDOWS
#include "fs_windows.c"
#endif

#if !OS_WINDOWS
char *
cbase_mkdtemp(char *template) {
#if OS_UNIX
    return mkdtemp(template);
#else
    (void)template;
    errno = ENOSYS;
    return NULL;
#endif
}
#endif

int32
cbase_mkdir(char *path) {
#if OS_WINDOWS
    if (CreateDirectoryA(path, NULL)) {
        return 0;
    }
    windows_set_errno(GetLastError());
    return -1;
#elif HAS_POSIX_WIN_SUBSET
    return mkdir(path, 0777);
#else
    (void)path;
    errno = ENOSYS;
    return -1;
#endif
}

int32
cbase_rmdir(char *path) {
#if OS_WINDOWS
    if (RemoveDirectoryA(path)) {
        return 0;
    }
    windows_set_errno(GetLastError());
    return -1;
#elif HAS_POSIX_WIN_SUBSET
    return rmdir(path);
#else
    (void)path;
    errno = ENOSYS;
    return -1;
#endif
}

int32
cbase_unlink(char *path) {
#if OS_WINDOWS
    if (DeleteFileA(path)) {
        return 0;
    }
    windows_set_errno(GetLastError());
    return -1;
#elif HAS_POSIX_WIN_SUBSET
    return unlink(path);
#else
    (void)path;
    errno = ENOSYS;
    return -1;
#endif
}

int32
cbase_remove_file(char *path) {
    return cbase_unlink(path);
}

int32
cbase_remove_empty_dir(char *path) {
    return cbase_rmdir(path);
}

int32
cbase_mkstemps(char *template_path, int32 suffix_len) {
    char characters[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int64 len;
    int64 x_start;

    if ((template_path == NULL) || (suffix_len < 0)) {
        errno = EINVAL;
        return -1;
    }

    len = strlen32(template_path);
    if (suffix_len > len - 6) {
        errno = EINVAL;
        return -1;
    }

    x_start = len - suffix_len - 6;
    if (memcmp64(template_path + x_start, "XXXXXX", 6) != 0) {
        errno = EINVAL;
        return -1;
    }

    for (int32 attempt = 0; attempt < 10000; attempt += 1) {
        uint32 state;
        int32 fd;
        int32 flags = O_RDWR |O_CREAT |O_EXCL;

#if defined(O_BINARY)
        flags |= O_BINARY;
#endif

        state = (uint32)time(NULL);
        state ^= (uint32)(uintptr_t)template_path;
        state ^= (uint32)attempt*2654435761u;
#if OS_WINDOWS
        state ^= (uint32)GetCurrentProcessId();
        state ^= (uint32)GetCurrentThreadId();
        state ^= (uint32)GetTickCount64();
#elif HAS_POSIX_WIN_SUBSET
        state ^= (uint32)getpid();
#endif

        for (int32 i = 0; i < 6; i += 1) {
            state = state*1103515245u + 12345u;
            template_path[x_start + i]
                = characters[state % (SIZEOF(characters) - 1)];
        }

        fd = open(template_path, flags, S_IRUSR |S_IWUSR);
        if (fd >= 0) {
            return fd;
        }
        if (errno != EEXIST) {
            return -1;
        }
    }

    errno = EEXIST;
    return -1;
}

int32
cbase_make_temp_file(
    char *buffer,
    int32 capacity,
    char *prefix,
    char *suffix
) {
#if OS_UNIX || OS_WINDOWS
    char *tmpdir;
    int32 len;
    int32 suffix_len;
    int32 prefix_len = 0;

    if ((buffer == NULL) || (capacity <= 0) || (prefix == NULL)) {
        errno = EINVAL;
        return -1;
    }
    if (suffix == NULL) {
        suffix = "";
    }
    suffix_len = strlen32(suffix);

    tmpdir = getenv("TMPDIR");
#if OS_WINDOWS
    if (tmpdir == NULL) {
        DWORD temp_len;

        temp_len = GetTempPathA((DWORD)capacity, buffer);
        if ((temp_len <= 0) || (temp_len >= (DWORD)capacity)) {
            windows_set_errno(GetLastError());
            return -1;
        }
        prefix_len = (int32)temp_len;
        len = snprintf2(buffer + prefix_len, capacity - prefix_len,
                        "%s_XXXXXX%s", prefix, suffix);
    } else {
        len = snprintf2(buffer, capacity, "%s/%s_XXXXXX%s",
                        tmpdir, prefix, suffix);
    }
#else
    if (tmpdir == NULL) {
        tmpdir = "/tmp";
    }
    len = snprintf2(buffer, capacity, "%s/%s_XXXXXX%s",
                    tmpdir, prefix, suffix);
#endif
    if ((len <= 0) || (len >= (capacity - prefix_len))) {
        errno = ENAMETOOLONG;
        return -1;
    }

    return cbase_mkstemps(buffer, suffix_len);
#else
    (void)buffer;
    (void)capacity;
    (void)prefix;
    (void)suffix;
    errno = ENOSYS;
    return -1;
#endif
}

char *
cbase_getcwd(char *buffer, int64 size) {
    if (size <= 0) {
        errno = EINVAL;
        return NULL;
    }

#if OS_WINDOWS
    {
        DWORD len;

        if (size > MAXOF((DWORD)0)) {
            errno = EINVAL;
            return NULL;
        }

        len = GetCurrentDirectoryA((DWORD)size, buffer);
        if (len == 0) {
            windows_set_errno(GetLastError());
            return NULL;
        }
        if (len >= (DWORD)size) {
            errno = ERANGE;
            return NULL;
        }
        return buffer;
    }
#elif HAS_POSIX_WIN_SUBSET
    return getcwd(buffer, (size_t)size);
#else
    (void)buffer;
    errno = ENOSYS;
    return NULL;
#endif
}

bool
xregular_file_exists(char *path) {
#if HAS_POSIX_WIN_SUBSET || CBASE_CRT_MSVC
    struct stat st;

    if (stat(path, &st) < 0) {
        if (errno == ENOENT) {
            return false;
        }
        error("stat(%s) failed: %s", path, strerror(errno));
        fatal(EXIT_FAILURE);
    }
    if (!S_ISREG(st.st_mode)) {
        error("expected regular file: %s", path);
        fatal(EXIT_FAILURE);
    }
    return true;
#else
    (void)path;
    errno = ENOSYS;
    error("regular-file checks are unsupported on this platform");
    fatal(EXIT_FAILURE);
#endif
}

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

int32
util_filename_from(char *buffer, int64 size, int fd) {
    if ((buffer == NULL) || (size <= 0)) {
        return -EINVAL;
    }

#if OS_LINUX
    {
        char linkpath[64];
        ssize_t len;

        SNPRINTF(linkpath, "/proc/self/fd/%d", fd);
        if ((len = readlink(linkpath, buffer, (size_t)(size - 1))) < 0) {
            return -errno;
        }
        if (len > MAXOF((int32)0)) {
            return -ENAMETOOLONG;
        }
        buffer[len] = '\0';
        return (int32)len;
    }
#elif CBASE_HAS_F_GETPATH
    {
        static char buffer2[MAXPATHLEN];
        int64 len;

        if (fcntl(fd, F_GETPATH, buffer2) < 0) {
            return -errno;
        }
        len = MIN(strlen32(buffer2), size - 1);
        memcpy64(buffer, buffer2, len + 1);
        buffer[len] = '\0';
        return (int32)len;
    }
#elif OS_WINDOWS
    {
        HANDLE h;
        DWORD len;
        intptr h2 = _get_osfhandle(fd);

        if ((h = (HANDLE)h2) == INVALID_HANDLE_VALUE) {
            return -EINVAL;
        }
        if (size > MAXOF((DWORD)0)) {
            return -EINVAL;
        }

        len = GetFinalPathNameByHandleA(h, buffer, (DWORD)size,
                                        FILE_NAME_NORMALIZED);

        if (len <= 0) {
            windows_set_errno(GetLastError());
            return -errno;
        }
        if (len >= size) {
            return -ENAMETOOLONG;
        }

        if (strncmp32(buffer, "\\\\?\\", 4) == 0) {
            memmove64(buffer, buffer + 4, len - 3);
            len -= 4;
        }

        return (int32)len;
    }
#else
    (void)fd;
    return -ENOSYS;
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
#endif

    if (*fd < 0) {
        return 0;
    }

#if DEBUGGING
    if (filename == NULL) {
        if (util_filename_from(buffer, sizeof(buffer), *fd) >= 0) {
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

    if (close(*fd) < 0) {
        char error_buffer[4096];
        char itoa_buffer[32];
        ITOA(itoa_buffer, line);

#if CC_GCC || CC_CLANG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif
        strerror_r(errno, error_buffer, sizeof(error_buffer));
#if CC_GCC || CC_CLANG
#pragma GCC diagnostic pop
#endif

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
    if (cbase_unlink(filename) < 0) {
        error2("Error removing file %s: %s.\n", filename, strerror(errno));
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
    int err;
    if (fclose(f)) {
        err = errno;
        if (err == 0) {
            err = EIO;
        }
        error_impl(file, line, func,
                   "Error closing %s: %s.\n", filename, strerror(err));
        return -err;
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
#elif OS_WINDOWS
int32
util_copy_file_sync(char *destination, char *source) {
    DWORD error_code;

    if (CopyFileA(source, destination, false)) {
        return 0;
    }

    error_code = GetLastError();
    windows_set_errno(error_code);
    error("Error copying %s to %s: windows error %lu.\n",
          source, destination, (ulong)error_code);
    return -1;
}
#endif

#if OS_UNIX
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

int32
read_entire_file(char *path, char **file_bytes) {
    FILE *file;
    int64 len;
    int64 read_len;
    char *bytes;
    int32 err;

    if (file_bytes) {
        *file_bytes = NULL;
    }
    if (path_missing(path) || (file_bytes == NULL)) {
        error("Error reading file: invalid arguments.\n");
        return -EINVAL;
    }

    if ((file = fopen(path, "rb")) == NULL) {
        err = errno;
        if (err == 0) {
            err = EIO;
        }
        error("Error opening "RED("%s")" for reading: %s",
              path, strerror(err));
        return -err;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        err = errno;
        if (err == 0) {
            err = EIO;
        }
        error("Error seeking end of %s: %s.\n", path, strerror(err));
        XFCLOSE(file, path);
        return -err;
    }
    if ((len = ftell(file)) < 0) {
        err = errno;
        if (err == 0) {
            err = EIO;
        }
        error("Error in ftell(%s): %s.\n", path, strerror(err));
        XFCLOSE(file, path);
        return -err;
    }
    if (len > MAXOF((int32)0)) {
        error("Only files up to 2GB are supported.\n");
        XFCLOSE(file, path);
        return -EFBIG;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        err = errno;
        if (err == 0) {
            err = EIO;
        }
        error("Error rewinding %s: %s.\n", path, strerror(err));
        XFCLOSE(file, path);
        return -err;
    }

    bytes = malloc2(len + 1);
    read_len = 0;
    if (len > 0) {
        read_len = fread64(bytes, 1, len, file);
    }
    if (read_len != len) {
        err = errno;
        if (err == 0) {
            err = EIO;
        }
        error("Error reading "RED("%s")": %s.\n", path, strerror(err));
        free2(bytes, (len + 1)*SIZEOF(*bytes));
        XFCLOSE(file, path);
        return -err;
    }
    bytes[read_len] = '\0';
    if ((err = XFCLOSE(file, path)) < 0) {
        free2(bytes, (len + 1)*SIZEOF(*bytes));
        return err;
    }

    *file_bytes = bytes;
    return (int32)read_len;
}

int64
write_entire_file(char *path, char *text, int64 text_len) {
    FILE *file;
    int64 write_len;
    int32 err;

    if (path_missing(path)) {
        error("Error writing file: invalid path.\n");
        return -EINVAL;
    }
    if (text_len < 0) {
        error("Error writing negative length %lld to %s.", text_len, path);
        return -EINVAL;
    }
    if ((text_len > 0) && (text == NULL)) {
        error("Error writing %lld bytes to %s: invalid buffer.",
              text_len, path);
        return -EINVAL;
    }

    if ((file = fopen(path, "wb")) == NULL) {
        err = errno;
        if (err == 0) {
            err = EIO;
        }
        error("Error opening %s for writing: %s", path, strerror(err));
        return -err;
    }

    write_len = 0;
    if (text_len > 0) {
        write_len = fwrite64(text, 1, text_len, file);
    }
    if (write_len != text_len) {
        err = errno;
        if (err == 0) {
            err = EIO;
        }
        error("Error writing %lld bytes to %s: %s.",
              text_len, path, strerror(err));
        XFCLOSE(file, path);
        return -err;
    }

    if ((err = XFCLOSE(file, path)) < 0) {
        return err;
    }
    return write_len;
}

#if 0 == TESTING_fs
static inline void
fs_functions_sink(void) {
    (void)fs_functions_sink;
    (void)basename2;
    (void)catfile;
    (void)cbase_mkdtemp;
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
#endif
#if OS_UNIX || OS_WINDOWS
    (void)util_copy_file_sync;
#endif
    return;
}
#endif

#if TESTING
void
test_join_path(char *buffer, int64 buffer_len, char *dir, char *name) {
    int32 len;

    len = snprintf2(buffer, buffer_len, "%s/%s", dir, name);
    ASSERT_POSITIVE(len);
    ASSERT(len < buffer_len);

    return;
}

void
test_make_temp_dir(char *buffer, int32 capacity, char *name) {
#if OS_UNIX || OS_WINDOWS
    char *tmpdir;
    int32 len;
    int32 prefix_len = 0;

    tmpdir = getenv("TMPDIR");
#if OS_WINDOWS
    if (tmpdir == NULL) {
        DWORD temp_len;

        temp_len = GetTempPathA((DWORD)capacity, buffer);
        if ((temp_len <= 0) || (temp_len >= (DWORD)capacity)) {
            error("Temporary directory path too long.\n");
            fatal(EXIT_FAILURE);
        }
        prefix_len = (int32)temp_len;
        len = snprintf2(buffer + prefix_len, capacity - prefix_len,
                        "%s_XXXXXX", name);
    } else {
        len = snprintf2(buffer, capacity, "%s/%s_XXXXXX", tmpdir, name);
    }
#else
    if (tmpdir == NULL) {
        tmpdir = "/tmp";
    }
    len = snprintf2(buffer, capacity, "%s/%s_XXXXXX", tmpdir, name);
#endif
    if ((len <= 0) || (len >= (capacity - prefix_len))) {
        error("Temporary directory path too long.\n");
        fatal(EXIT_FAILURE);
    }
    if (cbase_mkdtemp(buffer) == NULL) {
        error("Error creating temporary directory %s: %s.\n",
              buffer, strerror(errno));
        fatal(EXIT_FAILURE);
    }

    return;
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
        if (cbase_remove_empty_dir(path) < 0) {
            error("Error removing test directory %s: %s.\n",
                  path, strerror(errno));
        }
    } else if (cbase_remove_file(path) < 0) {
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

    cbase_remove_file(link_path);
    supported = symlink("target", link_path) == 0;
    if (supported) {
        cbase_remove_file(link_path);
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

    cbase_remove_file(src_path);
    cbase_remove_file(link_path);

    if ((fd = open(src_path, O_CREAT | O_WRONLY | O_TRUNC, 0644)) < 0) {
        return false;
    }
    write64(fd, "x", 1);
    XCLOSE(&fd, src_path);

    supported = link(src_path, link_path) == 0;
    cbase_remove_file(link_path);
    cbase_remove_file(src_path);

    return supported;
}
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
        char dir_path[PATH_MAX];
        char file_path[PATH_MAX];
        char template_path[PATH_MAX];
        char temp_file_path[PATH_MAX];
        int32 fd;

        SNPRINTF(dir_path, "%s/wrapped_dir", temp_dir);
        ASSERT_ZERO(cbase_mkdir(dir_path));
        ASSERT_ZERO(cbase_remove_empty_dir(dir_path));

        SNPRINTF(file_path, "%s/wrapped_file", temp_dir);
        WRITE_FILE(file_path, "x");
        ASSERT(util_file_exists(file_path));
        ASSERT_ZERO(cbase_unlink(file_path));
        ASSERT(!util_file_exists(file_path));

        WRITE_FILE(file_path, "x");
        ASSERT(util_file_exists(file_path));
        ASSERT_ZERO(cbase_remove_file(file_path));
        ASSERT(!util_file_exists(file_path));

        SNPRINTF(template_path, "%s/stem_XXXXXX.txt", temp_dir);
        fd = cbase_mkstemps(template_path, STRLIT_LEN(".txt"));
        ASSERT_NON_NEGATIVE(fd);
        ASSERT(write64(fd, "x", 1) == 1);
        XCLOSE(&fd, template_path);
        ASSERT(util_file_exists(template_path));
        ASSERT_ZERO(cbase_remove_file(template_path));

        fd = cbase_make_temp_file(temp_file_path,
                                  SIZEOF(temp_file_path),
                                  "fs_file",
                                  ".tmp");
        ASSERT_NON_NEGATIVE(fd);
        ASSERT(write64(fd, "y", 1) == 1);
        XCLOSE(&fd, temp_file_path);
        ASSERT(util_file_exists(temp_file_path));
        ASSERT_ZERO(cbase_remove_file(temp_file_path));
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

        ASSERT(write_entire_file(path, STRLIT("abcdef")) == 6);
        ASSERT(util_file_exists(path));
        ASSERT((contents_len = read_entire_file(path, &contents)) >= 0);
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

        ASSERT_POSITIVE(util_filename_from(buffer2, sizeof(buffer2), fd));
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

        ASSERT_POSITIVE(util_filename_from(buffer4, sizeof(buffer4), fd));
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
