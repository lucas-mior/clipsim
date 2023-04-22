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

#include "clipsim.h"
#include "util.h"
#include "text.h"

void text_bundle_spaces(Entry *e) {
    DEBUG_PRINT("text_bundle_spaces(%.*s, %zu)\n", 30, e->data, e->len)
    char *out;
    char temp = '\0';
    char *c = e->data;

    out = e->out = xalloc(NULL, MIN(e->len+1, OUT_SIZE+1));

    if (e->len >= OUT_SIZE) {
        temp = e->data[OUT_SIZE];
        e->data[OUT_SIZE] = '\0';
    }

    while ((*c == ' ') || (*c == '\t') || (*c == '\n'))
        c++;
    while (*c != '\0') {
        while (((*c == ' ') || (*c == '\t') || (*c == '\n'))
            && ((*(c+1) == ' ') || (*(c+1) == '\t') || (*(c+1) == '\n')))
            c++;

        *out++ = *c++;
        e->olen += 1;
    }
    *out++ = '\0';

    if (temp) {
        e->data[OUT_SIZE] = temp;
        temp = '\0';
    }

    if (e->olen == e->len) {
        free(e->out);
        e->out = e->data;
    }
    return;
}

bool text_valid_content(uchar *data, ulong len) {
    DEBUG_PRINT("text_valid_content(%.*s, %lu) %d\n", 20, data, len)
    static const uchar PNG[] = {0x89, 0x50, 0x4e, 0x47};

    if (len > MAX_ENTRY_SIZE) {
        printf("Too large entry. This wont' be added to history.\n");
        return false;
    }

    { /* Check if it is made only of spaces and newlines */
        uchar *aux = data;
        do {
            aux++;
        } while ((*(aux-1) == ' ')
              || (*(aux-1) == '\t')
              || (*(aux-1) == '\n'));
        if (*(aux-1) == '\0') {
            fprintf(stderr, "Only white space copied to clipboard. "
                            "This won't be added to history.\n");
            return false;
        }
    }

    if (len <= 2) { /* Check if it is a single ascii character 
                       possibly followed by new line */
        if ((' ' <= *data) && (*data <= '~')) {
            if (len == 1 || (*(data+1) == '\n')) {
                fprintf(stderr, "Ignoring single character '%c'\n", *data);
                return false;
            }
        }
    }

    if (len >= 4) { /* check if it is an image */
        if (!memcmp(data, PNG, 4)) {
            fprintf(stderr, "Image copied to clipboard. "
                            "This won't be added to history.\n");
            return false;
        }
    }

    return true;
}
