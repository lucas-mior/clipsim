/* This file is part of clipsim. */
/* Copyright (C) 2022-2023 Lucas Mior */

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

#include "clipsim.h"

static const char *tmp = "/tmp/clipsim";
static File command_fifo = { .file = NULL, .fd = -1,
                             .name = "/tmp/clipsim/command.fifo" };
static File passid_fifo  = { .file = NULL, .fd = -1,
                             .name = "/tmp/clipsim/passid.fifo" };
static File content_fifo = { .file = NULL, .fd = -1,
                             .name = "/tmp/clipsim/content.fifo" };

static void ipc_daemon_history_save(void);
static void ipc_client_check_save(void);
static void ipc_daemon_pipe_entries(void);
static void ipc_daemon_pipe_id(const int32);
static void ipc_client_print_entries(void);
static int32 ipc_daemon_get_id(void);
static void ipc_client_ask_id(const int32);
static void ipc_make_fifos(void);
static void ipc_clean_fifo(const char *);
static void ipc_create_fifo(const char *);

int ipc_daemon_listen_fifo(void *unused) {
    DEBUG_PRINT("");
    (void) unused;
    char command;
    struct timespec pause;
    pause.tv_sec = 0;
    pause.tv_nsec = PAUSE10MS;

    if (mkdir(tmp, 0770) < 0) {
        if (errno != EEXIST)
            util_die_notify("Error creating %s: %s\n", tmp, strerror(errno));
    }
    ipc_make_fifos();

    while (true) {
        ssize_t r;
        nanosleep(&pause, NULL);
        if (util_open(&command_fifo, O_RDONLY) < 0)
            continue;
        mtx_lock(&lock);

        r = read(command_fifo.fd, &command, sizeof (*(&command)));
        if (r < (ssize_t) sizeof (*(&command))) {
            util_die_notify("Failed to read command from %s: %s\n",
                            command_fifo.name, strerror(errno));
        }

        util_close(&command_fifo);
        switch (command) {
        case COMMAND_PRINT:
            ipc_daemon_pipe_entries();
            break;
        case COMMAND_SAVE:
            ipc_daemon_history_save();
            break;
        case COMMAND_COPY:
            history_recover(ipc_daemon_get_id());
            break;
        case COMMAND_REMOVE:
            history_remove(ipc_daemon_get_id());
            break;
        case COMMAND_INFO:
            ipc_daemon_pipe_id(ipc_daemon_get_id());
            break;
        default:
            fprintf(stderr, "Invalid command received: '%c'\n", command);
            break;
        }

        mtx_unlock(&lock);
    }
}

void ipc_client_speak_fifo(uint command, int32 id) {
    DEBUG_PRINT("%u, %d", command, id);
    ssize_t w;
    if (util_open(&command_fifo, O_WRONLY | O_NONBLOCK) < 0) {
        fprintf(stderr, "Could not open Fifo for sending command to daemon. "
                        "Is `%s daemon` running?\n", "clipsim");
        return;
    }

    w = write(command_fifo.fd, &command, sizeof (*(&command)));
    if (w < (ssize_t) sizeof (*(&command))) {
            fprintf(stderr, "Failed to write command to %s: %s\n",
                            command_fifo.name, strerror(errno));
        util_close(&command_fifo);
        return;
    } else {
        util_close(&command_fifo);
    }

    switch (command) {
    case COMMAND_PRINT:
        ipc_client_print_entries();
        break;
    case COMMAND_SAVE:
        ipc_client_check_save();
        break;
    case COMMAND_COPY:
    case COMMAND_REMOVE:
        ipc_client_ask_id(id);
        break;
    case COMMAND_INFO:
        ipc_client_ask_id(id);
        ipc_client_print_entries();
        break;
    default:
        fprintf(stderr, "Invalid command: %u\n", command);
        break;
    }

    return;
}

void ipc_daemon_history_save(void) {
    DEBUG_PRINT("");
    char saved;
    fprintf(stderr, "Trying to save history...\n");
    if (util_open(&content_fifo, O_WRONLY) < 0)
        return;

    saved = history_save();

    write(content_fifo.fd, &saved, sizeof (*(&saved)));

    util_close(&content_fifo);
    return;
}

void ipc_client_check_save(void) {
    DEBUG_PRINT("");
    ssize_t r;
    char saved;
    fprintf(stderr, "Trying to save history...\n");
    if (util_open(&content_fifo, O_RDONLY) < 0)
        return;

    if ((r = read(content_fifo.fd, &saved, sizeof (*(&saved)))) > 0) {
        if (saved)
            fprintf(stderr, "History saved to disk.\n");
        else
            fprintf(stderr, "Error saving history to disk.\n");
    }

    util_close(&content_fifo);
    return;
}

void ipc_daemon_pipe_entries(void) {
    DEBUG_PRINT("");
    static char buffer[BUFSIZ];
    size_t w = 0;
    int32 lastindex;

    content_fifo.file = fopen(content_fifo.name, "w");
    setvbuf(content_fifo.file, buffer, _IOFBF, BUFSIZ);

    lastindex = history_lastindex();

    if (lastindex == -1) {
        fprintf(stderr, "Clipboard history empty. Start copying text.\n");
        dprintf(content_fifo.fd,
                "000 Clipboard history empty. Start copying text.\n");
        goto close;
    }

    for (int32 i = lastindex; i >= 0; i -= 1) {
        Entry *e = &entries[i];
        fprintf(content_fifo.file, "%.*d ", PRINT_DIGITS, i);
        w = fwrite(e->trimmed, 1, (e->trimmed_length+1), content_fifo.file);
        if (w < (e->trimmed_length+1)) {
            fprintf(stderr, "Error writing to client fifo.\n");
            goto close;
        }
    }
    fflush(content_fifo.file);

    close:
    util_close(&content_fifo);
    return;
}

