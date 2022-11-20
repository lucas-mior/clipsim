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
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "clipsim.h"
#include "comm.h"
#include "config.h"
#include "hist.h"
#include "util.h"

static Fifo cmd = { .file = NULL, .fd = NOFD, .name = "/tmp/clipsimcmd.fifo" };
static Fifo wid = { .file = NULL, .fd = NOFD, .name = "/tmp/clipsimwid.fifo" };
static Fifo dat = { .file = NULL, .fd = NOFD, .name = "/tmp/clipsimdat.fifo" };

static void make_fifos(void);
static void create_fifo(const char *name);
static void daemon_pipe_entries(void);
static void daemon_pipe_id(int);
static void client_print_entries(void);
static void daemon_with_id(void (*)(int));
static void client_ask_id(int);
static void daemon_hist_save(void);
static inline void bundle_spaces(Entry *);
static inline bool flush_dat(char *, size_t *);
static void closef(Fifo *);
static bool openf(Fifo *, int);

void *daemon_listen_fifo(void *unused) {
    char command;
    struct timespec pause;
    (void) unused;
    pause.tv_sec = 0;
    pause.tv_nsec = PAUSE10MS;

    make_fifos();

    while (true) {
        nanosleep(&pause, NULL);
        if (!openf(&cmd, O_RDONLY))
            continue;
        pthread_mutex_lock(&lock);

        if (read(cmd.fd, &command, sizeof(command)) < 0) {
            fprintf(stderr, "Failed to read command from pipe: "
                            "%s\n", strerror(errno));
            closef(&cmd);
            pthread_mutex_unlock(&lock);
            continue;
        }

        closef(&cmd);
        switch (command) {
            case CHAR_PRINT:
                daemon_pipe_entries();
                break;
            case CHAR_SAVE:
                daemon_hist_save();
                break;
            case CHAR_COPY:
                daemon_with_id(hist_rec);
                break;
            case CHAR_DELETE:
                daemon_with_id(hist_del);
                break;
            case CHAR_INFO:
                daemon_with_id(daemon_pipe_id);
                break;
            default:
                fprintf(stderr, "Invalid command received: '%c'\n", command);
                break;
        }

        pthread_mutex_unlock(&lock);
    }
}

static void make_fifos(void) {
    unlink(cmd.name);
    unlink(wid.name);
    unlink(dat.name);
    create_fifo(cmd.name);
    create_fifo(wid.name);
    create_fifo(dat.name);
    return;
}

