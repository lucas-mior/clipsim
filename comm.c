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
#include "hist.h"
#include "util.h"
#include "text.h"

static Fifo cmd = { .file = NULL, .fd = NOFD, .name = "/tmp/clipsimcmd.fifo" };
static Fifo wid = { .file = NULL, .fd = NOFD, .name = "/tmp/clipsimwid.fifo" };
static Fifo dat = { .file = NULL, .fd = NOFD, .name = "/tmp/clipsimdat.fifo" };

static void comm_client_check_save(void);
static void comm_daemon_hist_save(void);
static bool comm_flush_dat(char *, size_t *);
static void comm_daemon_pipe_entries(void);
static void comm_daemon_pipe_id(int32 id);
static void comm_client_print_entries(void);
static void comm_daemon_with_id(void (*what)(int32));
static void comm_client_ask_id(int32 id);
static void comm_make_fifos(void);
static void comm_create_fifo(const char *);
static void comm_closef(Fifo *);
static bool comm_openf(Fifo *, int);

void *comm_daemon_listen_fifo(void *unused) {
    DEBUG_PRINT("void *comm_daemon_listen_fifo(void *unused) %d\n", __LINE__)
    char command;
    struct timespec pause;
    (void) unused;
    pause.tv_sec = 0;
    pause.tv_nsec = PAUSE10MS;

    comm_make_fifos();

    while (true) {
        nanosleep(&pause, NULL);
        if (!comm_openf(&cmd, O_RDONLY))
            continue;
        pthread_mutex_lock(&lock);

        if (read(cmd.fd, &command, sizeof(command)) < 0) {
            fprintf(stderr, "Failed to read command from pipe: "
                            "%s\n", strerror(errno));
            comm_closef(&cmd);
            pthread_mutex_unlock(&lock);
            continue;
        }

        comm_closef(&cmd);
        switch (command) {
            case PRINT:
                comm_daemon_pipe_entries();
                break;
            case SAVE:
                comm_daemon_hist_save();
                break;
            case COPY:
                comm_daemon_with_id(hist_recover);
                break;
            case DELETE:
                comm_daemon_with_id(hist_delete);
                break;
            case INFO:
                comm_daemon_with_id(comm_daemon_pipe_id);
                break;
            default:
                fprintf(stderr, "Invalid command received: '%c'\n", command);
                break;
        }

        pthread_mutex_unlock(&lock);
    }
}

void comm_client_speak_fifo(char command, int32 id) {
    if (!comm_openf(&cmd, O_WRONLY | O_NONBLOCK)) {
        fprintf(stderr, "Could not open Fifo for sending command to daemon. "
                        "Is `%s daemon` running?\n", progname);
        return;
    }

    if (write(cmd.fd, &command, sizeof(command)) < 0) {
        fprintf(stderr, "Failed to send command to daemon: "
                        "%s\n", strerror(errno));
        comm_closef(&cmd);
        return;
    } else {
        comm_closef(&cmd);
    }

    switch (command) {
        case PRINT:
            comm_client_print_entries();
            break;
        case SAVE:
            comm_client_check_save();
            break;
        case COPY:
        case DELETE:
            comm_client_ask_id(id);
            break;
        case INFO:
            comm_client_ask_id(id);
            comm_client_print_entries();
            break;
        default:
            fprintf(stderr, "Invalid command: '%c'\n", command);
            break;
    }

    return;
}

void comm_client_check_save(void) {
    ssize_t r;
    char saved;
    fprintf(stderr, "Trying to save history...\n");
    if (!comm_openf(&dat, O_RDONLY))
        return;

    if ((r = read(dat.fd, &saved, sizeof(saved))) > 0) {
        if (saved)
            fprintf(stderr, "History saved to disk.\n");
        else
            fprintf(stderr, "Error saving history to disk.\n");
    }

    comm_closef(&dat);
    return;
}

void comm_daemon_hist_save(void) {
    DEBUG_PRINT("void comm_daemon_hist_save(void) %d\n", __LINE__)
    char saved;
    fprintf(stderr, "Trying to save history...\n");
    if (!comm_openf(&dat, O_WRONLY))
        return;

    saved = hist_save();

    write(dat.fd, &saved, sizeof(saved));

    comm_closef(&dat);
    return;
}

