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

#ifdef __linux__
static pid_t check_pid(const char *, const char *);

pid_t
check_pid(const char *executable, const char *number) {
    static char buffer[256];
    static char command[256];
    int pid;
    int cmdline;
	int n;

    if ((pid = atoi(number)) <= 0)
        return 0;

    n = snprintf(buffer, sizeof(buffer), "/proc/%s/cmdline", number);
	if (n < 0) {
		error("Error printing buffer name.\n");
		return 0;
	}
    buffer[sizeof(buffer) - 1] = '\0';

    if ((cmdline = open(buffer, O_RDONLY)) < 0)
        return 0;

    if (read(cmdline, command, sizeof(command)) <= 0) {
        close(cmdline);
        return 0;
    }
    close(cmdline);

    command[strcspn(buffer, "\n")] = '\0';
    if (!strcmp(command, executable))
        return pid;

    return 0;
}

void
send_signal(const char *executable, const int signal_number) {
    DIR *processes;
    struct dirent *process;
    pid_t pid;

    if ((processes = opendir("/proc")) == NULL) {
        error("Error opening /proc: %s\n", strerror(errno));
        return;
    }

    while ((process = readdir(processes))) {
        if (process->d_type != DT_DIR)
            continue;
        if ((pid = check_pid(executable, process->d_name))) {
            kill(pid, signal_number);
            break;
        }
    }

    closedir(processes);
    return;
}
#else
void
send_signal(const char *executable, const int signal_number) {
    char signal_string[14];
    snprintf(signal_string, sizeof(signal_string), "%d", signal_number);

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
