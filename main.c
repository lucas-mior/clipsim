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

typedef struct Command {
    const char *shortname;
    const char *longname;
    const char *description;
} Command;

static const Command commands[] = {
    [COMMAND_PRINT]  = {"-p", "--print",
                        "print entire history, with trimmed whitespace" },
    [COMMAND_INFO]   = {"-i", "--info",
                        "print entry number <n>, with original whitespace" },
    [COMMAND_COPY]   = {"-c", "--copy",
                        "copy entry number <n>, with original whitespace" },
    [COMMAND_REMOVE] = {"-r", "--remove",
                        "remove entry number <n>" },
    [COMMAND_SAVE]   = {"-s", "--save",
                        "save history to $XDG_CACHE_HOME/clipsim/history" },
    [COMMAND_DAEMON] = {"-d", "--daemon",
                        "spawn daemon (clipboard watcher and command fifo)" },
    [COMMAND_HELP]   = {"-h", "--help",
                        "print this help message" },
};

Entry entries[HISTORY_BUFFER_SIZE] = {0};
bool is_image[HISTORY_BUFFER_SIZE] = {0};
const char TEXT_TAG = (char)0x01;
const char IMAGE_TAG = (char)0x02;
mtx_t lock;
char *program;
magic_t magic;

static bool main_check_cmdline(char *);
static bool main_check_running(void);
static void main_usage(FILE *) __attribute__((noreturn));
static void main_launch_daemon(void) __attribute__((noreturn));

int32
main(int32 argc, char *argv[]) {
    DEBUG_PRINT("%d, %s", argc, argv[0]);
    int32 id;
    bool spell_error = true;

    program = basename(argv[0]);

    signal(SIGSEGV, util_segv_handler);
    signal(SIGTERM, history_exit);
    signal(SIGINT, history_exit);

    if (argc <= 1 || argc >= 4)
        main_usage(stderr);

    for (int32 i = 0; i < LENGTH(commands); i += 1) {
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
            case COMMAND_HELP:
                main_usage(stdout);
            default:
                main_usage(stderr);
            }
        }
    }

    if (spell_error)
        main_usage(stderr);

    exit(EXIT_SUCCESS);
}

void
main_usage(FILE *stream) {
    DEBUG_PRINT("%p", (void *)stream);
    fprintf(stream, "usage: %s COMMAND [n]\n", "clipsim");
    fprintf(stream, "Available commands:\n");
    for (int32 i = 0; i < LENGTH(commands); i += 1) {
        fprintf(stream, "%s | %-*s : %s\n",
                commands[i].shortname, 8, commands[i].longname,
                commands[i].description);
    }
    exit(stream != stdout);
}

bool
main_check_cmdline(char *pid) {
    char buffer[256];
    char command[256];
    int32 n;
    isize r;
    int32 cmdline;
    char cmd1[] = {'c', 'l', 'i', 'p', 's', 'i', 'm', '\0',
                   '-', 'd', '\0'};
    char cmd2[] = {'c', 'l', 'i', 'p', 's', 'i', 'm', '\0',
                   '-', '-', 'd', 'a', 'e', 'm', 'o', 'n', '\0'};

    n = snprintf(buffer, sizeof(buffer), "/proc/%s/cmdline", pid);
    if (n < 0) {
        error("Error printing buffer name.\n");
        return false;
    }
    buffer[sizeof(buffer) - 1] = '\0';

    if ((cmdline = open(buffer, O_RDONLY)) < 0)
        return false;
    if ((r = read(cmdline, command, sizeof(command))) <= 0) {
        close(cmdline);
        return false;
    }
    close(cmdline);

    switch (r) {
    case sizeof(cmd1):
        if (!memcmp(command, cmd1, (usize)r))
            return true;
        break;
    case sizeof(cmd2):
        if (!memcmp(command, cmd2, (usize)r))
            return true;
        break;
    default:
        break;
    }
    return false;
}

bool
main_check_running(void) {
    DEBUG_PRINT("void");
    DIR *processes;
    struct dirent *process;
    pid_t pid_this = getpid();

    if ((processes = opendir("/proc")) == NULL) {
        error("Error opening /proc: %s\n", strerror(errno));
        return false;
    }

    while ((process = readdir(processes))) {
        pid_t pid;
        if ((pid = atoi(process->d_name)) <= 0)
            continue;

        if (pid == pid_this)
            continue;

        if (main_check_cmdline(process->d_name)) {
            closedir(processes);
            return true;
        }
    }
    closedir(processes);
    return false;
}

void
main_launch_daemon(void) {
    DEBUG_PRINT("void");
    thrd_t ipc_thread;
    thrd_t xi_thread;
    int32 mtx_error;
    char *CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE;

    if (main_check_running()) {
        error("clipsim --daemon is already running.\n");
        exit(EXIT_FAILURE);
    }

    if ((mtx_error = mtx_init(&lock, mtx_plain)) != thrd_success) {
        error("Error initializing lock: %s\n", strerror(mtx_error));
        exit(EXIT_FAILURE);
    }

    history_read();

    if ((magic = magic_open(MAGIC_MIME_TYPE)) == NULL) {
        error("Error in magic_open(MAGIC_MIME_TYPE): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (magic_load(magic, NULL) != 0) {
        error("Error in magic_load(): %s\n", magic_error(magic));
        exit(EXIT_FAILURE);
    }

    thrd_create(&ipc_thread, ipc_daemon_listen_fifo, NULL);

    if ((CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE
          = getenv("CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE")) == NULL) {
        error("CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE is not defined.\n");
    } else {
        if (!strcmp(CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE, "0")) {
            CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE = NULL;
        } else if (!strcmp(CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE, "false")) {
            CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE = NULL;
        }
    }

    if (CLIPSIM_BLOCK_MIDDLE_MOUSE_PASTE)
        thrd_create(&xi_thread, xi_daemon_loop, NULL);
    clipboard_daemon_watch();
}
