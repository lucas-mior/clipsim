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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "clipsim.h"
#include "util.h"
#include "history.h"

void *util_realloc(void *old, size_t size) {
    DEBUG_PRINT("*util_realloc(%p, %zu)\n", old, size)
    void *p;
    if ((p = realloc(old, size)) == NULL) {
        fprintf(stderr, "Failed to allocate %zu bytes.\n", size);
        if (old)
            fprintf(stderr, "Reallocating from: %p\n", old);
        exit(EXIT_FAILURE);
    }
    return p;
}

void *util_calloc(size_t nmemb, size_t size) {
    DEBUG_PRINT("*xcalloc(%zu, %zu)\n", nmemb, size)
    void *p;
    if ((p = calloc(nmemb, size)) == NULL) {
        fprintf(stderr, "Failed to allocate %zu members of %zu bytes each.\n",
                        nmemb, size);
        exit(EXIT_FAILURE);
    }
    return p;
}

bool util_strtol(int32 *num, char *string, int base) {
    char *pend;
    long x;
    errno = 0;
    x = strtol(string, &pend, base);
    if ((errno != 0) || (string == pend) || (*pend != 0)) {
        return false;
    } else if ((x > INT32_MAX) || (x < INT32_MIN)) {
        return false;
    } else {
        *num = (int32) x;
        return true;
    }
}

void util_segv_handler(int unused) {
    char *message = "Memory error. Please send a bug report.\n";
    char *notifiers[2] = { "dunstify", "notify-send" };
    (void) unused;

    write(STDERR_FILENO, message, strlen(message));
    for (uint i = 0; i < ARRLEN(notifiers); i += 1) {
        execlp(notifiers[i], notifiers[i], "-u", "critical", 
               "clipsim", message, NULL);
    }
    exit(EXIT_FAILURE);
}

void util_int_handler(int unused) {
    (void) unused;
    history_save();
    exit(EXIT_FAILURE);
}

void util_close(File *f) {
    if (f->fd >= 0) {
        if (close(f->fd) < 0) {
            fprintf(stderr, "Error closing %s: %s\n",
                            f->name, strerror(errno));
        }
        f->fd = -1;
    }
    if (f->file != NULL) {
        if (fclose(f->file) != 0) {
            fprintf(stderr, "Error closing %s: %s\n",
                            f->name, strerror(errno));
        }
        f->file = NULL;
    }
    return;
}

bool util_open(File *f, int flag) {
    if ((f->fd = open(f->name, flag)) < 0) {
        fprintf(stderr, "Error opening %s: %s\n",
                        f->name, strerror(errno));
        return false;
    } else {
        return true;
    }
}

int util_copy_file(const char *destination, const char *source) {
    int src_fd, dest_fd;
    char buffer[BUFSIZ];
    ssize_t bytes_read, bytes_written;

    src_fd = open(source, O_RDONLY);
    if (src_fd == -1) {
        fprintf(stderr, "Error opening %s: %s\n", source, strerror(errno));
        return -1;
    }

    dest_fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 
                                S_IRUSR | S_IWUSR);
    if (dest_fd == -1) {
        fprintf(stderr, "Error opening %s: %s\n", destination, strerror(errno));
        close(src_fd);
        return -1;
    }

    while ((bytes_read = read(src_fd, buffer, BUFSIZ)) > 0) {
        bytes_written = write(dest_fd, buffer, (size_t) bytes_read);
        if (bytes_written != bytes_read) {
            fprintf(stderr, "Error: Unable to write data to target file");
            close(src_fd);
            close(dest_fd);
            return -1;
        }
    }

    if (bytes_read == -1) {
        fprintf(stderr, "Error: Unable to read data from source file");
        close(src_fd);
        close(dest_fd);
        return -1;
    }

    close(src_fd);
    close(dest_fd);
    return 0;
}
