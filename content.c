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

#include "clipsim.h"

void content_remove_newline(char *text, ulong *length) {
    text[*length] = '\0';
    while (text[*length-1] == '\n') {
        text[*length-1] = '\0';
        *length -= 1;
    }
    return;
}

void content_trim_spaces(char **trimmed, ulong *trimmed_length,
                         char *content, const ulong length) {
    DEBUG_PRINT("content_trim_spaces(%p, %p, %s, %lu)\n", 
                trimmed, trimmed_length, content, length)
    char *p;
    char temp = '\0';
    char *c = content;

    *trimmed = p = util_realloc(NULL, MIN(length+1, TRIMMED_SIZE+1));

    if (length >= TRIMMED_SIZE) {
        temp = content[TRIMMED_SIZE];
        content[TRIMMED_SIZE] = '\0';
    }

    while (IS_SPACE(*c))
        c++;
    while (*c != '\0') {
        while (IS_SPACE(*c) && IS_SPACE(*(c+1)))
            c++;

        *p++ = *c++;
    }
    *p = '\0';
    *trimmed_length = (size_t) (p - *trimmed);

    if (temp) {
        content[TRIMMED_SIZE] = temp;
        temp = '\0';
    }

    if (*trimmed_length == length) {
        free(*trimmed);
        *trimmed = content;
    } else {
        *trimmed = util_realloc(*trimmed, *trimmed_length+1);
    }
    return;
}

int32 content_check_content(uchar *data, const ulong length) {
    DEBUG_PRINT("content_check_content(%.*s, %lu)\n", 20, data, length)
    static const uchar PNG[] = {0x89, 0x50, 0x4e, 0x47};

    { /* Check if it is made only of spaces and newlines */
        uchar *aux = data;
        do {
            aux++;
        } while (IS_SPACE(*(aux-1)));
        if (*(aux-1) == '\0') {
            fprintf(stderr, "Only white space copied to clipboard. "
                            "This won't be added to history.\n");
            return ERROR;
        }
    }

    if (length <= 2) { /* Check if it is a single ascii character
                       possibly followed by new line */
        if ((' ' <= *data) && (*data <= '~')) {
            if (length == 1 || (*(data+1) == '\n')) {
                fprintf(stderr, "Ignoring single character '%c'\n", *data);
                return ERROR;
            }
        }
    }

    if (length >= 4) { /* check if it is an image */
        if (!memcmp(data, PNG, 4)) {
            fprintf(stderr, "Image copied to clipboard.\n");
            return IMAGE;
        }
    }

    if (length > ENTRY_MAX_LENGTH) {
        printf("Too large entry. This wont' be added to history.\n");
        return ERROR;
    }

    return TEXT;
}
