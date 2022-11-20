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
#include "config.h"
#include "hist.h"
#include "util.h"

static char *histfile = NULL;
static volatile bool recovered = false;
static const char sep = 0x01;
static const int max_file_len = 1024;

static void hist_find(void);
static Entry *equal_to_previous(char *, size_t);
static void new_entry(size_t);
static void hist_reorder(Entry *);
static void hist_clean(uint);

static inline void hist_find(void) {
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
    histfile = emalloc(min+1);

    (void) snprintf(histfile, min+1, "%s/%s", cache, clipsim);
    return;
}

void hist_read(void) {
    FILE *history = NULL;
    size_t i = 0;
    int c;
    size_t to_alloc;

    hist_find();
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

    new_entry(to_alloc = DEF_ALLOC);
    while ((c = fgetc(history)) != EOF) {
        if (c == sep) {
            if (i >= (to_alloc - 1)) {
                last_entry->data = erealloc(last_entry->data, to_alloc+1);
            } else if (i < to_alloc) {
                last_entry->data = erealloc(last_entry->data, i+1);
            }

            last_entry->len = i;
            last_entry->data[i] = '\0';
            new_entry(to_alloc = DEF_ALLOC);
            i = 0;
        } else {
            if (i >= (to_alloc - 1)) {
                if (to_alloc < 0xFFFF) {
                    to_alloc *= 2;
                } else {
                    fprintf(stderr, "Too long entry. Skipping.\n");
                    free(last_entry->data);
                    last_entry = last_entry->prev;
                    free(last_entry->next);
                    last_entry->next = NULL;
                    new_entry(to_alloc = DEF_ALLOC);
                }
                last_entry->data = erealloc(last_entry->data, to_alloc);
            }
            last_entry->data[i] = (char) c;
            i += 1;
        }
    }
    if (i >= (to_alloc - 1))
        last_entry->data = erealloc(last_entry->data, to_alloc+1);

    last_entry->len = i;
    last_entry->data[i] = '\0';

    (void) fclose(history);
    return;
}

static Entry *equal_to_previous(char *save, size_t min) {
    Entry *e = last_entry;
    if (!last_entry->data)
        return NULL;
    do {
        if (e->len == min) {
            if (!strcmp(e->data, save)) {
                return e;
            }
        }
    } while ((e = e->prev));
    return NULL;
}

void hist_add(char *save, ulong len) {
    size_t min;
    Entry *e;

    if (recovered) {
        recovered = false;
        return;
    }

    min = MIN(len, MAX_ENTRY_SIZE);
    save[min] = '\0';

    if (save[min-1] == '\n') {
        save[min-1] = '\0';
        min -= 1;
    }

    if (min <= 1) {
        if ((' ' <= *save) && (*save <= '~')) {
            fprintf(stderr, "Ignoring single character '%c'\n", *save);
            return;
        }
    }

    if ((e = equal_to_previous(save, min))) {
        fprintf(stderr, "Entry is equal to previous entry. Reordering...\n");
        if (e->id != last_entry->id)
            hist_reorder(e);
        free(save);
        return;
    }

    new_entry(0);
    last_entry->data = save;
    last_entry->data[min] = '\0';
    last_entry->len = min;

    if (last_entry->id > (SAVE_INTERVAL * 2))
        hist_save();

    return;
}

bool hist_save(void) {
    int history;
    Entry *e = last_entry;
    if (last_entry->id > SAVE_INTERVAL)
        hist_clean(SAVE_INTERVAL);

    while (e->prev)
        e = e->prev;

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

    do {
        if (e->data != NULL) {
            write(history, &sep, 1);
            write(history, e->data, e->len);
            /* dprintf(history, "%c%s", sep, e->data); */
        }
    } while ((e = e->next));

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

void hist_rec(int id) {
    Entry *e;
    bool found = false;
    pid_t child = -1;
    int fd[2];

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

    if (id <= 0)
        id = last_entry->id + id;

    if (!last_entry->data) {
        fprintf(stderr, "Clipboard history empty. Start copying text.\n");
        dprintf(fd[1], "Clipboard history empty. Start copying text.\n");
        close(fd[1]);
        wait(NULL);
        return;
    }
    e = last_entry;

    do {
        if (e->id == id) {
            found = true;
            dprintf(fd[1], "%s", e->data);
            close(fd[1]);
            wait(NULL);
            break;
        }
    } while ((e = e->prev) && e->data);

    if ((id != last_entry->id) && found)
        hist_reorder(e);

    if (!found) {
        fprintf(stderr, "Id Number %d not found.\n", id);
        close(fd[1]);
        kill(child, SIGKILL);
    }

    recovered = true;
    return;
}

void hist_del(int id) {
    Entry *e = last_entry->prev;
    Entry *aux;

    if (id <= 0) {
        id = last_entry->id + id;
    } else if (id == last_entry->id) {
        hist_rec(-1);
        hist_del(-1);
        return;
    }
    do {
        if (e->id == id) {
            aux = e->prev;
            e->prev->next = e->next;
            e->next->prev = e->prev;
            free(e->data);
            if (e->out != e->data)
                free(e->out);
            free(e);

            while ((aux = aux->next))
                aux->id -= 1;

            return;
        }
    } while ((e = e->prev));

    fprintf(stderr, "Id %d not found for deletion.\n", id);
    return;
}

static void hist_reorder(Entry *e) {
    Entry *aux = e->prev;

    e->prev->next = e->next;
    e->next->prev = e->prev;
    e->id = last_entry->id + 1;

    last_entry->next = e;
    e->next = NULL;
    e->prev = last_entry;
    last_entry = e;
    do {
        aux = aux->next;
        aux->id -= 1;
    } while (aux->next);

    return;
}

static void hist_clean(uint save) {
    Entry *e;
    Entry *first;
    e = last_entry;

    while (e->prev && (save > 0)) {
        e->id = (int) save;
        save -= 1;
        e = e->prev;
    }
    first = e->next;

    while (e->prev)
        e = e->prev;

    while (e != first) {
        free(e->data);
        if (e->out != e->data)
            free(e->out);
        e = e->next;
        free(e->prev);
    }

    e->prev = NULL;

    return;
}

static void new_entry(size_t size) {
    Entry *old = last_entry;

    last_entry->next = emalloc(sizeof(Entry));
    last_entry = last_entry->next;
    last_entry->id = old->id + 1;
    last_entry->prev = old;
    last_entry->next = NULL;
    last_entry->olen = 0;
    last_entry->out = NULL;
    if (size) {
        last_entry->data = emalloc(size);
        last_entry->len = size - 1;
    }
    return;
}
