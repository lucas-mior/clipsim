/* This file is part of clipsim.
 * Copyright (C) 2024 Lucas Mior

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(CLIPSIM_H)
#define CLIPSIM_H

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <magic.h>

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define RESET "\x1b[0m"

#if !defined(DEBUGGING)
#define DEBUGGING 0
#endif

#if DEBUGGING
#define DEBUG_PRINT(...) \
do { \
    dprintf(STDERR_FILENO, \
            "%s:%d -> "RED"%s("RESET"", __FILE__, __LINE__, __func__); \
    dprintf(STDERR_FILENO, __VA_ARGS__); \
    dprintf(STDERR_FILENO, RED")"RESET"\n"); \
} while (0);
#else
#define DEBUG_PRINT(...)
#endif

#define LENGTH(x) (isize)((sizeof(x) / sizeof(*x)))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define IS_SPACE(x) ((x == ' ') || (x == '\t') || (x == '\n') || (x == '\r'))

#define PAUSE10MS (1000*1000*10)
#define HISTORY_BUFFER_SIZE 128
#define HISTORY_INVALID_ID (HISTORY_BUFFER_SIZE+1)
#define HISTORY_KEEP_SIZE (HISTORY_BUFFER_SIZE/2)
#define ENTRY_MAX_LENGTH BUFSIZ
#define PRINT_DIGITS 3
#define TRIMMED_SIZE 255

#if !defined(INTEGERS)
#define INTEGERS
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long ulonglong;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef size_t usize;
typedef ssize_t isize;
#endif

typedef struct Entry {
    char *content;
    int content_length;
    int16 trimmed_length;
    int16 trimmed;
} Entry;

typedef struct File {
    FILE *file;
    char *name;
    int fd;
    int unused;
} File;

enum {
    CLIPBOARD_TEXT = 0,
    CLIPBOARD_IMAGE,
    CLIPBOARD_LARGE,
    CLIPBOARD_OTHER,
    CLIPBOARD_ERROR,
};

enum {
    COMMAND_PRINT = 0,
    COMMAND_INFO,
    COMMAND_COPY,
    COMMAND_REMOVE,
    COMMAND_SAVE,
    COMMAND_DAEMON,
    COMMAND_HELP,
};

extern Entry entries[];
extern bool is_image[];
extern pthread_mutex_t lock;
extern const char TEXT_TAG;
extern const char IMAGE_TAG;
extern magic_t magic;

static int32 util_open(File *file, const int32 flag);
static void util_close(File *file);

int32
util_open(File *file, const int32 flag) {
    if ((file->fd = open(file->name, flag)) < 0) {
        fprintf(stderr, "Error opening %s: %s\n", file->name, strerror(errno));
        return -1;
    } else {
        return 0;
    }
}

void
util_close(File *file) {
    if (file->fd >= 0) {
        if (close(file->fd) < 0)
            fprintf(stderr, "Error closing %s: %s\n", file->name, strerror(errno));
        file->fd = -1;
    }
    if (file->file != NULL) {
        if (fclose(file->file) != 0)
            fprintf(stderr, "Error closing %s: %s\n", file->name, strerror(errno));
        file->file = NULL;
    }
    return;
}
#endif /* CLIPSIM_H */
