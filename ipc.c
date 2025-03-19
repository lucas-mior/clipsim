/* This file is part of clipsim.
 * Copyright (C) 2024 Lucas Mior

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "clipsim.h"

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
static int32 ipc_daemon_get_id(void);
static void ipc_client_ask_id(int32);
static void ipc_make_fifos(void);
static void ipc_clean_fifo(const char *);
static void ipc_create_fifo(const char *);
static void sig_abrt_handler(int32) __attribute__((noreturn));

void
sig_abrt_handler(int32 unused) {
    (void) unused;
    error("Received SIGABRT signal, something is wrong with history file.\n");
    error("Creating backup for history file...\n");
    history_backup();

    error("Restarting clipsim --daemon with empty history...\n");
    execlp("clipsim", "clipsim", "--daemon", NULL);

    error("Error while trying to exec clipsim --daemon: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
}

int32
ipc_daemon_listen_fifo(void *unused) {
    DEBUG_PRINT("void");
    (void) unused;
    char command;
    struct timespec pause;
    pause.tv_sec = 0;
    pause.tv_nsec = PAUSE10MS;

    ipc_make_fifos();

    signal(SIGABRT, sig_abrt_handler);

    while (true) {
        isize r;
        nanosleep(&pause, NULL);
        if (util_open(&command_fifo, O_RDONLY) < 0)
            continue;
        mtx_lock(&lock);

        r = read(command_fifo.fd, &command, sizeof(*(&command)));
        if (r < (isize) sizeof(*(&command))) {
            error("Error reading command from %s: %s\n",
                  command_fifo.name, strerror(errno));
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
            history_recover(ipc_daemon_get_id());
            break;
        case COMMAND_REMOVE:
            history_remove(ipc_daemon_get_id());
            break;
        case COMMAND_INFO:
            ipc_daemon_pipe_id(ipc_daemon_get_id());
            break;
        default:
            error("Invalid command received: '%c'\n", command);
        }

        mtx_unlock(&lock);
    }
}

void
ipc_client_speak_fifo(int32 command, int32 id) {
    DEBUG_PRINT("%u, %d", command, id);
    isize w;
    if (util_open(&command_fifo, O_WRONLY | O_NONBLOCK) < 0) {
        error("Could not open Fifo for sending command to daemon. "
              "Is `%s daemon` running?\n", "clipsim");
        exit(EXIT_FAILURE);
    }

    w = write(command_fifo.fd, &command, sizeof(*(&command)));
    util_close(&command_fifo);
    if (w < (isize) sizeof(*(&command))) {
        error("Error writing command to %s: %s\n",
              command_fifo.name, strerror(errno));
        exit(EXIT_FAILURE);
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
        error("Invalid command: %u\n", command);
        exit(EXIT_FAILURE);
    }

    return;
}

void
ipc_daemon_history_save(void) {
    DEBUG_PRINT("void");
    char saved;
    isize saved_size = sizeof(*(&saved));
    error("Trying to save history...\n");
    if (util_open(&content_fifo, O_WRONLY) < 0)
        return;

    saved = history_save();

    if (write(content_fifo.fd, &saved, (usize) saved_size) < saved_size)
        error("Error sending save result to client.\n");

    util_close(&content_fifo);
    return;
}

void
ipc_client_check_save(void) {
    DEBUG_PRINT("void");
    isize r;
    char saved = 0;
    error("Trying to save history...\n");
    if (util_open(&content_fifo, O_RDONLY) < 0)
        exit(EXIT_FAILURE);

    if ((r = read(content_fifo.fd, &saved, sizeof(*(&saved)))) > 0) {
        if (saved)
            error("History saved to disk.\n");
        else
            error("Error saving history to disk.\n");
    }

    util_close(&content_fifo);
    if (!saved)
        exit(EXIT_FAILURE);
    return;
}

void
ipc_daemon_pipe_entries(void) {
    DEBUG_PRINT("void");
    static char buffer[BUFSIZ];
    int32 history_length;

    if ((content_fifo.file = fopen(content_fifo.name, "w")) == NULL) {
        error("Error opening %s: %s.\n", content_fifo.name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    setvbuf(content_fifo.file, buffer, _IOFBF, BUFSIZ);

    history_length = history_length_get();

    if (history_length <= 0) {
        error("Clipboard history empty. Start copying text.\n");
        goto close;
    }

    for (int32 i = history_length - 1; i >= 0; i -= 1) {
        Entry *e = &entries[i];
        usize size = (usize) e->trimmed_length + 1;
        char *trimmed = &e->content[e->trimmed];

        fprintf(content_fifo.file, "%.*d ", PRINT_DIGITS, i);
        if (fwrite(trimmed, 1, size, content_fifo.file) < size) {
            error("Error writing to client fifo.\n");
            break;
        }
    }
    fflush(content_fifo.file);

    close:
    util_close(&content_fifo);
    return;
}

void
ipc_daemon_pipe_id(int32 id) {
    DEBUG_PRINT("%d", id);
    Entry *e;
    int32 history_length;
    usize tag_size = sizeof(*(&IMAGE_TAG));

    if (util_open(&content_fifo, O_WRONLY) < 0)
        return;

    history_length = history_length_get();

    if (history_length <= -1) {
        error("Clipboard history empty. Start copying text.\n");
        dprintf(content_fifo.fd,
                "000 Clipboard history empty. Start copying text.\n");
        goto close;
    }

    if (id < 0)
        id = history_length + id;
    if ((id >= history_length) || (id < 0)) {
        error("Invalid index: %d\n", id);
        goto close;
    }

    e = &entries[id];
    if (is_image[id]) {
        if (write(content_fifo.fd, &IMAGE_TAG, tag_size) < (isize)tag_size) {
            dprintf(content_fifo.fd, "Error printing image tag.\n");
            goto close;
        }
    } else {
        dprintf(content_fifo.fd,
                "Lenght: \033[31;1m%d\n\033[0;m", e->content_length);
    }
    dprintf(content_fifo.fd, "%s", e->content);

    close:
    util_close(&content_fifo);
    return;
}


void
ipc_client_print_entries(void) {
    DEBUG_PRINT("void");
    static char buffer[BUFSIZ];
    isize r;

    if (util_open(&content_fifo, O_RDONLY) < 0)
        return;

    r = read(content_fifo.fd, buffer, sizeof(buffer));
    if (r <= 0) {
        error("Error reading data from %s", content_fifo.name);
        if (r < 0)
            error(": %s", strerror(errno));
        error(".\n");

        util_close(&content_fifo);
        exit(EXIT_FAILURE);
    }

    if (buffer[0] != IMAGE_TAG) {
        do {
            fwrite(buffer, 1, (usize) r, stdout);
        } while ((r = read(content_fifo.fd, buffer, sizeof(buffer))) > 0);
    } else {
        int32 test;
        char *CLIPSIM_IMAGE_PREVIEW;

        if (r == 1) {
            r = read(content_fifo.fd, buffer + 1, sizeof(buffer) - 1);
            if (r <= 0) {
                util_die_notify("Error reading image name from %s.\n",
                                content_fifo.name);
            }
        }
        util_close(&content_fifo);
        if ((test = open(buffer + 1, O_RDONLY)) >= 0) {
            close(test);
        } else {
            error("Error opening %s: %s\n", buffer + 1, strerror(errno));
            return;
        }

        CLIPSIM_IMAGE_PREVIEW = getenv("CLIPSIM_IMAGE_PREVIEW");
        if (CLIPSIM_IMAGE_PREVIEW == NULL)
            CLIPSIM_IMAGE_PREVIEW = "chafa";
        if (!strcmp(CLIPSIM_IMAGE_PREVIEW, "stiv_draw"))
            execlp("stiv_draw", "stiv_draw", buffer + 1, "30", "15", NULL);
        else
            execlp("chafa", "chafa", buffer + 1, "-s", "40x", NULL);
    }

    util_close(&content_fifo);
    return;
}

int32
ipc_daemon_get_id(void) {
    DEBUG_PRINT("void");
    int32 id;

    if ((passid_fifo.file = fopen(passid_fifo.name, "r")) == NULL) {
        error("Error opening fifo for reading id: %s\n", strerror(errno));
        return HISTORY_INVALID_ID;
    }

    if (fread(&id, sizeof(*(&id)), 1, passid_fifo.file) != 1) {
        error("Error reading id from pipe: %s\n", strerror(errno));
        util_close(&passid_fifo);
        return HISTORY_INVALID_ID;
    }

    util_close(&passid_fifo);
    return id;
}

void
ipc_client_ask_id(int32 id) {
    DEBUG_PRINT("%d", id);
    if ((passid_fifo.file = fopen(passid_fifo.name, "w")) == NULL) {
        util_die_notify("Error opening fifo for sending id to daemon: "
                        "%s\n", strerror(errno));
    }

    if (fwrite(&id, sizeof(*(&id)), 1, passid_fifo.file) != 1)
        error("Error sending id to daemon: %s\n", strerror(errno));

    util_close(&passid_fifo);
    return;
}

void
ipc_make_fifos(void) {
    DEBUG_PRINT("void");

    char *tmp = "/tmp/clipsim";
    if (mkdir(tmp, 0770) < 0) {
        if (errno != EEXIST)
            util_die_notify("Error creating %s: %s\n", tmp, strerror(errno));
    }

    ipc_clean_fifo(command_fifo.name);
    ipc_clean_fifo(passid_fifo.name);
    ipc_clean_fifo(content_fifo.name);

    ipc_create_fifo(command_fifo.name);
    ipc_create_fifo(passid_fifo.name);
    ipc_create_fifo(content_fifo.name);
    return;
}

void
ipc_clean_fifo(const char *name) {
    DEBUG_PRINT("%s", name);
    if (unlink(name) < 0) {
        if (errno != ENOENT)
            util_die_notify("Error deleting %s: %s\n", name, strerror(errno));
    }
    return;
}

void
ipc_create_fifo(const char *name) {
    DEBUG_PRINT("%s", name);
    if (mkfifo(name, 0600) < 0) {
        if (errno != EEXIST) {
            util_die_notify("Error creating fifo %s: %s\n",
                            name, strerror(errno));
        }
    }
    return;
}
