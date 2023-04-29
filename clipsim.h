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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifndef CLIPSIM_H
#define CLIPSIM_H

#define ARRLEN(x) (sizeof(x) / sizeof(x[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define IS_SPACE(x) ((x == ' ') || (x == '\t') || (x == '\n'))

#ifdef CLIPSIM_DEBUG
#define DEBUG_PRINT(s, ...) printf(s, __VA_ARGS__);
#else
#define DEBUG_PRINT(s, ...)
#endif

#define PAUSE10MS (1000 * 1000 * 10)
#define HISTORY_BUFFER_SIZE 512U
#define HISTORY_KEEP_SIZE (HISTORY_BUFFER_SIZE/2)
#define ENTRY_MAX_LENGTH (BUFSIZ/2)
#define PRINT_DIGITS 3
#define TRIMMED_SIZE 255

enum {
    PRINT = 0,
    INFO,
    COPY,
    EXCLUDE,
    SAVE,
    DAEMON,
    HELP,
};

typedef struct Command {
    const char *shortname;
    const char *longname;
    const char *description;
} Command;

static const Command commands[] = {
    [PRINT]  =  {"-p", "--print", "print history" },
    [INFO]   =  {"-i", "--info", "print entry number <n>" },
    [COPY]   =  {"-c", "--copy", "copy entry number <n>" },
    [EXCLUDE] = {"-x", "--exclude", "exclude entry number <n>" },
    [SAVE]   =  {"-s", "--save", "save history to $XDG_CACHE_HOME/clipsim/history" },
    [DAEMON] =  {"-d", "--daemon", "spawn daemon" },
    [HELP]   =  {"-h", "--help", "print help message" },
};

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

typedef struct Entry {
    size_t content_length;
    size_t trimmed_length;
    char *content;
    char *trimmed;
    char *image_path;
} Entry;

typedef enum GetClipboardResult {
    TEXT,
    LARGE,
    IMAGE,
    OTHER,
    ERROR,
} GetClipboardResult;

#pragma clang diagnostic ignored "-Wpadded"
typedef struct File {
    FILE *file;
    char *name;
    int fd;
} File;

extern Entry entries[HISTORY_BUFFER_SIZE];
extern pthread_mutex_t lock;
static const char IMAGE_END = 0x02;
static const char TEXT_END = 0x01;

void *clipboard_daemon_watch(void *);
void content_remove_newline(char *, ulong *);
void content_trim_spaces(Entry *);
GetClipboardResult content_valid_content(uchar *, ulong);
int32 history_lastindex(void);
void history_read(void);
void history_append(char *, ulong);
bool history_save(void);
void history_recover(int32);
void history_exclude(int32);
void *ipc_daemon_listen_fifo(void *);
void ipc_client_speak_fifo(int, int32);
void send_signal(char *, int);
void *util_realloc(void *, size_t);
void *util_calloc(size_t, size_t);
bool util_strtol(int32 *, char *, int);
void util_segv_handler(int) __attribute__((noreturn));
void util_int_handler(int) __attribute__((noreturn));
void util_close(File *);
bool util_open(File *, int);
bool util_copy_file(const char *, const char *);

#endif /* CLIPSIM_H */
