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

#include <sys/stat.h>
#include "clipsim.h"

static pid_t check_pid(const char *, const char*);

void send_signal(const char *executable, const int signal_number) {
    DIR *processes;
    struct dirent *program;
    pid_t pid;

    if (!(processes = opendir("/proc"))) {
        fprintf(stderr, "Error opening /proc: %s\n", strerror(errno));
        return;
    }

    while ((program = readdir(processes))) {
        if (program->d_type != DT_DIR)
            continue;
        if ((pid = check_pid(executable, program->d_name))) {
            kill(pid, SIGRTMIN+signal_number);
            break;
        }
    }

    closedir(processes);
    return;
}

pid_t check_pid(const char *executable, const char *number) {
    static char buffer[256];
    static char command[256];
    int pid;
    FILE *cmdline;

    if ((pid = atoi(number)) <= 0)
        return 0;

    snprintf(buffer, sizeof(buffer), "/proc/%s/cmdline", number);
    buffer[sizeof(buffer)-1] = '\0';
    if (!(cmdline = fopen(buffer, "r")))
        return 0;
    if (!fgets(command, sizeof(command), cmdline)) {
        fclose(cmdline);
        return 0;
    }
    command[strcspn(buffer, "\n")] = '\0';
    if (!strcmp(command, executable)) {
        fclose(cmdline);
        return pid;
    }

    fclose(cmdline);
    return 0;
}
