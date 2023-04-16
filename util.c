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

void *ealloc(void *old, size_t size) {
    void *p;
    if ((p = realloc(old, size)) == NULL) {
        fprintf(stderr, "Failed to allocate %zu bytes.\n", size);
        exit(1);
    }
    return p;
}

void *ecalloc(size_t nmemb, size_t size) {
    void *p;
    if ((p = calloc(nmemb, size)) == NULL) {
        fprintf(stderr, "Failed to allocate %zu members of %zu bytes each.\n", 
                        nmemb, size);
        exit(1);
    }
    return p;
}

bool estrtol(int *num, char *string, int base) {
    char *pend;
    long x;
    errno = 0;
    x = strtol(string, &pend, base);
    if ((errno != 0) || (string == pend) || (*pend != 0)) {
        return false;
    } else if ((x > INT_MAX) || (x < INT_MIN)) {
        return false;
    } else {
        *num = (int) x;
        return true;
    }
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
