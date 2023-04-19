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
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "clipsim.h"
#include "hist.h"
#include "util.h"
#include "text.h"

char *histfile = NULL;
static volatile bool recovered = false;
static const char sep = 0x01;
static const int32 max_file_len = 1024;

static void hist_file_find(void);
static int32 hist_repeated_index(char *, size_t);
static void hist_new_entry(size_t);
static void hist_reorder(int32);
static void hist_clean(void);

static inline void hist_file_find(void) {
    DEBUG_PRINT("static inline void hist_find(void) %d\n", __LINE__)
    char *cache = NULL;
    const char *clipsim = "/clipsim/history";
    size_t min;

    if (!(cache = getenv("XDG_CACHE_HOME"))) {
        fprintf(stderr, "XDG_CACHE_HOME needs to be set. "
                        "History will not be saved.\n");
        histfile = NULL;
        return;
    } else if ((min = strlen(cache)) >= max_file_len) {
        fprintf(stderr, "Cache name too long. History will not be saved.\n");
        histfile = NULL;
        return;
    }

    min = min + 1 + strlen(clipsim);
    histfile = xalloc(NULL, min+1);

    (void) snprintf(histfile, min+1, "%s/%s", cache, clipsim);
    return;
}

void hist_read(void) {
    DEBUG_PRINT("void hist_read(void) %d\n", __LINE__)
    FILE *history = NULL;
    size_t i = 0;
    int c;
    size_t to_alloc;
    Entry *e = NULL;

    hist_file_find();
    if (!histfile) {
        fprintf(stderr, "History file name unresolved. "
                        "History will start empty.\n");
        return;
    }
    if (!(history = fopen(histfile, "r"))) {
        fprintf(stderr, "Error opening history file for reading: %s\n"
                        "History will start empty.\n", strerror(errno));
        return;
    }

    if ((c = fgetc(history)) != sep) {
        fprintf(stderr, "History file is corrupted. "
                        "Delete it and restart %s.\n", progname);
        (void) fclose(history);
        return;
    }

    hist_new_entry(to_alloc = DEF_ALLOC);
    while ((c = fgetc(history)) != EOF) {
        e = &entries[lastindex];
        if (c == sep) {
            if (i >= (to_alloc - 1)) {
                e->data = xalloc(e->data, to_alloc+1);
            } else if (i < to_alloc) {
                e->data = xalloc(e->data, i+1);
            }
            e->len = i;
            e->data[i] = '\0';

            hist_new_entry(to_alloc = DEF_ALLOC);
            i = 0;
        } else {
            if (i >= (to_alloc - 1)) {
                if (to_alloc < 0xFFFF) {
                    to_alloc *= 2;
                } else {
                    fprintf(stderr, "Too long entry. Skipping.\n");
                    free(e->data);
                    hist_new_entry(to_alloc = DEF_ALLOC);
                }
                e->data = xalloc(e->data, to_alloc);
            }
            e->data[i] = (char) c;
            i += 1;
        }
    }

    e->len = i;
    e->data[i] = '\0';

    (void) fclose(history);
    return;
}

int32 hist_repeated_index(char *save, size_t min) {
    DEBUG_PRINT("int32 hist_equal_to_previous(char *save, size_t min) %d\n", __LINE__)
    for (int32 i = lastindex; i >= 0; i -= 1) {
        Entry *e = &entries[i];
        if (e->len == min) {
            if (!strcmp(e->data, save)) {
                return i;
            }
        }
    }
    return -1;
}

void hist_add(char *save, ulong len) {
    DEBUG_PRINT("void hist_add(char *save, ulong len) %d\n", __LINE__)
    size_t min;
    int32 eindex;
    Entry *e;

    if (recovered) {
        recovered = false;
        return;
    }

    if (!text_valid_content((uchar *) save, len))
        return;

    min = MIN(len, MAX_ENTRY_SIZE);
    save[min] = '\0';

    if (save[min-1] == '\n') {
        save[min-1] = '\0';
        min -= 1;
    }

    if ((eindex = hist_repeated_index(save, min)) >= 0) {
        fprintf(stderr, "Entry is equal to previous entry. Reordering...\n");
        if (eindex != lastindex)
            hist_reorder(eindex);
        free(save);
        return;
    }

    hist_new_entry(0);
    e = &entries[lastindex];
    e->data = save;
    e->data[min] = '\0';
    e->len = min;

    if (lastindex+1 >= (int32) HIST_SIZE) {
        hist_clean();
        hist_save();
    }

    return;
}

bool hist_save(void) {
    DEBUG_PRINT("bool hist_save(void) %d\n", __LINE__)
    int history;

    if (!histfile) {
        fprintf(stderr, "History file name unresolved, can't save history.");
        return false;
    }
    if ((history = open(histfile, O_WRONLY | O_CREAT | O_TRUNC,
                                   S_IRUSR | S_IWUSR)) < 0) {
        fprintf(stderr, "Failed to open history file for saving: "
                        "%s\n", strerror(errno));
        return false;
    }

    for (int i = 0; i <= lastindex; i += 1) {
        Entry *e = &entries[i];
        write(history, &sep, 1);
        write(history, e->data, e->len);
    }

    if (fsync(history) < 0) {
        fprintf(stderr, "Error saving history to disk: %s\n", strerror(errno));
        (void) close(history);
        return false;
    } else {
        fprintf(stderr, "History saved to disk.\n");
        (void) close(history);
        return true;
    }
}

void hist_recover(int32 id) {
    DEBUG_PRINT("void hist_recover(int32 id) %d\n", __LINE__)
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
            fprintf(stderr, "execlp() returned");
            return;
        case -1:
            fprintf(stderr, "fork() failed.");
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
        hist_reorder(id);

    recovered = true;
    return;
}

void hist_delete(int32 id) {
    DEBUG_PRINT("void hist_delete(int32 id) %d\n", __LINE__)
    Entry *e;
    if (lastindex == 0) {
        return;
    }

    if (id < 0) {
        id = lastindex + id + 1;
    } else if (id == lastindex) {
        hist_recover(-2);
        hist_delete(-2);
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

void hist_reorder(int32 eindex) {
    DEBUG_PRINT("void hist_reorder(int32 eindex) %d\n", __LINE__)
    Entry aux = entries[eindex];
    memmove(&entries[eindex], &entries[eindex+1], 
            (size_t) (lastindex - eindex)*sizeof(Entry));
    memmove(&entries[lastindex], &aux, sizeof(Entry));
    return;
}

void hist_clean(void) {
    DEBUG_PRINT("void hist_clean(void) %d\n", __LINE__)
    for (uint i = 0; i <= HIST_KEEP-1; i += 1) {
        Entry *e = &entries[i];
        free(e->data);
        if (e->out != e->data)
            free(e->out);

    }
    memmove(&entries[0], &entries[HIST_KEEP], HIST_KEEP*sizeof(Entry));
    memset(&entries[HIST_KEEP], 0, HIST_KEEP*sizeof(Entry));
    lastindex = HIST_KEEP-1;
    return;
}

void hist_new_entry(size_t size) {
    DEBUG_PRINT("void hist_new_entry(size_t size) %d\n", __LINE__)
    Entry *e;
    lastindex += 1;
    e = &entries[lastindex];

    if (size) {
        e->data = xalloc(NULL, size);
        e->len = size - 1;
    }
    return;
}
