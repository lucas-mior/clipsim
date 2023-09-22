/* This file is part of clipsim.
 * Copyright (C) 2022 Lucas Mior

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

Entry entries[HISTORY_BUFFER_SIZE] = {0};
mtx_t lock;

static void main_usage(FILE *) __attribute__((noreturn));
static void main_launch_daemon(void);

int main(int argc, char *argv[]) {
    DEBUG_PRINT("%d, %s", argc, argv[0]);
    int32 id;
    bool spell_error = true;

    signal(SIGSEGV, util_segv_handler);

    if (argc <= 1 || argc >= 4)
        main_usage(stderr);

    for (uint i = 0; i < ARRAY_LENGTH(commands); i += 1) {
        if (!strcmp(argv[1], commands[i].shortname)
            || !strcmp(argv[1], commands[i].longname)) {
            spell_error = false;
            switch (i) {
            case COMMAND_PRINT:
                ipc_client_speak_fifo(COMMAND_PRINT, 0);
                break;
            case COMMAND_INFO:
            case COMMAND_COPY:
            case COMMAND_REMOVE:
                if ((argc != 3) || util_string_int32(&id, argv[2]) < 0)
                    main_usage(stderr);
                ipc_client_speak_fifo(i, id);
                break;
            case COMMAND_SAVE:
                ipc_client_speak_fifo(COMMAND_SAVE, 0);
                break;
            case COMMAND_DAEMON:
                main_launch_daemon();
                break;
            case COMMAND_HELP:
                main_usage(stdout);
            default:
                main_usage(stderr);
            }
        }
    }

    if (spell_error)
        main_usage(stderr);

    return 0;
}

void main_usage(FILE *stream) {
    DEBUG_PRINT("%p", (void *) stream);
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
    DEBUG_PRINT("");
    thrd_t ipc_thread;
    thrd_t clipboard_thread;
    int ipc_error = 0;
    int clipboard_error = 0;
    int error;

    // TODO: Check if clipsim -d | --daemon is already running. If so, quit
    if ((error = mtx_init(&lock, mtx_plain)) != thrd_success) {
        fprintf(stderr, "Error initializing lock: %s\n", strerror(error));
        exit(EXIT_FAILURE);
    }

    history_read();

    ipc_error = thrd_create(&ipc_thread, ipc_daemon_listen_fifo, NULL);
    clipboard_error = thrd_create(&clipboard_thread,
                                  clipboard_daemon_watch, NULL);

    if (ipc_error != thrd_success) {
        fprintf(stderr, "Error on IPC thread: %s\n",
                         strerror(ipc_error));
        exit(EXIT_FAILURE);
    }
    if (clipboard_error != thrd_success) {
        fprintf(stderr, "Error on clipboard thread: %s\n",
                strerror(clipboard_error));
        exit(EXIT_FAILURE);
    }

    thrd_join(ipc_thread, NULL);
    thrd_join(clipboard_thread, NULL);
    mtx_destroy(&lock);
    return;
}
