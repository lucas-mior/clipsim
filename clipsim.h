// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(CLIPSIM_H)
#define CLIPSIM_H

#include "cbase.h"
#include <magic.h>

#if !defined(DEBUGGING)
#define DEBUGGING 0
#endif

#if DEBUGGING
#define DEBUG_PRINT(...) \
do { \
    char debug_buffer[4096]; \
    int32 debug_len = snprintf2(debug_buffer, sizeof(debug_buffer), __VA_ARGS__); \
    if (debug_len > 0) { \
        int32 debug_limit = 0; \
        if (debug_len < (int32)sizeof(debug_buffer)) { \
            debug_limit = debug_len; \
        } else { \
            debug_limit = (int32)sizeof(debug_buffer) - 1; \
        } \
        for (int32 _i = 0; _i < debug_limit; _i += 1) { \
            if (debug_buffer[_i] == '\n') { \
                debug_buffer[_i] = ' '; \
            } \
        } \
        dprintf(STDERR_FILENO, \
                "%s:%d -> %s(%s)\n", __FILE__, __LINE__, __func__, debug_buffer); \
    } \
} while (0);
#else
#define DEBUG_PRINT(...)
#endif

#define IS_SPACE(x) ((x == ' ') || (x == '\t') || (x == '\n') || (x == '\r'))

#define PAUSE10MS (1000*1000*10)
#define HISTORY_BUFFER_SIZE 128
#define HISTORY_INVALID_ID (HISTORY_BUFFER_SIZE+1)
#define HISTORY_KEEP_SIZE (HISTORY_BUFFER_SIZE/2)
#define ENTRY_MAX_LENGTH SIZEMB(1)
#define MAX_MAGIC_BUFFER_LEN SIZEKB(16)
#define PRINT_DIGITS 3
#define TRIMMED_SIZE 255

typedef struct Entry {
    char *content;
    int32 content_length;
    int32 trimmed;
    int32 trimmed_length;
    int32 padding;
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

static Entry entries[HISTORY_BUFFER_SIZE] = {0};
static bool is_image[HISTORY_BUFFER_SIZE] = {0};
static char TEXT_TAG = (char)0x01;
static char IMAGE_TAG = (char)0x02;
static pthread_mutex_t lock;
static magic_t magic = 0;

static void util_close(File *file);
static void main_reopen_magic(void);

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

static void
reopen_magic(void) {
    if (magic) {
        magic_close(magic);
    }
    if ((magic = magic_open(MAGIC_MIME_TYPE)) == NULL) {
        error("Error in magic_open(MAGIC_MIME_TYPE): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (magic_load(magic, NULL) != 0) {
        error("Error in magic_load(): %s\n", magic_error(magic));
        exit(EXIT_FAILURE);
    }
    return;
}

#endif /* CLIPSIM_H */
