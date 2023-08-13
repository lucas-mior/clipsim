/* This file is part of clipsim. */
/* Copyright (C) 2022 Lucas Mior */

/* This program is free software: you can redistribute it and/or modify */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation, either version 3 of the License, or */
/* (at your option) any later version. */

/* This program is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/* GNU General Public License for more details. */

/* You should have received a copy of the GNU General Public License */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>.*/

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <threads.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>

#ifndef CLIPSIM_H
#define CLIPSIM_H

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define IS_SPACE(x) ((x == ' ') || (x == '\t') || (x == '\n'))

#define PAUSE10MS (1000 * 1000 * 10)
#define HISTORY_BUFFER_SIZE 512U
#define HISTORY_KEEP_SIZE (HISTORY_BUFFER_SIZE/2)
#define ENTRY_MAX_LENGTH BUFSIZ
#define PRINT_DIGITS 3
#define TRIMMED_SIZE 255

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

#pragma clang diagnostic ignored "-Wpadded"
typedef struct Entry {
    size_t content_length;
    size_t trimmed_length;
    char *content;
    char *trimmed;
    char *image_path;
} Entry;

typedef struct File {
    FILE *file;
    char *name;
    int fd;
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

static const char TEXT_END = (char) 0x01;
static const char IMAGE_END = (char) 0x02;

typedef struct Command {
    const char *shortname;
    const char *longname;
    const char *description;
} Command;

static const Command commands[] = {
    [COMMAND_PRINT]  = {"-p", "--print",  "print entire history, with trimmed whitespace" },
    [COMMAND_INFO]   = {"-i", "--info",   "print entry number <n>, with original whitespace" },
    [COMMAND_COPY]   = {"-c", "--copy",   "copy entry number <n>, with original whitespace" },
    [COMMAND_REMOVE] = {"-r", "--remove", "remove entry number <n>" },
    [COMMAND_SAVE]   = {"-s", "--save",   "save history to $XDG_CACHE_HOME/clipsim/history" },
    [COMMAND_DAEMON] = {"-d", "--daemon", "spawn daemon (clipboard watcher and command listener)" },
    [COMMAND_HELP]   = {"-h", "--help",   "print this help message" },
};

extern Entry entries[HISTORY_BUFFER_SIZE];
extern mtx_t lock;

void content_remove_newline(char *, size_t *);
void content_trim_spaces(char **, size_t *, char *, size_t);
int32 content_check_content(uchar *, size_t);

int32 history_lastindex(void);
void history_read(void);
void history_append(char *, ulong);
bool history_save(void);
void history_recover(int32);
void history_remove(int32);

int clipboard_daemon_watch(void *);

int ipc_daemon_listen_fifo(void *);
void ipc_client_speak_fifo(uint, int32);

void send_signal(const char *, const int);

void *util_realloc(void *, const size_t);
void *util_calloc(const size_t, const size_t);
int util_string_int32(int32 *, const char *, const int);
void util_segv_handler(int) __attribute__((noreturn));
void util_close(File *);
int util_open(File *, const int);
int util_copy_file(const char *, const char *);

int nanosleep(const struct timespec *, struct timespec *);

#endif /* CLIPSIM_H */
