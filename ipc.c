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
#include "ipc.h"
#include "history.h"
#include "util.h"
#include "text.h"

static Fifo command_fifo = { .file = NULL, .fd = -1,
                             .name = "/tmp/clipsimcmd.fifo" };
static Fifo passid_fifo  = { .file = NULL, .fd = -1,
                             .name = "/tmp/clipsimwid.fifo" };
static Fifo content_fifo = { .file = NULL, .fd = -1,
                             .name = "/tmp/clipsimdat.fifo" };

static void ipc_daemon_history_save(void);
static void ipc_client_check_save(void);
static void ipc_daemon_pipe_entries(void);
static void ipc_daemon_pipe_id(int32 id);
static void ipc_client_print_entries(void);
static void ipc_daemon_with_id(void (*)(int32));
static void ipc_client_ask_id(int32 id);
static void ipc_make_fifos(void);
static void ipc_create_fifo(const char *);
static void ipc_closef(Fifo *);
static bool ipc_openf(Fifo *, int);

void *ipc_daemon_listen_fifo(void *unused) {
    DEBUG_PRINT("*ipc_daemon_listen_fifo(void *unused) %d\n", __LINE__)
    char command;
    struct timespec pause;
    (void) unused;
    pause.tv_sec = 0;
    pause.tv_nsec = PAUSE10MS;

    ipc_make_fifos();

    while (true) {
        nanosleep(&pause, NULL);
        if (!ipc_openf(&command_fifo, O_RDONLY))
            continue;
        pthread_mutex_lock(&lock);

        if (read(command_fifo.fd, &command, sizeof(command)) < 0) {
            fprintf(stderr, "Failed to read command from pipe: "
                            "%s\n", strerror(errno));
            ipc_closef(&command_fifo);
            pthread_mutex_unlock(&lock);
            continue;
        }

        ipc_closef(&command_fifo);
        switch (command) {
            case PRINT:
                ipc_daemon_pipe_entries();
                break;
            case SAVE:
                ipc_daemon_history_save();
                break;
            case COPY:
                ipc_daemon_with_id(history_recover);
                break;
            case DELETE:
                ipc_daemon_with_id(history_delete);
                break;
            case INFO:
                ipc_daemon_with_id(ipc_daemon_pipe_id);
                break;
            default:
                fprintf(stderr, "Invalid command received: '%c'\n", command);
                break;
        }

        pthread_mutex_unlock(&lock);
    }
}

void ipc_client_speak_fifo(char command, int32 id) {
    if (!ipc_openf(&command_fifo, O_WRONLY | O_NONBLOCK)) {
        fprintf(stderr, "Could not open Fifo for sending command to daemon. "
                        "Is `%s daemon` running?\n", progname);
        return;
    }

    if (write(command_fifo.fd, &command, sizeof(command)) < 0) {
        fprintf(stderr, "Failed to send command to daemon: "
                        "%s\n", strerror(errno));
        ipc_closef(&command_fifo);
        return;
    } else {
        ipc_closef(&command_fifo);
    }

    switch (command) {
        case PRINT:
            ipc_client_print_entries();
            break;
        case SAVE:
            ipc_client_check_save();
            break;
        case COPY:
        case DELETE:
            ipc_client_ask_id(id);
            break;
        case INFO:
            ipc_client_ask_id(id);
            ipc_client_print_entries();
            break;
        default:
            fprintf(stderr, "Invalid command: '%c'\n", command);
            break;
    }

    return;
}

void ipc_daemon_history_save(void) {
    DEBUG_PRINT("ipc_daemon_history_save(void) %d\n", __LINE__)
    char saved;
    fprintf(stderr, "Trying to save history...\n");
    if (!ipc_openf(&content_fifo, O_WRONLY))
        return;

    saved = history_save();

    write(content_fifo.fd, &saved, sizeof(saved));

    ipc_closef(&content_fifo);
    return;
}

void ipc_client_check_save(void) {
    ssize_t r;
    char saved;
    fprintf(stderr, "Trying to save history...\n");
    if (!ipc_openf(&content_fifo, O_RDONLY))
        return;

    if ((r = read(content_fifo.fd, &saved, sizeof(saved))) > 0) {
        if (saved)
            fprintf(stderr, "History saved to disk.\n");
        else
            fprintf(stderr, "Error saving history to disk.\n");
    }

    ipc_closef(&content_fifo);
    return;
}