inline bool comm_flush_dat(char *pbuf, size_t *copied) {
    DEBUG_PRINT("inline bool comm_flush_dat(char *pbuf, size_t *copied) %d\n", __LINE__)
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

void comm_daemon_pipe_entries(void) {
    DEBUG_PRINT("void comm_daemon_pipe_entries(void) %d\n", __LINE__)
    static char buffer[OUT_BUF];
    char *pbuf;
    size_t copied = 0;

    if (!comm_openf(&dat, O_WRONLY))
        return;

    if (lastindex == -1) {
        fprintf(stderr, "Clipboard history empty. Start copying text.\n");
        dprintf(dat.fd, "000 Clipboard history empty. Start copying text.\n");
        goto close;
    }

    pbuf = buffer;
    for (int32 i = lastindex; i >= 0; i -= 1) {
        Entry *e = &entries[i];
        if (e->out == NULL)
            text_bundle_spaces(e);

        resetbuf:
        if ((copied + (PRINT_DIGITS+1) + (e->olen+1)) <= sizeof(buffer)) {
            sprintf(pbuf, "%.*d ", PRINT_DIGITS, i);
            pbuf += (PRINT_DIGITS+1);
            copied += (PRINT_DIGITS+1);
            memcpy(pbuf, e->out, (e->olen+1));
            copied += (e->olen+1);
            pbuf += (e->olen+1);
        } else {
            pbuf = buffer;
            if (comm_flush_dat(pbuf, &copied))
                goto resetbuf;
            else
                goto close;
        }
    }
    pbuf = buffer;
    comm_flush_dat(pbuf, &copied);

    close:
    comm_closef(&dat);
    return;
}

void comm_daemon_pipe_id(int32 id) {
    DEBUG_PRINT("void comm_daemon_pipe_id(int32 id) %d\n", __LINE__)
    Entry *e;

    if (!comm_openf(&dat, O_WRONLY))
        return;

    if (lastindex == -1) {
        fprintf(stderr, "Clipboard history empty. Start copying text.\n");
        dprintf(dat.fd, "000 Clipboard history empty. Start copying text.\n");
        goto close;
    }

    e = &entries[id];
    dprintf(dat.fd, "Lenght: \033[31;1m%lu\n\033[0;m", e->len);
    dprintf(dat.fd, "%s", e->data);

    close:
    comm_closef(&dat);
    return;
}

void comm_client_print_entries(void) {
    static char buffer[OUT_BUF];
    ssize_t r;

    if (!comm_openf(&dat, O_RDONLY))
        return;

    while ((r = read(dat.fd, &buffer, sizeof(buffer))) > 0)
        fwrite(buffer, 1, (size_t) r, stdout);

    comm_closef(&dat);
    return;
}

void comm_daemon_with_id(void (*what)(int32)) {
    DEBUG_PRINT("void comm_daemon_with_id(void (*what)(int32)) %d\n", __LINE__)
    int32 id;

    if (!(wid.file = fopen(wid.name, "r"))) {
        fprintf(stderr, "Error opening fifo for reading id: "
                        "%s\n", strerror(errno));
        return;
    }

    if (fread(&id, sizeof(id), 1, wid.file) != 1) {
        fprintf(stderr, "Failed to read id from pipe: "
                        "%s\n", strerror(errno));
        comm_closef(&wid);
        return;
    }
    comm_closef(&wid);

    what(id);
    return;
}

void comm_client_ask_id(int32 id) {
    DEBUG_PRINT("void comm_client_ask_id(int32 id) %d\n", __LINE__)
    if (!(wid.file = fopen(wid.name, "w"))) {
        fprintf(stderr, "Error opening fifo for sending id to daemon: "
                        "%s\n", strerror(errno));
        return;
    }

    if (fwrite(&id, sizeof(id), 1, wid.file) != 1) {
        fprintf(stderr, "Failed to send id to daemon: "
                        "%s\n", strerror(errno));
    }

    comm_closef(&wid);
    return;
}

void comm_make_fifos(void) {
    DEBUG_PRINT("void comm_make_fifos(void) %d\n", __LINE__)
    unlink(cmd.name);
    unlink(wid.name);
    unlink(dat.name);
    comm_create_fifo(cmd.name);
    comm_create_fifo(wid.name);
    comm_create_fifo(dat.name);
    return;
}

void comm_create_fifo(const char *name) {
    if (mkfifo(name, 0711) < 0) {
        if (errno != EEXIST) {
            fprintf(stderr, "Failed to create fifo %s: %s\n",
                             name, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    return;
}

void comm_closef(Fifo *f) {
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

bool comm_openf(Fifo *f, int flag) {
    if ((f->fd = open(f->name, flag)) < 0) {
        fprintf(stderr, "open(%s) failed: %s\n", f->name, strerror(errno));
        return false;
    } else {
        return true;
    }
}