void ipc_daemon_pipe_id(const int32 id) {
    DEBUG_PRINT("%d", id);
    Entry *e;
    int32 lastindex;

    if (util_open(&content_fifo, O_WRONLY) < 0)
        return;

    lastindex = history_lastindex();

    if (lastindex == -1) {
        fprintf(stderr, "Clipboard history empty. Start copying text.\n");
        dprintf(content_fifo.fd,
                "000 Clipboard history empty. Start copying text.\n");
        goto close;
    }

    e = &entries[id];
    if (e->image_path) {
        write(content_fifo.fd, &IMAGE_END, sizeof (*(&IMAGE_END)));
    } else {
        dprintf(content_fifo.fd,
                "Lenght: \033[31;1m%lu\n\033[0;m", e->content_length);
    }
    dprintf(content_fifo.fd, "%s", e->content);

    close:
    util_close(&content_fifo);
    return;
}


void ipc_client_print_entries(void) {
    DEBUG_PRINT("");
    static char buffer[BUFSIZ];
    ssize_t r;

    if (util_open(&content_fifo, O_RDONLY) < 0)
        return;

    r = read(content_fifo.fd, &buffer, sizeof (buffer));
    if (r <= 0) {
        fprintf(stderr, "Error reading data from %s: %s\n",
                        content_fifo.name, strerror(errno));
        util_close(&content_fifo);
        return;
    }
    if (buffer[0] != IMAGE_END) {
        do {
            fwrite(buffer, 1, (size_t) r, stdout);
        } while ((r = read(content_fifo.fd, &buffer, sizeof (buffer))) > 0);
    } else {
        int test;
        char *CLIPSIM_IMAGE_PREVIEW;
        if (r == 1)
            read(content_fifo.fd, buffer+1, sizeof (buffer)-1);
        util_close(&content_fifo);
        if ((test = open(buffer+1, O_RDONLY)) >= 0) {
            close(test);
        } else {
            fprintf(stderr, "Error opening %s: %s\n", 
                            buffer+1, strerror(errno)); 
            return;
        }

        CLIPSIM_IMAGE_PREVIEW = getenv("CLIPSIM_IMAGE_PREVIEW");
        if (CLIPSIM_IMAGE_PREVIEW == NULL)
            CLIPSIM_IMAGE_PREVIEW = "chafa";
        if (!strcmp(CLIPSIM_IMAGE_PREVIEW, "stiv"))
            execlp("stiv", "stiv", buffer+1, "30", "15", NULL);   
        else
            execlp("chafa", "chafa", buffer+1, "-s", "40x", NULL);
    }

    util_close(&content_fifo);
    return;
}

int32 ipc_daemon_get_id(void) {
    DEBUG_PRINT("void");
    int32 id;

    if ((passid_fifo.file = fopen(passid_fifo.name, "r")) == NULL) {
        fprintf(stderr, "Error opening fifo for reading id: "
                        "%s\n", strerror(errno));
        return 0;
    }

    if (fread(&id, sizeof (*(&id)), 1, passid_fifo.file) != 1) {
        fprintf(stderr, "Failed to read id from pipe: "
                        "%s\n", strerror(errno));
        util_close(&passid_fifo);
        return 0;
    }
    util_close(&passid_fifo);

    return id;
}

void ipc_client_ask_id(const int32 id) {
    DEBUG_PRINT("%d", id);
    if ((passid_fifo.file = fopen(passid_fifo.name, "w")) == NULL) {
        fprintf(stderr, "Error opening fifo for sending id to daemon: "
                        "%s\n", strerror(errno));
        return;
    }

    if (fwrite(&id, sizeof (*(&id)), 1, passid_fifo.file) != 1) {
        fprintf(stderr, "Failed to send id to daemon: "
                        "%s\n", strerror(errno));
    }

    util_close(&passid_fifo);
    return;
}

void ipc_make_fifos(void) {
    DEBUG_PRINT("");
    ipc_clean_fifo(command_fifo.name);
    ipc_clean_fifo(passid_fifo.name);
    ipc_clean_fifo(content_fifo.name);

    ipc_create_fifo(command_fifo.name);
    ipc_create_fifo(passid_fifo.name);
    ipc_create_fifo(content_fifo.name);
    return;
}

void ipc_clean_fifo(const char *fifoname) {
    if (unlink(fifoname) < 0) {
        if (errno != ENOENT) {
            util_die_notify("Error deleting %s: %s\n", fifoname, strerror(errno));
        }
    }
    return;
}

void ipc_create_fifo(const char *name) {
    DEBUG_PRINT("%s", name);
    if (mkfifo(name, 0600) < 0) {
        if (errno != EEXIST) {
            fprintf(stderr, "Failed to create fifo %s: %s\n",
                             name, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    return;
}