void ipc_daemon_pipe_entries(void) {
    DEBUG_PRINT("ipc_daemon_pipe_entries(void) %d\n", __LINE__)
    static char buffer[BUFSIZ];
    size_t copied = 0;

    content_fifo.file = fopen(content_fifo.name, "w");
    setvbuf(content_fifo.file, buffer, _IOFBF, BUFSIZ);

    if (lastindex == -1) {
        fprintf(stderr, "Clipboard history empty. Start copying text.\n");
        dprintf(content_fifo.fd, 
                "000 Clipboard history empty. Start copying text.\n");
        goto close;
    }

    for (int32 i = lastindex; i >= 0; i -= 1) {
        Entry *e = &entries[i];
        if (e->out == NULL)
            text_bundle_spaces(e);

        fprintf(content_fifo.file, "%.*d ", PRINT_DIGITS, i);
        copied = fwrite(e->out, 1, (e->olen+1), content_fifo.file);
        if (copied < (e->olen+1)) {
            fprintf(stderr, "Error writing to client fifo.\n");
            goto close;
        }
    }
    fflush(content_fifo.file);

    close:
    ipc_closef(&content_fifo);
    return;
}

void ipc_daemon_pipe_id(int32 id) {
    DEBUG_PRINT("ipc_daemon_pipe_id(%d) %d\n", id)
    Entry *e;

    if (!ipc_openf(&content_fifo, O_WRONLY))
        return;

    if (lastindex == -1) {
        fprintf(stderr, "Clipboard history empty. Start copying text.\n");
        dprintf(content_fifo.fd, 
                "000 Clipboard history empty. Start copying text.\n");
        goto close;
    }

    e = &entries[id];
    dprintf(content_fifo.fd, "Lenght: \033[31;1m%lu\n\033[0;m", e->len);
    dprintf(content_fifo.fd, "%s", e->data);

    close:
    ipc_closef(&content_fifo);
    return;
}

void ipc_client_print_entries(void) {
    static char buffer[BUFSIZ];
    ssize_t r;

    if (!ipc_openf(&content_fifo, O_RDONLY))
        return;

    while ((r = read(content_fifo.fd, &buffer, sizeof(buffer))) > 0)
        fwrite(buffer, 1, (size_t) r, stdout);

    ipc_closef(&content_fifo);
    return;
}

void ipc_daemon_with_id(void (*what)(int32)) {
    DEBUG_PRINT("ipc_daemon_with_id(void (*what)(int32)) %d\n", __LINE__)
    int32 id;

    if (!(passid_fifo.file = fopen(passid_fifo.name, "r"))) {
        fprintf(stderr, "Error opening fifo for reading id: "
                        "%s\n", strerror(errno));
        return;
    }

    if (fread(&id, sizeof(id), 1, passid_fifo.file) != 1) {
        fprintf(stderr, "Failed to read id from pipe: "
                        "%s\n", strerror(errno));
        ipc_closef(&passid_fifo);
        return;
    }
    ipc_closef(&passid_fifo);

    what(id);
    return;
}

void ipc_client_ask_id(int32 id) {
    DEBUG_PRINT("ipc_client_ask_id(%d)\n", id)
    if (!(passid_fifo.file = fopen(passid_fifo.name, "w"))) {
        fprintf(stderr, "Error opening fifo for sending id to daemon: "
                        "%s\n", strerror(errno));
        return;
    }

    if (fwrite(&id, sizeof(id), 1, passid_fifo.file) != 1) {
        fprintf(stderr, "Failed to send id to daemon: "
                        "%s\n", strerror(errno));
    }

    ipc_closef(&passid_fifo);
    return;
}

void ipc_make_fifos(void) {
    DEBUG_PRINT("ipc_make_fifos(void) %d\n", __LINE__)
    unlink(command_fifo.name);
    unlink(passid_fifo.name);
    unlink(content_fifo.name);
    ipc_create_fifo(command_fifo.name);
    ipc_create_fifo(passid_fifo.name);
    ipc_create_fifo(content_fifo.name);
    return;
}

void ipc_create_fifo(const char *name) {
    if (mkfifo(name, 0711) < 0) {
        if (errno != EEXIST) {
            fprintf(stderr, "Failed to create fifo %s: %s\n",
                             name, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    return;
}

void ipc_closef(Fifo *f) {
    if (f->fd >= 0) {
        if (close(f->fd) < 0)
            fprintf(stderr, "close(%s) failed: "
                            "%s\n", f->name, strerror(errno));
        f->fd = -1;
    }
    if (f->file != NULL) {
        if (fclose(f->file) != 0)
            fprintf(stderr, "fclose(%s) failed: "
                            "%s\n", f->name, strerror(errno));
        f->file = NULL;
    }
    return;
}

bool ipc_openf(Fifo *f, int flag) {
    if ((f->fd = open(f->name, flag)) < 0) {
        fprintf(stderr, "open(%s) failed: %s\n", f->name, strerror(errno));
        return false;
    } else {
        return true;
    }
}
