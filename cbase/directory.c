// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(DIRECTORY_C)
#define DIRECTORY_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_directory 1
#elif !defined(TESTING_directory)
#define TESTING_directory 0
#endif

#include "cbase.h"

static void
get_directory_entries_free(DirEntry *list, int64 capacity) {
    free2(list, capacity*SIZEOF(*list));
    return;
}

#if !OS_WINDOWS
int32
get_directory_entries(char *directory, DirEntry **directory_list) {
    DIR *dir;
    struct dirent *entry;
    DirEntry *entries;
    int32 length = 0;
    int32 capacity = 256;
    int32 error_code;

    if ((dir = opendir(directory)) == NULL) {
        return -1;
    }

    entries = malloc2(capacity*SIZEOF(*entries));

    errno = 0;
    while ((entry = readdir(dir)) != NULL) {
        int32 name_len = strlen32(entry->d_name);

        if (name_len >= SIZEOF(entries[0].name)) {
            error("File name too long. Skipping...\n");
            continue;
        }

        if (length >= capacity) {
            int64 old_capacity = capacity;

            if (capacity > (MAXOF(capacity) / 2)) {
                error("Error: too many files in directory '%s'.\n", directory);
                fatal(EXIT_FAILURE);
            }
            capacity *= 2;
            entries = realloc2(entries, old_capacity, capacity,
                               SIZEOF(*entries));
        }

        entries[length].name_len = name_len;
        memcpy64(entries[length].name, entry->d_name, name_len + 1);
        length += 1;
    }

    error_code = errno;
    if (closedir(dir) < 0) {
        if (error_code == 0) {
            error_code = errno;
        }
    }

    if (error_code != 0) {
        get_directory_entries_free(entries, capacity);
        errno = error_code;
        return -1;
    }

    entries = realloc2(entries, capacity, length, SIZEOF(*entries));
    *directory_list = entries;

    return length;
}
#else
int32
get_directory_entries(char *directory, DirEntry **directory_list) {
    WIN32_FIND_DATAW find_data;
    HANDLE find_handle;
    wchar_t *wide_pattern;
    DirEntry *entries;
    DWORD error_code;
    int64 pattern_capacity;
    int64 pattern_length;
    int32 length;
    int32 capacity = 16;
    int32 wide_dir_length;

    if ((directory == NULL) || (directory_list == NULL)) {
        errno = EINVAL;
        return -1;
    }

    wide_dir_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          directory, -1, NULL, 0);
    if (wide_dir_length <= 0) {
        windows_set_errno(GetLastError());
        return -1;
    }

    pattern_capacity = (int64)wide_dir_length + 2;
    wide_pattern = malloc2(pattern_capacity*SIZEOF(*wide_pattern));
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, directory, -1,
                            wide_pattern, wide_dir_length)
        != wide_dir_length) {
        error_code = GetLastError();
        free2(wide_pattern, pattern_capacity*SIZEOF(*wide_pattern));
        windows_set_errno(error_code);
        return -1;
    }

    pattern_length = (int64)wide_dir_length - 1;
    if ((pattern_length > 0)
        && (wide_pattern[pattern_length - 1] != L'/')
        && (wide_pattern[pattern_length - 1] != L'\\')) {
        wide_pattern[pattern_length] = L'\\';
        pattern_length += 1;
    }
    wide_pattern[pattern_length] = L'*';
    pattern_length += 1;
    wide_pattern[pattern_length] = L'\0';

    find_handle = FindFirstFileW(wide_pattern, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        error_code = GetLastError();
        free2(wide_pattern, pattern_capacity*SIZEOF(*wide_pattern));
        windows_set_errno(error_code);
        return -1;
    }
    free2(wide_pattern, pattern_capacity*SIZEOF(*wide_pattern));

    length = 0;
    entries = malloc2(capacity*SIZEOF(*entries));
    while (true) {
        int32 name_len;
        int32 utf8_length;

        utf8_length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                          find_data.cFileName, -1,
                                          NULL, 0, NULL, NULL);
        if (utf8_length <= 0) {
            error_code = GetLastError();
            FindClose(find_handle);
            get_directory_entries_free(entries, capacity);
            windows_set_errno(error_code);
            return -1;
        }

        name_len = utf8_length - 1;
        if (name_len >= SIZEOF(entries[0].name)) {
            error("File name too long. Skipping...\n");
        } else {
            if (length >= capacity) {
                int64 old_capacity = capacity;

                if (capacity > (MAXOF(capacity) / 2)) {
                    error("Error: too many files in directory '%s'.\n",
                          directory);
                    fatal(EXIT_FAILURE);
                }
                capacity *= 2;
                entries = realloc2(entries, old_capacity, capacity,
                                   SIZEOF(*entries));
            }

            entries[length].name_len = name_len;
            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                    find_data.cFileName, -1,
                                    entries[length].name,
                                    utf8_length, NULL, NULL)
                != utf8_length) {
                error_code = GetLastError();
                FindClose(find_handle);
                get_directory_entries_free(entries, capacity);
                windows_set_errno(error_code);
                return -1;
            }
            length += 1;
        }

        if (!FindNextFileW(find_handle, &find_data)) {
            error_code = GetLastError();
            break;
        }
    }

    if (!FindClose(find_handle) && (error_code == ERROR_NO_MORE_FILES)) {
        error_code = GetLastError();
    }
    if (error_code != ERROR_NO_MORE_FILES) {
        get_directory_entries_free(entries, capacity);
        windows_set_errno(error_code);
        return -1;
    }

    entries = realloc2(entries, capacity, length, SIZEOF(*entries));
    *directory_list = entries;
    return length;
}
#endif

#if TESTING_directory
#define CBASE_IMPLEMENT
#include "cbase.h"

static int32
directory_entry_index(DirEntry *entries, int32 length, char *name) {
    for (int32 i = 0; i < length; i += 1) {
        if (strequal(entries[i].name, name)) {
            return i;
        }
    }

    return -1;
}

static void
test_directory_entries_are_valid(DirEntry *entries, int32 length) {
    for (int32 i = 0; i < length; i += 1) {
        ASSERT_NON_NEGATIVE(entries[i].name_len);
        ASSERT_LESS(entries[i].name_len, SIZEOF(entries[i].name));
        ASSERT_EQUAL(entries[i].name_len, strlen32(entries[i].name));
        ASSERT_EQUAL(entries[i].name[entries[i].name_len], '\0');
    }

    return;
}

static void
test_get_directory_entries_reads_directory(void) {
    DirEntry *entries = NULL;
    int32 length = get_directory_entries("cbase", &entries);

    ASSERT_POSITIVE(length);
    test_directory_entries_are_valid(entries, length);
    ASSERT_NON_NEGATIVE(directory_entry_index(entries, length, "cbase.h"));
    ASSERT_NON_NEGATIVE(directory_entry_index(entries, length, "directory.c"));

    free2(entries, (int64)length*SIZEOF(*entries));
    return;
}

static void
test_get_directory_entries_reports_missing_directory(void) {
    DirEntry *entries = NULL;

    ASSERT_EQUAL(get_directory_entries("cbase/this_directory_must_not_exist",
                                       &entries), -1);
    return;
}

int
main(void) {
    test_get_directory_entries_reads_directory();
    test_get_directory_entries_reports_missing_directory();
    exit(EXIT_SUCCESS);
}
#endif /* TESTING_directory */

#endif