static void create_fifo(const char *name) {
    if (mkfifo(name, 0711) < 0) {
        if (errno != EEXIST) {
            fprintf(stderr, "Failed to create fifo %s: %s\n",
                             name, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    return;
}

static void client_check_save(void) {
    ssize_t r;
    char saved;
    fprintf(stderr, "Trying to save history...\n");
    if (!openf(&dat, O_RDONLY))
        return;

    if ((r = read(dat.fd, &saved, sizeof(saved))) > 0) {
        if (saved)
            fprintf(stderr, "History saved to disk.\n");
        else
            fprintf(stderr, "Error saving history to disk.\n");
    }

    closef(&dat);
    return;
}

static void daemon_hist_save(void) {
    char saved;
    fprintf(stderr, "Trying to save history...\n");
    if (!openf(&dat, O_WRONLY))
        return;

    saved = hist_save();

    write(dat.fd, &saved, sizeof(saved));

    closef(&dat);
    return;
}

void client_speak_fifo(char command, int id) {
    if (!openf(&cmd, O_WRONLY | O_NONBLOCK)) {
        fprintf(stderr, "Could not open Fifo for sending command to daemon. "
                        "Is `%s daemon` running?\n", progname);
        return;
    }

    if (write(cmd.fd, &command, sizeof(command)) < 0) {
        fprintf(stderr, "Failed to send command to daemon: "
                        "%s\n", strerror(errno));
        closef(&cmd);
        return;
    } else {
        closef(&cmd);
    }

    switch (command) {
        case CHAR_PRINT:
            client_print_entries();
            break;
        case CHAR_SAVE:
            client_check_save();
            break;
        case CHAR_COPY:
        case CHAR_DELETE:
            client_ask_id(id);
            break;
        case CHAR_INFO:
            client_ask_id(id);
            client_print_entries();
            break;
        default:
            fprintf(stderr, "Invalid command: '%c'\n", command);
            break;
    }

    return;
}

static inline void bundle_spaces(Entry *e) {
    char *out;
    char temp = '\0';
    bool rectemp = false;
    char *c = e->data;

    out = e->out = emalloc(MIN(e->len+1, OUT_SIZE+1));

    if (e->len >= OUT_SIZE) {
        rectemp = true;
        temp = e->data[OUT_SIZE];
        e->data[OUT_SIZE] = '\0';
    }

    while ((*c == ' ') || (*c == '\t') || (*c == '\n'))
        c++;
    while (*c != '\0') {
        while (((*c == ' ') || (*c == '\t') || (*c == '\n'))
            && ((*(c+1) == ' ') || (*(c+1) == '\t') || (*(c+1) == '\n')))
            c++;

       *out++ = *c++;
       e->olen += 1;
    }
    *out++ = '\0';

    if (rectemp)
        e->data[OUT_SIZE] = temp;

    if (e->olen == e->len) {
        free(e->out);
        e->out = e->data;
    }
    return;
}

static inline bool flush_dat(char *pbuf, size_t *copied) {
    ssize_t w;
    while (*copied > 0) {
        if ((w = write(dat.fd, pbuf, *copied)) < 0) {
            fprintf(stderr, "write() failed in flush_dat(): "
                            "%s\n", strerror(errno));
            return false;
        } else {
            *copied -= (size_t) w;
        }
    }
    return true;
}

void daemon_pipe_entries(void) {
    static char buffer[OUT_BUF];
    char *pbuf;
    size_t copied = 0;
    Entry *e = last_entry;

    if (!openf(&dat, O_WRONLY))
        return;

    if (!e->data) {
        fprintf(stderr, "Clipboard history empty. Start copying text.\n");
        dprintf(dat.fd, "001 Clipboard history empty. Start copying text.\n");
        goto close;
    }

    pbuf = buffer;
    do {
        if (e->out == NULL)
            bundle_spaces(e);

        resetbuf:
        if ((copied + (PRINT_DIGITS+1) + (e->olen+1)) <= sizeof(buffer)) {
            sprintf(pbuf, "%.*d ", PRINT_DIGITS, e->id);
            pbuf += (PRINT_DIGITS+1);
            copied += (PRINT_DIGITS+1);
            memcpy(pbuf, e->out, (e->olen+1));
            copied += (e->olen+1);
            pbuf += (e->olen+1);
        } else {
            pbuf = buffer;
            if (flush_dat(pbuf, &copied))
                goto resetbuf;
            else
                goto close;
        }
        e = e->prev;
    } while (e && e->data);
    pbuf = buffer;
    flush_dat(pbuf, &copied);

    close:
    closef(&dat);
    return;
}

void daemon_pipe_id(int id) {
    Entry *e = last_entry;
    bool found = false;

    if (!openf(&dat, O_WRONLY))
        return;

    if (!e->data) {
        fprintf(stderr, "Clipboard history empty. Start copying text.\n");
        dprintf(dat.fd, "001 Clipboard history empty. Start copying text.\n");
        goto close;
    }

    do {
        if (id == e->id) {
            found = true;
            dprintf(dat.fd, "%s", e->data);
            break;
        }
        e = e->prev;
    } while (e && e->data);

    if (!found) {
        dprintf(dat.fd, "Entry with id number %d not found.\n", id);
        fprintf(stderr, "Entry with id number %d not found.\n", id);
    }
    close:
    closef(&dat);
    return;
}

void client_print_entries(void) {
    static char buffer[OUT_BUF];
    ssize_t r;

    if (!openf(&dat, O_RDONLY))
        return;

    while ((r = read(dat.fd, &buffer, sizeof(buffer))) > 0)
        fwrite(buffer, 1, (size_t) r, stdout);

    closef(&dat);
    return;
}

void daemon_with_id(void (*what)(int)) {
    int id;

    if (!(wid.file = fopen(wid.name, "r"))) {
        fprintf(stderr, "Error opening fifo for reading id: "
                        "%s\n", strerror(errno));
        return;
    }

    if (fread(&id, sizeof(id), 1, wid.file) != 1) {
        fprintf(stderr, "Failed to read id from pipe: "
                        "%s\n", strerror(errno));
        closef(&wid);
        return;
    }
    closef(&wid);

    what(id);
    return;
}

void client_ask_id(int id) {
    if (!(wid.file = fopen(wid.name, "w"))) {
        fprintf(stderr, "Error opening fifo for sending id to daemon: "
                        "%s\n", strerror(errno));
        return;
    }

    if (fwrite(&id, sizeof(id), 1, wid.file) != 1) {
        fprintf(stderr, "Failed to send id to daemon: "
                        "%s\n", strerror(errno));
    }

    closef(&wid);
    return;
}

void closef(Fifo *f) {
    if (f->fd != NOFD) {
        if (close(f->fd) < 0)
            fprintf(stderr, "close(%s) failed: "
                            "%s\n", f->name, strerror(errno));
        f->fd = NOFD;
    }
    if (f->file != NULL) {
        if (fclose(f->file) != 0)
            fprintf(stderr, "fclose(%s) failed: "
                            "%s\n", f->name, strerror(errno));
        f->file = NULL;
    }
    return;
}

bool openf(Fifo *f, int flag) {
    if ((f->fd = open(f->name, flag)) < 0) {
        fprintf(stderr, "open(%s) failed: %s\n", f->name, strerror(errno));
        return false;
    } else {
        return true;
    }
}
