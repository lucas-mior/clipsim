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

Entry entries[HISTORY_BUFFER_SIZE] = {0};
mtx_t lock;

static void main_usage(FILE *) __attribute__((noreturn));
static void main_launch_daemon(void);

int main(int argc, char *argv[]) {
    int32 id;
    bool spell = false;

    signal(SIGSEGV, util_segv_handler);
    signal(SIGINT, util_int_handler);
    signal(SIGTERM, util_int_handler);

    if (argc <= 1 || argc >= 4)
        main_usage(stderr);

    for (uint i = 0; i < ARRAY_LENGTH(commands); i += 1) {
        if (!strcmp(argv[1], commands[i].shortname)
            || !strcmp(argv[1], commands[i].longname)) {
            spell = true;
            switch (i) {
            case PRINT:
                ipc_client_speak_fifo(PRINT, 0);
                break;
            case INFO:
            case COPY:
            case REMOVE:
                if (argc != 3 || !util_string_int32(&id, argv[2], 10))
                    main_usage(stderr);
                ipc_client_speak_fifo(i, id);
                break;
            case SAVE:
                ipc_client_speak_fifo(SAVE, 0);
                break;
            case DAEMON:
                main_launch_daemon();
                break;
            case HELP:
                main_usage(stdout);
            default:
                main_usage(stderr);
            }
        }
    }

    if (!spell)
        main_usage(stderr);

    return 0;
}

void main_usage(FILE *stream) {
    DEBUG_PRINT("usage(%p)\n", (void *) stream)
    fprintf(stream, "usage: %s COMMAND [n]\n", "clipsim");
    fprintf(stream, "Available commands:\n");
    for (uint i = 0; i < ARRAY_LENGTH(commands); i += 1) {
        fprintf(stream, "%s | %-*s : %s\n",
                commands[i].shortname, 8, commands[i].longname, 
                commands[i].description);
    }
    exit(stream != stdout);
}

void main_launch_daemon(void) {
    DEBUG_PRINT("launch_daemon(void) %d\n", __LINE__);
    thrd_t ipc_thread;
    thrd_t clipboard_thread;
    int ipc_error = 0;
    int clipboard_error = 0;
    int e;

    if ((e = mtx_init(&lock, mtx_plain)) != thrd_success) {
        fprintf(stderr, "mtx_init() failed: %s\n", strerror(e));
        return;
    }

    history_read();

    ipc_error = thrd_create(&ipc_thread, ipc_daemon_listen_fifo, NULL);
    clipboard_error = thrd_create(&clipboard_thread, clipboard_daemon_watch, NULL);

    if (ipc_error != thrd_success) {
        fprintf(stderr, "Error on IPC thread: %s\n",
                         strerror(ipc_error));
        exit(EXIT_FAILURE);
    } else if (clipboard_error != thrd_success) {
        fprintf(stderr, "Error on clipboard thread: %s\n",
                strerror(clipboard_error));
        exit(EXIT_FAILURE);
        return;
    }

    thrd_join(ipc_thread, NULL);
    thrd_join(clipboard_thread, NULL);
    mtx_destroy(&lock);
    return;
}
