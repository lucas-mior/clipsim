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
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "clipsim.h"
#include "config.h"
#include "util.h"

void *emalloc(size_t size) {
    char *temp;
    if (!(temp = malloc(size))) {
        fprintf(stderr, "Failed to allocate memory. Exiting.\n");
        exit(EXIT_FAILURE);
    } else {
        return temp;
    }
}

void *erealloc(char *ptr, size_t size) {
    void *temp;
    if (!(temp = realloc(ptr, size))) {
        fprintf(stderr, "Failed to reallocate more memory. "
                        "Exiting.\n");
        exit(EXIT_FAILURE);
    } else {
        return temp;
    }
}

bool estrtol(int *num, char *string, int base) {
    char *pend;
    long x;
    errno = 0;
    x = strtol(string, &pend, base);
    if ((x > INT_MAX) || (x < INT_MIN))
        return false;
    *num = (int) x;
    return (errno == 0) && (string != pend) && (*pend == 0);
}

void segv_handler(int unused) {
    char *msg = "Memory error. Please send a bug report.\n";
    (void) unused;

    write(2, msg, strlen(msg));
    if (!access("/usr/bin/dunstify", X_OK)) {
        execl("/usr/bin/dunstify", "dunstify", "-u", "critical",
                                   progname, msg, NULL);
    }
    exit(1);
}
