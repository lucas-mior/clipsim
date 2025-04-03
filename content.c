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
content_trim_spaces(int16 *trimmed, int16 *trimmed_length,
                    char *content, const int16 length) {
    DEBUG_PRINT("%p, %p, %s, %d",
                (void *) trimmed, (void *) trimmed_length, content, length);
    char *out;
    char temp = '\0';
    char *in = content;

    *trimmed = length + 1;
    out = &content[*trimmed];

    if (length >= TRIMMED_SIZE) {
        temp = content[TRIMMED_SIZE];
        content[TRIMMED_SIZE] = '\0';
    }

    while (IS_SPACE(*in))
        in += 1;
    while (*in != '\0') {
        while (IS_SPACE(*in) && IS_SPACE(*(in + 1)))
            in += 1;

        *out = *in;
        out += 1;
        in += 1;
    }
    *out = '\0';
    *trimmed_length = (int16)(out - &content[*trimmed]);

    if (temp)
        content[TRIMMED_SIZE] = temp;

    if (*trimmed_length == length)
        *trimmed = 0;
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

    do {
        const char *mime_type;
        if ((mime_type = magic_buffer(magic, data, (usize)length)) == NULL) {
            error("Error in magic_buffer(%.*s): %s.\n",
                  30, data, magic_error(magic));
            break;
        }
        if (!strncmp(mime_type, "image/", 6))
            return CLIPBOARD_IMAGE;
    } while (0);

    if (length > (ENTRY_MAX_LENGTH - 1)) {
        error("Too large entry. This wont' be added to history.\n");
        return CLIPBOARD_ERROR;
    }

    if (memchr(data, TEXT_TAG, length) || memchr(data, IMAGE_TAG, length)) {
        util_die_notify("Entry %s contains control chars.", data);
    }

    return CLIPBOARD_TEXT;
}
