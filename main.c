// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#include "clipsim.h"

#define CBASE_IMPLEMENT
#include "cbase.h"

#include "clipsim.c"
#include "history.c"
#include "ipc.c"
#include "clipboard.c"
#include "xi.c"

typedef struct ClipsimCommand {
    char *shortname;
    char *longname;
    char *description;
} ClipsimCommand;

static ClipsimCommand commands[] = {
    [COMMAND_PRINT]  = {"-p", "--print",  "print entire history, with trimmed whitespace"},
    [COMMAND_INFO]   = {"-i", "--info",   "print entry number <n>, with original whitespace"},
    [COMMAND_COPY]   = {"-c", "--copy",   "copy entry number <n>, with original whitespace"},
    [COMMAND_REMOVE] = {"-r", "--remove", "remove entry number <n>"},
    [COMMAND_SAVE]   = {"-s", "--save",   "save history to $XDG_CACHE_HOME/clipsim/history"},
    [COMMAND_DAEMON] = {"-d", "--daemon", "spawn daemon (clipboard watcher and command socket)"},
    [COMMAND_HELP]   = {"-h", "--help",   "print this help message"},
};

static void main_set_signal(int32, void (*)(int));
static void main_setup_daemon_signals(void);
static bool main_block_middle_mouse_paste_enabled(void);
static noreturn void main_usage(FILE *);
static noreturn void main_launch_daemon(void);

int32
main(int32 argc, char *argv[]) {
    DEBUG_PRINT("%d, %s", argc, argv[0])
    int32 id;
    bool spell_error = true;

    program = basename(argv[0]);

    signal(SIGSEGV, util_segv_handler);

    if (argc <= 1 || argc >= 4) {
        main_usage(stderr);
    }

    for (int32 i = 0; i < LENGTH(commands); i += 1) {
        if (strequal(argv[1], commands[i].shortname)
            || strequal(argv[1], commands[i].longname)) {
            spell_error = false;
            switch (i) {
            case COMMAND_PRINT:
                ipc_client_speak(COMMAND_PRINT, 0);
                break;
            case COMMAND_INFO:
            case COMMAND_COPY:
            case COMMAND_REMOVE:
                if ((argc != 3) || util_string_int32(&id, argv[2]) < 0) {
                    main_usage(stderr);
                }
                ipc_client_speak(i, id);
                break;
            case COMMAND_SAVE:
                ipc_client_speak(COMMAND_SAVE, 0);
                break;
            case COMMAND_DAEMON:
                main_launch_daemon();
            case COMMAND_HELP:
                main_usage(stdout);
            default:
                main_usage(stderr);
            }
        }
    }

    if (spell_error) {
        main_usage(stderr);
    }

    exit(EXIT_SUCCESS);
}

void
main_set_signal(int32 signum, void (*handler)(int)) {
    if (signal(signum, handler) == SIG_ERR) {
        error("Error installing signal handler for %d: %s.\n",
              signum, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return;
}

void
main_setup_daemon_signals(void) {
    main_set_signal(SIGTERM, history_exit);
    main_set_signal(SIGINT, history_exit);
    main_set_signal(SIGPIPE, SIG_IGN);
    return;
}

bool
main_block_middle_mouse_paste_enabled(void) {
    char *CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE;

    GETENV(CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE);

    if (CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE == NULL) {
        error("Primary selection will not be cleared"
              " When pressing the middle mouse button.\n");
        return false;
    }

    if (strequal(CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE, "0")) {
        return false;
    }
    if (strequal(CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE, "false")) {
        return false;
    }

    return true;
}

void
main_usage(FILE *stream) {
    DEBUG_PRINT("%p", (void *)stream)
    fprintf(stream, "usage: %s COMMAND [n]\n", "clipsim");
    fprintf(stream, "Available commands:\n");
    for (int32 i = 0; i < LENGTH(commands); i += 1) {
        fprintf(stream, "%s | %-*s : %s\n", commands[i].shortname, 8,
                commands[i].longname, commands[i].description);
    }
    exit(stream != stdout);
}



void
main_launch_daemon(void) {
    DEBUG_PRINT("void")
    pthread_t ipc_thread;
    bool block_middle_mouse_paste;

    ipc_lock_daemon();

    main_setup_daemon_signals();

    block_middle_mouse_paste = main_block_middle_mouse_paste_enabled();
    if (block_middle_mouse_paste && !XInitThreads()) {
        error("Error initializing Xlib thread support.\n");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_init(&clipsim_lock, NULL);

    history_read();

    reopen_magic();

    pthread_create(&ipc_thread, NULL, ipc_daemon_listen, NULL);

    if (block_middle_mouse_paste) {
        pthread_t xi_thread;
        pthread_create(&xi_thread, NULL, xi_daemon_loop, NULL);
    }
    clipboard_daemon_watch();
}
