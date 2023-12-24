/* This file is part of clipsim.
 * Copyright (C) 2023 Lucas Mior

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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
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
#include <threads.h>
#include <time.h>
#include <unistd.h>

#ifndef CLIPSIM_H
#define CLIPSIM_H

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define RESET "\x1b[0m"

#ifdef CLIPSIM_DEBUG
#define DEPRINTF(...) dprintf(STDERR_FILENO, __VA_ARGS__);
#define DEBUG_PRINT(...) \
do { \
    DEPRINTF("%s:%d -> "RED"%s("RESET"", __FILE__, __LINE__, __func__); \
    DEPRINTF(__VA_ARGS__); \
    DEPRINTF(RED")"RESET"\n"); \
} while (0)
#else
#define DEBUG_PRINT(...)
#endif

#define LENGTH(x) (isize) ((sizeof (x) / sizeof (*x)))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define IS_SPACE(x) ((x == ' ') || (x == '\t') || (x == '\n'))

#define PAUSE10MS (1000 * 1000 * 10)
#define HISTORY_BUFFER_SIZE 128
#define HISTORY_KEEP_SIZE (HISTORY_BUFFER_SIZE/2)
#define ENTRY_MAX_LENGTH BUFSIZ
#define PRINT_DIGITS 3
#define TRIMMED_SIZE 255

#ifndef INTEGERS
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
    int content_length;
    int trimmed_length;
    char *content;
    char *trimmed;
    char *image_path;
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
extern mtx_t lock;
extern const char TEXT_TAG;
extern const char IMAGE_TAG;
extern char *program;

void content_remove_newline(char *, int *);
void content_trim_spaces(char **, int *, char *, int);
int32 content_check_content(uchar *, int);

int32 history_lastindex(void);
void history_read(void);
void history_append(char *, int);
bool history_save(void);
void history_recover(int32);
void history_remove(int32);

int clipboard_daemon_watch(void) __attribute__((noreturn));

int ipc_daemon_listen_fifo(void *) __attribute__((noreturn));
void ipc_client_speak_fifo(uint, int32);

void send_signal(const char *, const int);

void *util_malloc(const usize);
void *util_memdup(const void *, const usize);
char *util_strdup(const char *);
void *util_realloc(void *, const usize);
void *util_calloc(const usize, const usize);
int util_string_int32(int32 *, const char *);
void util_segv_handler(int) __attribute__((noreturn));
void util_close(File *);
int util_open(File *, const int);
int util_copy_file(const char *, const char *);
void util_die_notify(const char *, ...) __attribute__((noreturn));
void error(char *, ...);

#endif /* CLIPSIM_H */
