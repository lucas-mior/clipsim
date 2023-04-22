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

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/wait.h>

#include "clipsim.h"
#include "history.h"
#include "util.h"
#include "text.h"

static volatile bool recovered = false;
static const char SEPARATOR = 0x01;

static void history_file_find(void);
static int32 history_repeated_index(char *, size_t);
static void history_new_entry(size_t);
static void history_reorder(int32);
static void history_clean(void);
File history = { .file = NULL, .fd = -1, .name = NULL };
static int32 lastindex;

int32 history_lastindex(void) {
    return lastindex;
}

void history_file_find(void) {
    DEBUG_PRINT("history_find(void) %d\n", __LINE__)
    char *cache = NULL;
    const char *clipsim = "clipsim/history";
    size_t length;

    if (!(cache = getenv("XDG_CACHE_HOME"))) {
        fprintf(stderr, "XDG_CACHE_HOME needs to be set. "
                        "History will not be saved.\n");
        history.name = NULL;
        return;
    }
     
    length = strlen(cache);
    length += 1 + strlen(clipsim);
    history.name = xalloc(NULL, length+1);

    (void) snprintf(history.name, length+1, "%s/%s", cache, clipsim);
    return;
}

void history_read(void) {
    DEBUG_PRINT("history_read(void) %d\n", __LINE__)
    size_t i = 0;
    int c;
    size_t to_alloc;
    size_t malloced;
    Entry *e = NULL;

    lastindex = -1;
    history_file_find();
    if (!history.name) {
        fprintf(stderr, "History file name unresolved. "
                        "History will start empty.\n");
        return;
    }
    if (!(history.file = fopen(history.name, "r"))) {
        fprintf(stderr, "Error opening history file for reading: %s\n"
                        "History will start empty.\n", strerror(errno));
        return;
    }

    if ((c = fgetc(history.file)) != SEPARATOR) {
        fprintf(stderr, "History file is corrupted. "
                        "Delete it and restart %s.\n", "clipsim");
        (void) fclose(history.file);
        return;
    }

    history_new_entry(to_alloc = DEF_ALLOC);
    malloced = malloc_usable_size(entries[lastindex].data);
    while ((c = fgetc(history.file)) != EOF) {
        e = &entries[lastindex];
        if (c == SEPARATOR) {
            e->data = xalloc(e->data, i+1);
            e->len = i;
            e->data[i] = '\0';

            history_new_entry(to_alloc = DEF_ALLOC);
            malloced = malloc_usable_size(entries[lastindex].data);
            i = 0;
        } else {
            if (i >= (malloced - 1)) {
                to_alloc *= 2;
                if (to_alloc > MAX_ENTRY_SIZE) {
                    fprintf(stderr, "Too long entry on history file.");
                    exit(EXIT_FAILURE);
                }
                e->data = xalloc(e->data, to_alloc);
                malloced = malloc_usable_size(e->data);
            }
            e->data[i] = (char) c;
            i += 1;
        }
    }

    e->data = xalloc(e->data, i+1);
    e->len = i;
    e->data[i] = '\0';

    closef(&history);
    return;
}

bool history_save(void) {
    DEBUG_PRINT("history_save(void) %d\n", __LINE__)

    if (lastindex < 0) {
        fprintf(stderr, "History is empty. Not saving.\n");
        return false;
    }
    if (!history.name) {
        fprintf(stderr, "History file name unresolved, can't save history.");
        return false;
    }
    if ((history.fd = open(history.name, O_WRONLY | O_CREAT | O_TRUNC,
                                         S_IRUSR | S_IWUSR)) < 0) {
        fprintf(stderr, "Failed to open history file for saving: "
                        "%s\n", strerror(errno));
        return false;
    }

    for (uint i = 0; i <= (uint) lastindex; i += 1) {
        Entry *e = &entries[i];
        write(history.fd, &SEPARATOR, 1);
        write(history.fd, e->data, e->len);
    }

    if (fsync(history.fd) < 0) {
        fprintf(stderr, "Error saving history to disk: %s\n", strerror(errno));
        closef(&history);
        return false;
    } else {
        fprintf(stderr, "History saved to disk.\n");
        closef(&history);
        return true;
    }
}

int32 history_repeated_index(char *data, size_t length) {
    DEBUG_PRINT("history_repeated_index(%.*s, %lu)\n", 20, data, length)
    for (int32 i = lastindex; i >= 0; i -= 1) {
        Entry *e = &entries[i];
        if (e->len == length) {
            if (!strcmp(e->data, data)) {
                return i;
            }
        }
    }
    return -1;
}

