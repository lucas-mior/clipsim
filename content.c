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

void
content_remove_newline(char *text, int32 *length) {
    DEBUG_PRINT("%s, %d", text, *length);
    text[*length] = '\0';
    while (text[*length - 1] == '\n') {
        text[*length - 1] = '\0';
        *length -= 1;
    }
    return;
}

void
content_trim_spaces(char **trimmed, int32 *trimmed_length,
                    char *content, const int32 length) {
    DEBUG_PRINT("%p, %p, %s, %d",
                (void *) trimmed, (void *) trimmed_length, content, length);
    char *p;
    char temp = '\0';
    char *c = content;

    *trimmed = p = util_malloc(MIN((usize) length + 1, TRIMMED_SIZE + 1));

    if (length >= TRIMMED_SIZE) {
        temp = content[TRIMMED_SIZE];
        content[TRIMMED_SIZE] = '\0';
    }

    while (IS_SPACE(*c))
        c += 1;
    while (*c != '\0') {
        while (IS_SPACE(*c) && IS_SPACE(*(c + 1)))
            c += 1;

        *p = *c;
        p += 1;
        c += 1;
    }
    *p = '\0';
    *trimmed_length = (int32) (p - *trimmed);

    if (temp) {
        content[TRIMMED_SIZE] = temp;
        temp = '\0';
    }

    if (*trimmed_length == length) {
        free(*trimmed);
        *trimmed = content;
    } else {
        *trimmed = util_realloc(*trimmed, (usize) *trimmed_length + 1);
    }
    return;
}

int32
content_check_content(uchar *data, const int32 length) {
    DEBUG_PRINT("%s, %d", data, length);

    { /* Check if it is made only of spaces and newlines */
        uchar *aux = data;
        do {
            aux += 1;
        } while (IS_SPACE(*(aux - 1)));
        if (*(aux - 1) == '\0') {
            error("Only white space copied to clipboard. "
                  "This won't be added to history.\n");
            return CLIPBOARD_ERROR;
        }
    }

    if (length <= 2) { /* Check if it is a single ascii character
                          possibly followed by new line */
        if ((' ' <= *data) && (*data <= '~')) {
            if (length == 1 || (*(data + 1) == '\n')) {
                error("Ignoring single character '%c'\n", *data);
                return CLIPBOARD_ERROR;
            }
        }
    }

    do {
        const char *mime_type;
        if ((mime_type = magic_buffer(magic, data, (usize) length)) == NULL) {
            error("Error in magic_buffer(%s): %s.\n", data, magic_error(magic));
            break;
        }
        if (!strncmp(mime_type, "image/", 6)) {
            return CLIPBOARD_IMAGE;
        }
    } while (0);

    if (length > (ENTRY_MAX_LENGTH - 1)) {
        error("Too large entry. This wont' be added to history.\n");
        return CLIPBOARD_ERROR;
    }

    return CLIPBOARD_TEXT;
}
