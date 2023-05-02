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
static void ipc_daemon_pipe_id(int32);
static void ipc_client_print_entries(void);
static void ipc_daemon_with_id(void (*)(int32));
static void ipc_client_ask_id(int32);
static void ipc_make_fifos(void);
static void ipc_create_fifo(const char *);

int ipc_daemon_listen_fifo(void *unused) {
    DEBUG_PRINT("*ipc_daemon_listen_fifo(void *unused) %d\n", __LINE__)
    char command;
    struct timespec pause;
    (void) unused;
    pause.tv_sec = 0;
    pause.tv_nsec = PAUSE10MS;

    if (mkdir(tmp, 0770) < 0) {
        if (errno != EEXIST) {
            fprintf(stderr, "Error creating %s: %s\n", tmp, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    ipc_make_fifos();

    while (true) {
        nanosleep(&pause, NULL);
        if (!util_open(&command_fifo, O_RDONLY))
            continue;
        mtx_lock(&lock);

        if (read(command_fifo.fd, &command, sizeof(command)) < 0) {
            fprintf(stderr, "Failed to read command from %s: %s\n",
                            command_fifo.name, strerror(errno));
            util_close(&command_fifo);
            mtx_unlock(&lock);
            continue;
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
                ipc_daemon_with_id(history_recover);
                break;
            case COMMAND_REMOVE:
                ipc_daemon_with_id(history_remove);
                break;
            case COMMAND_INFO:
                ipc_daemon_with_id(ipc_daemon_pipe_id);
                break;
            default:
                fprintf(stderr, "Invalid command received: '%c'\n", command);
                break;
        }

        mtx_unlock(&lock);
    }
}

void ipc_client_speak_fifo(uint command, int32 id) {
    if (!util_open(&command_fifo, O_WRONLY | O_NONBLOCK)) {
        fprintf(stderr, "Could not open Fifo for sending command to daemon. "
                        "Is `%s daemon` running?\n", "clipsim");
        return;
    }

    if (write(command_fifo.fd, &command, sizeof(command)) < 0) {
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
    DEBUG_PRINT("ipc_daemon_history_save(void) %d\n", __LINE__)
    char saved;
    fprintf(stderr, "Trying to save history...\n");
    if (!util_open(&content_fifo, O_WRONLY))
        return;

    saved = history_save();

    write(content_fifo.fd, &saved, sizeof(saved));

    util_close(&content_fifo);
    return;
}

void ipc_client_check_save(void) {
    ssize_t r;
    char saved;
    fprintf(stderr, "Trying to save history...\n");
    if (!util_open(&content_fifo, O_RDONLY))
        return;

    if ((r = read(content_fifo.fd, &saved, sizeof(saved))) > 0) {
        if (saved)
            fprintf(stderr, "History saved to disk.\n");
        else
            fprintf(stderr, "Error saving history to disk.\n");
    }

    util_close(&content_fifo);
    return;
}

void ipc_daemon_pipe_entries(void) {
    DEBUG_PRINT("ipc_daemon_pipe_entries(void) %d\n", __LINE__)
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

void ipc_daemon_pipe_id(int32 id) {
    DEBUG_PRINT("ipc_daemon_pipe_id(%d)\n", id)
    Entry *e;
    int32 lastindex;

    if (!util_open(&content_fifo, O_WRONLY))
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
        write(content_fifo.fd, &IMAGE_END, 1);
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
    static char buffer[BUFSIZ];
    ssize_t r;

    if (!util_open(&content_fifo, O_RDONLY))
        return;

    r = read(content_fifo.fd, &buffer, sizeof(buffer));
    if (r <= 0) {
        fprintf(stderr, "Error reading data from %s: %s\n",
                        content_fifo.name, strerror(errno));
        util_close(&content_fifo);
        return;
    }
    if (buffer[0] != IMAGE_END) {
        do {
            fwrite(buffer, 1, (size_t) r, stdout);
        } while ((r = read(content_fifo.fd, &buffer, sizeof(buffer))) > 0);
    } else {
        int test;
        char *CLIPSIM_IMAGE_PREVIEW;
        if (r == 1)
            read(content_fifo.fd, buffer+1, sizeof(buffer)-1);
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

void ipc_daemon_with_id(void (*func)(int32)) {
    DEBUG_PRINT("ipc_daemon_with_id(void (*func)(int32)) %d\n", __LINE__)
    int32 id;

    if (!(passid_fifo.file = fopen(passid_fifo.name, "r"))) {
        fprintf(stderr, "Error opening fifo for reading id: "
                        "%s\n", strerror(errno));
        return;
    }

    if (fread(&id, sizeof(id), 1, passid_fifo.file) != 1) {
        fprintf(stderr, "Failed to read id from pipe: "
                        "%s\n", strerror(errno));
        util_close(&passid_fifo);
        return;
    }
    util_close(&passid_fifo);

    func(id);
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

    util_close(&passid_fifo);
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
