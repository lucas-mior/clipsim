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

#include "clip.h"
#include "clipsim.h"
#include "comm.h"
#include "config.h"
#include "hist.h"
#include "util.h"

char *progname;
Entry *last_entry;
pthread_mutex_t lock;

static void usage(FILE *stream);
static void launch_daemon(void);

int main(int argc, char *argv[]) {
    int32 id;
    progname = argv[0];

    signal(SIGSEGV, segv_handler);

    if (argc <= 1 || argc >= 4) {
        usage(stderr);
        return 1;
    }

    if (!strcmp(argv[1], "print")) {
        client_speak_fifo(PRINT, 0);
    } else if (!strcmp(argv[1], "info") &&
               (argc == 3) && estrtol(&id, argv[2], 10)) {
        client_speak_fifo(INFO, id);
    } else if (!strcmp(argv[1], "copy") &&
               (argc == 3) && estrtol(&id, argv[2], 10)) {
        client_speak_fifo(COPY, id);
    } else if (!strcmp(argv[1], "delete") &&
               (argc == 3) && estrtol(&id, argv[2], 10)) {
        client_speak_fifo(DELETE, id);
    } else if (!strcmp(argv[1], "save")) {
        client_speak_fifo(SAVE, 0);
    } else if (!strcmp(argv[1], "daemon")) {
        launch_daemon();
    } else if (!strcmp(argv[1], "help")) {
        usage(stdout);
        return 0;
    } else {
        usage(stderr);
        return 1;
    }

    return 0;
}

static void usage(FILE *stream) {
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
    return;
}

static void launch_daemon(void) {
    pthread_t fifo_thread;
    pthread_t clip_thread;
    int err_fifo = 0;
    int err_clip = 0;

    if (pthread_mutex_init(&lock, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init() failed.\n");
        return;
    }

    last_entry = emalloc(sizeof(Entry));

    last_entry->id = 0;
    last_entry->len = 0;
    last_entry->olen = 0;
    last_entry->out = NULL;
    last_entry->data = NULL;
    last_entry->next = NULL;
    last_entry->prev = NULL;

    hist_read();

    err_fifo = pthread_create(&fifo_thread, NULL, daemon_listen_fifo, NULL);
    err_clip = pthread_create(&clip_thread, NULL, daemon_watch_clip, NULL);
    if (err_fifo) {
        fprintf(stderr, "Error on fifo thread: %s\n", strerror(err_clip));
        pthread_cancel(fifo_thread);
        pthread_cancel(clip_thread);
        return;
    } else if (err_clip) {
        fprintf(stderr, "Error on clip thread: %s\n", strerror(err_clip));
        pthread_cancel(fifo_thread);
        pthread_cancel(clip_thread);
        return;
    }
    pthread_join(fifo_thread, NULL);
    pthread_join(clip_thread, NULL);
    pthread_mutex_destroy(&lock);
    return;
}