void history_append(char *data, ulong len) {
    DEBUG_PRINT("history_append(%.*s, %lu)\n", 20, data, len)
    int32 oldindex;
    Entry *e;

    if (recovered) {
        recovered = false;
        return;
    }

    if (!text_valid_content((uchar *) data, len))
        return;

    data[len] = '\0';

    if (data[len-1] == '\n') {
        data[len-1] = '\0';
        len -= 1;
    }

    if ((oldindex = history_repeated_index(data, len)) >= 0) {
        fprintf(stderr, "Entry is equal to previous entry. Reordering...\n");
        if (oldindex != lastindex)
            history_reorder(oldindex);
        free(data);
        return;
    }

    history_new_entry(0);
    e = &entries[lastindex];
    e->data = data;
    e->data[len] = '\0';
    e->len = len;

    if (lastindex+1 >= (int32) HISTORY_BUFFER_SIZE) {
        history_clean();
        history_save();
    }

    return;
}

void history_recover(int32 id) {
    DEBUG_PRINT("history_recover(%d)", id)
    pid_t child = -1;
    int fd[2];
    Entry *e;

    if (pipe(fd)){
        perror("pipe failed");
        return;
    }

    switch ((child = fork())) {
        case 0:
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            execlp("/usr/bin/xsel", "xsel", "-b", NULL);
            fprintf(stderr, "Failed to exec(): %s", strerror(errno));
            return;
        case -1:
            fprintf(stderr, "Failed to fork(): %s", strerror(errno));
            return;
        default:
            close(fd[0]);
    }

    if (id < 0)
        id = lastindex + id + 1;

    if (lastindex < 0) {
        fprintf(stderr, "Clipboard history empty. Start copying text.\n");
        dprintf(fd[1], "Clipboard history empty. Start copying text.\n");
        close(fd[1]);
        wait(NULL);
        return;
    }

    if (id > lastindex) {
        fprintf(stderr, "Invalid index: %d\n", id);
        close(fd[1]);
        kill(child, SIGKILL);
        recovered = true;
        return;
    }

    e = &entries[id];
    dprintf(fd[1], "%s", e->data);
    close(fd[1]);
    wait(NULL);

    if (id != lastindex)
        history_reorder(id);

    recovered = true;
    return;
}

void history_delete(int32 id) {
    DEBUG_PRINT("history_delete(%d)\n", id)
    Entry *e;
    if (lastindex == 0) {
        return;
    }

    if (id < 0) {
        id = lastindex + id + 1;
    } else if (id == lastindex) {
        history_recover(-2);
        history_delete(-2);
        return;
    }
    if (id > lastindex) {
        fprintf(stderr, "Invalid index %d for deletion.\n", id);
        return;
    }

    e = &entries[id];
    free(e->data);
    if (e->out != e->data)
        free(e->out);

    if (id < lastindex) {
        memmove(&entries[id], &entries[id+1], 
                (size_t) (lastindex - id)*sizeof(Entry));
        memset(&entries[lastindex], 0, sizeof(Entry));
    }
    lastindex -= 1;

    return;
}

void history_reorder(int32 oldindex) {
    DEBUG_PRINT("history_reorder(%d) %d\n", oldindex)
    Entry aux = entries[oldindex];
    memmove(&entries[oldindex], &entries[oldindex+1], 
            (size_t) (lastindex - oldindex)*sizeof(Entry));
    memmove(&entries[lastindex], &aux, sizeof(Entry));
    return;
}

void history_clean(void) {
    DEBUG_PRINT("history_clean(void) %d\n", __LINE__)
    for (uint i = 0; i <= HISTORY_KEEP_SIZE-1; i += 1) {
        Entry *e = &entries[i];
        free(e->data);
        if (e->out != e->data)
            free(e->out);

    }
    memmove(&entries[0], &entries[HISTORY_KEEP_SIZE],
            HISTORY_KEEP_SIZE*sizeof(Entry));
    memset(&entries[HISTORY_KEEP_SIZE], 0, HISTORY_KEEP_SIZE*sizeof(Entry));
    lastindex = HISTORY_KEEP_SIZE-1;
    return;
}

void history_new_entry(size_t size) {
    DEBUG_PRINT("history_new_entry(%zu)\n", size)
    Entry *e;
    lastindex += 1;
    e = &entries[lastindex];

    if (size) {
        e->data = xalloc(NULL, size);
        e->len = size - 1;
    }
    return;
}
