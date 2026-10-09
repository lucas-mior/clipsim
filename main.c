// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#define CBASE_IMPLEMENT
#include "cbase.h"

#include "clipsim.h"

#if DEBUGGING
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"
#endif

#include "clipsim.c"
#include "history.c"
#include "ipc.c"
#include "clipboard.c"
#include "xi.c"

typedef struct ClipsimCommand {
    char *shortname;
    char *longname;
    char *description;

    int32 shortname_len;
    int32 longname_len;
    int32 description_len;
} ClipsimCommand;

static ClipsimCommand commands[] = {
    [CMD_PRINT] = {
        STRPASS(shortname, "-p"),
        STRPASS(longname, "--print"),
        STRPASS(description, "print entire history, with trimmed whitespace"),
    },
    [CMD_INFO] = {
        STRPASS(shortname, "-i"),
        STRPASS(longname, "--info"),
        STRPASS(description,
                "print entry number <n>, with original whitespace"),
    },
    [CMD_COPY] = {
        STRPASS(shortname, "-c"),
        STRPASS(longname, "--copy"),
        STRPASS(description,
                "copy entry number <n>, with original whitespace"),
    },
    [CMD_REMOVE] = {
        STRPASS(shortname, "-r"),
        STRPASS(longname, "--remove"),
        STRPASS(description, "remove entry number <n>"),
    },
    [CMD_SAVE] = {
        STRPASS(shortname, "-s"),
        STRPASS(longname, "--save"),
        STRPASS(description, "save history to $XDG_CACHE_HOME/clipsim/history"),
    },
    [CMD_DAEMON] = {
        STRPASS(shortname, "-d"),
        STRPASS(longname, "--daemon"),
        STRPASS(description,
                "spawn daemon (clipboard watcher and command socket)"),
    },
    [CMD_HELP] = {
        STRPASS(shortname, "-h"),
        STRPASS(longname, "--help"),
        STRPASS(description, "print this help message"),
    },
};

static void main_set_signal(int32, void (*)(int));
static void main_setup_daemon_signals(void);
static bool main_block_middle_mouse_paste_enabled(void);
static noreturn void main_usage(FILE *);
static noreturn void main_launch_daemon(void);

int32
main(int32 argc, char *argv[]) {
    DEBUG_PRINT("%d, %s", argc, argv[0])
    int64 id;
    int32 command_len;
    int32 id_len;
    bool spell_error = true;

    program = basename(argv[0]);

    signal(SIGSEGV, util_segv_handler);

    if (argc <= 1 || argc >= 4) {
        main_usage(stderr);
    }

    command_len = strlen32(argv[1]);
    for (int32 i = 0; i < LENGTH(commands); i += 1) {
        if (STREQUAL(argv[1], command_len,
                     commands[i].shortname, commands[i].shortname_len)
            || STREQUAL(argv[1], command_len,
                        commands[i].longname, commands[i].longname_len)) {
            spell_error = false;
            switch (i) {
            case CMD_PRINT:
                ipc_client_speak(CMD_PRINT, 0);
                break;
            case CMD_INFO:
            case CMD_COPY:
            case CMD_REMOVE:
                if (argc != 3) {
                    main_usage(stderr);
                }
                id_len = strlen32(argv[2]);
                if ((parse_integer(argv[2], id_len, &id)) < 0) {
                    main_usage(stderr);
                }
                if ((id <= INT32_MIN) || (id >= INT32_MAX)) {
                    main_usage(stderr);
                }
                ipc_client_speak(i, (int32)id);
                break;
            case CMD_SAVE:
                ipc_client_speak(CMD_SAVE, 0);
                break;
            case CMD_DAEMON:
                main_launch_daemon();
            case CMD_HELP:
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
    int32 value_len;

    GETENV(CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE);

    if (CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE == NULL) {
        error("Primary selection will not be cleared"
              " When pressing the middle mouse button.\n");
        return false;
    }

    value_len = strlen32(CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE);
    if (STREQUAL(CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE, value_len, "0")) {
        return false;
    }
    if (STREQUAL(CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE, value_len, "false")) {
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
        fprintf(stream,
                "%s | %-*s : %s\n",
                commands[i].shortname, 8, commands[i].longname,
                commands[i].description);
    }
    exit(stream != stdout);
}



void
main_launch_daemon(void) {
    DEBUG_PRINT("%s", "void")
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
