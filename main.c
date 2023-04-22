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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

#include "clipboard.h"
#include "clipsim.h"
#include "ipc.h"
#include "history.h"
#include "util.h"

char *progname;
Entry entries[HISTORY_BUFFER_SIZE] = {0};
pthread_mutex_t lock;

static void usage(FILE *) __attribute__((noreturn));
static void launch_daemon(void);

int main(int argc, char *argv[]) {
    int32 id;
    progname = argv[0];

    signal(SIGSEGV, segv_handler);
    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);

    if (argc <= 1 || argc >= 4)
        usage(stderr);

    for (int c = PRINT; c <= HELP; c += 1) {
        if (!strcmp(argv[1], commands[c])) {
            switch (c) {
            case PRINT:
                ipc_client_speak_fifo(PRINT, 0);
                break;
            case INFO:
            case COPY:
            case DELETE:
                if (argc != 3 || !estrtol(&id, argv[2], 10))
                    usage(stderr);
                ipc_client_speak_fifo(c, id);
                break;
            case SAVE:
                ipc_client_speak_fifo(SAVE, 0);
                break;
            case DAEMON:
                launch_daemon();
                break;
            case HELP:
                usage(stdout);
            default:
                usage(stderr);
            }
        }
    }

    return 0;
}

void usage(FILE *stream) {
    DEBUG_PRINT("usage(%p)\n", (void *) stream)
    fprintf(stream,
            "usage: %s COMMAND [n]\n"
            "Available commands:\n"
            "    daemon : spawn daemon\n"
            "     print : print history\n"
            "  info <n> : print entry number <n>\n"
            "  copy <n> : copy entry number <n> to clipboard\n"
            "delete <n> : delete entry number <n> from history\n"
            "      save : save history to $XDG_CACHE_HOME/clipsim/history\n"
            "      help : print this help message to stdout\n", progname);
    exit(stream != stdout);
}

void launch_daemon(void) {
    DEBUG_PRINT("launch_daemon(void) %d\n", __LINE__)
    pthread_t ipc_thread;
    pthread_t clipboard_thread;
    int ipc_error = 0;
    int clipboard_error = 0;
    int e;

    if ((e = pthread_mutex_init(&lock, NULL)) != 0) {
        fprintf(stderr, "pthread_mutex_init() failed: %s\n", strerror(e));
        return;
    }

    history_read();

    ipc_error = pthread_create(&ipc_thread, NULL,
                               ipc_daemon_listen_fifo, NULL);
    clipboard_error = pthread_create(&clipboard_thread, NULL, 
                                     clipboard_daemon_watch, NULL);
    if (ipc_error) {
        fprintf(stderr, "Error on IPC thread: %s\n",
                         strerror(clipboard_error));
        pthread_cancel(ipc_thread);
        pthread_cancel(clipboard_thread);
        return;
    } else if (clipboard_error) {
        fprintf(stderr, "Error on clipboard thread: %s\n",
                strerror(clipboard_error));
        pthread_cancel(ipc_thread);
        pthread_cancel(clipboard_thread);
        return;
    }
    pthread_join(ipc_thread, NULL);
    pthread_join(clipboard_thread, NULL);
    pthread_mutex_destroy(&lock);
    return;
}
