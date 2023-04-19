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

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>

#include "send_signal.h"

static pid_t check_pid(char *, char*);

void send_signal(char *progname, int signum) {
    DIR *proc_dir;
    struct dirent *proc_dirent;
    pid_t pid;

    if (!(proc_dir = opendir("/proc"))) {
        fprintf(stderr, "Error opening /proc: %s\n", strerror(errno));
        return;
    }

    while ((proc_dirent = readdir(proc_dir))) {
        if ((pid = check_pid(progname, proc_dirent->d_name))) {
            kill(pid, SIGRTMIN+signum);
            break;
        }
    }

    closedir(proc_dir);
    return;
}

pid_t check_pid(char *progname, char *name) {
    int pid;
    static char pid_stat[256];
    static char buffer[256];
    char *pbuf;
    FILE *stat_file;

    if ((pid = atoi(name)) <= 0)
        return 0;

    snprintf(pid_stat, sizeof(pid_stat), "/proc/%s/stat", name);
    pid_stat[255] = '\0';
    if (!(stat_file = fopen(pid_stat, "r"))) {
        fprintf(stderr, "Error opening %s: %s\n", pid_stat, strerror(errno));
        return 0;
    }
    if (!(pbuf = fgets(buffer, sizeof(buffer), stat_file))) {
        fprintf(stderr, "Error reading %s: %s\n", pid_stat, strerror(errno));
        goto close;
    }
    if (!strtok(buffer, "(") || !(pbuf = strtok(NULL, ")"))) {
        fprintf(stderr, "Stat file for pid %d misses "
                        "command name between parenthesis: %s\n",
                        pid, buffer);
        goto close;
    }
    if (!strcmp(pbuf, progname)) {
        fclose(stat_file);
        return pid;
    }

    close:
    fclose(stat_file);
    return 0;
}

