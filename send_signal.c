/*
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

#include "util.c"

static void send_signal(const char *, const int);

#ifdef __linux__
#include <dirent.h>
void
send_signal(const char *executable, const int32 signal_number) {
    DIR *processes;
    struct dirent *process;

    if ((processes = opendir("/proc")) == NULL) {
        error("Error opening /proc: %s\n", strerror(errno));
        return;
    }

    while ((process = readdir(processes))) {
        static char buffer[256];
        static char command[256];
        int32 pid;
        int32 cmdline;

        if (process->d_type != DT_DIR)
            continue;
        if ((pid = atoi(process->d_name)) <= 0)
            continue;

        SNPRINTF(buffer, "/proc/%s/cmdline", process->d_name);

        if ((cmdline = open(buffer, O_RDONLY)) < 0)
            continue;

        if (read(cmdline, command, sizeof(command)) <= 0) {
            close(cmdline);
            continue;
        }
        if (!strcmp(command, executable)) {
            if (kill(pid, signal_number) < 0) {
                error("Error sending signal %d to program %s (pid %d): %s.\n",
                      signal_number, executable, pid, strerror(errno));
            }
        }

        close(cmdline);
    }

    closedir(processes);
    return;
}
#else
void
send_signal(const char *executable, const int32 signal_number) {
    char signal_string[14];
    int32 n;
    SNPRINTF(signal_string, "%d", signal_number);

    switch (fork()) {
        case -1:
            error("Error forking: %s\n", strerror(errno));
            return;
        case 0:
            execlp("pkill", "pkill", signal_string, executable, NULL);
            error("Error executing pkill: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        default:
            wait(NULL);
    }
    return;
}
#endif
