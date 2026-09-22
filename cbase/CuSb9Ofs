// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(FS_WINDOWS_C)
#define FS_WINDOWS_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_fs_windows 1
#elif !defined(TESTING_fs_windows)
#define TESTING_fs_windows 0
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

char *
cbase_mkdtemp(char *template) {
    char characters[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int64 len;

    if (template == NULL) {
        errno = EINVAL;
        return NULL;
    }

    len = strlen32(template);
    if ((len < 6) || (memcmp64(template + len - 6, "XXXXXX", 6) != 0)) {
        errno = EINVAL;
        return NULL;
    }

    for (uint32 attempt = 0; attempt < 10000; attempt += 1) {
        DWORD error_code;
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

        if (CreateDirectoryA(template, NULL)) {
            return template;
        }

        error_code = GetLastError();
        if (error_code != ERROR_ALREADY_EXISTS) {
            windows_set_errno(error_code);
            return NULL;
        }
    }

    errno = EEXIST;
    return NULL;
}

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

#if 0 == TESTING_fs_windows
static inline void
fs_windows_functions_sink(void) {
    (void)fs_windows_functions_sink;
    (void)lstat;
}
#endif

#if TESTING_fs_windows
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
    (void)lstat;
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
#endif /* TESTING_fs_windows */

#endif /* FS_WINDOWS_C */
