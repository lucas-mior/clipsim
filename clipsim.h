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
#include <pthread.h>

#ifndef CLIPSIM_H
#define CLIPSIM_H

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
    DELETE,
    SAVE,
    DAEMON,
    HELP,
};

typedef struct Command {
    const char *name;
    const char *description;
} Command;

static const Command commands[] = {
    [PRINT]  = {"print", "print history" },
    [INFO]   = {"info", "print entry number <n>" },
    [COPY]   = {"copy", "copy entry number <n>" },
    [DELETE] = {"delete", "delete entry number <n>" },
    [SAVE]   = {"save", "save history to $XDG_CACHE_HOME/clipsim/history" },
    [DAEMON] = {"daemon", "spawn daemon" },
    [HELP]   = {"help", "print help message" },
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
} Entry;

#pragma clang diagnostic ignored "-Wpadded"
typedef struct File {
    FILE *file;
    char *name;
    int fd;
} File;

extern Entry entries[HISTORY_BUFFER_SIZE];
extern pthread_mutex_t lock;

#endif /* CLIPSIM_H */
