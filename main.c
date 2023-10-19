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

static bool main_check_cmdline(char *);
static bool main_check_running(void);
static void main_usage(FILE *) __attribute__((noreturn));
static void main_launch_daemon(void);

int main(int argc, char *argv[]) {
    DEBUG_PRINT("%d, %s", argc, argv[0]);
    int32 id;
    bool spell_error = true;

    signal(SIGSEGV, util_segv_handler);

    if (argc <= 1 || argc >= 4)
        main_usage(stderr);

    for (uint i = 0; i < LENGTH(commands); i += 1) {
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

    exit(EXIT_SUCCESS);
}

void main_usage(FILE *stream) {
    DEBUG_PRINT("%p", (void *) stream);
    fprintf(stream, "usage: %s COMMAND [n]\n", "clipsim");
    fprintf(stream, "Available commands:\n");
    for (uint i = 0; i < LENGTH(commands); i += 1) {
        fprintf(stream, "%s | %-*s : %s\n",
                commands[i].shortname, 8, commands[i].longname, 
                commands[i].description);
    }
    exit(stream != stdout);
}

bool main_check_cmdline(char *pid) {
    char buffer[256];
    char command[256];
    int n;
    usize r;
    FILE *cmdline;
    char cmd1[] = {'c', 'l', 'i', 'p', 's', 'i', 'm', '\0',
                   '-', 'd', '\0'};
    char cmd2[] = {'c', 'l', 'i', 'p', 's', 'i', 'm', '\0',
                   '-', '-', 'd', 'a', 'e', 'm', 'o', 'n', '\0'};

    n = snprintf(buffer, sizeof (buffer), "/proc/%s/cmdline", pid);
    if (n < 0) {
        fprintf(stderr, "Error printing buffer name.\n");
        return false;
    }
    buffer[sizeof (buffer) - 1] = '\0';

    if ((cmdline = fopen(buffer, "r")) == NULL)
        return false;
    if ((r = fread(command, 1, sizeof (command), cmdline)) == 0) {
        fclose(cmdline);
        return false;
    }

    if (r == sizeof (cmd1)) {
        if (!memcmp(command, cmd1, r))
            return true;
    } else if (r == sizeof (cmd2)) {
        if (!memcmp(command, cmd2, r))
            return true;
    }
    return false;
}

bool main_check_running(void) {
    DEBUG_PRINT("void");
    DIR *processes;
    struct dirent *program;
    pid_t pid_this = getpid();

    if ((processes = opendir("/proc")) == NULL) {
        fprintf(stderr, "Error opening /proc: %s\n", strerror(errno));
        return false;
    }

    while ((program = readdir(processes))) {
        pid_t pid;
        if ((pid = atoi(program->d_name)) <= 0)
            continue;

        if (pid == pid_this)
            continue;

        if (main_check_cmdline(program->d_name)) {
            closedir(processes);
            return true;
        }
    }
    closedir(processes);
    return false;
}

void main_launch_daemon(void) {
    DEBUG_PRINT("");
    thrd_t ipc_thread;
    int error;

    if (main_check_running()) {
        fprintf(stderr, "clipsim --daemon is already running.\n");
        exit(EXIT_FAILURE);
    }

    if ((error = mtx_init(&lock, mtx_plain)) != thrd_success) {
        fprintf(stderr, "Error initializing lock: %s\n", strerror(error));
        exit(EXIT_FAILURE);
    }

    history_read();

    thrd_create(&ipc_thread, ipc_daemon_listen_fifo, NULL);
    clipboard_daemon_watch(NULL);

    return;
}
