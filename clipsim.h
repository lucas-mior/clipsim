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
#include <magic.h>

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

#define SNPRINTF(BUFFER, FORMAT, ...) \
    snprintf2(BUFFER, sizeof(BUFFER), FORMAT, __VA_ARGS__)

#define ARRAY_STRING(BUFFER, SEP, ARRAY, LENGTH) \
  _Generic((ARRAY), \
    int *: array_string(BUFFER, sizeof(BUFFER), SEP, "%d", ARRAY, LENGTH), \
    float *: array_string(BUFFER, sizeof(BUFFER), SEP, "%f", ARRAY, LENGTH), \
    double *: array_string(BUFFER, sizeof(BUFFER), SEP, "%f", ARRAY, LENGTH), \
    char **: array_string(BUFFER, sizeof(BUFFER), SEP, "%s", ARRAY, LENGTH) \
  )

#define LENGTH(x) (isize) ((sizeof(x) / sizeof(*x)))
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
extern mtx_t lock;
extern const char TEXT_TAG;
extern const char IMAGE_TAG;
extern char *program;
extern magic_t magic;


void content_remove_newline(char *, int *);
void content_trim_spaces(int16 *, int16 *, char *, int16);
int32 content_check_content(uchar *, int);

int32 history_length_get(void);
void history_read(void);
void history_append(char *, int);
bool history_save(void);
void history_recover(int32);
void history_remove(int32);
void history_backup(void);
void history_delete_tmp(void);
void history_exit(int) __attribute__((noreturn));

int clipboard_daemon_watch(void) __attribute__((noreturn));

int ipc_daemon_listen_fifo(void *) __attribute__((noreturn));
void ipc_client_speak_fifo(int32, int32);

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
int snprintf2(char *, size_t, char *, ...);
int util_command(const int, char **);
void array_string(char *, int32, char *, char *, char **, int32);
void error(char *, ...);

int32 xi_daemon_loop(void *);

#endif /* CLIPSIM_H */
