// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(ASCII_NORMALIZATION_C)
#define ASCII_NORMALIZATION_C

#include "cbase.h"

static char
ascii_normalization_lower(char c) {
    if ((c >= 'A') && (c <= 'Z')) {
        c = (char)(c - 'A' + 'a');
    }

    return c;
}

static char
ascii_normalization_upper(char c) {
    if ((c >= 'a') && (c <= 'z')) {
        c = (char)(c - 'a' + 'A');
    }

    return c;
}

int32
ascii_normalize_lower_snake(char *out, char *string, int32 string_len) {
    int32 written = 0;

    ASSERT(out != NULL);
    ASSERT(string_len >= 0);
    ASSERT((string != NULL) || (string_len == 0));

    for (int32 i = 0; i < string_len; i += 1) {
        if (string[i] == ' ') {
            out[written] = '_';
        } else {
            out[written] = ascii_normalization_lower(string[i]);
        }
        written += 1;
    }
    out[written] = '\0';
    return written;
}

int32
ascii_normalize_upper_snake(char *out, char *string, int32 string_len) {
    int32 written = 0;

    ASSERT(out != NULL);
    ASSERT(string_len >= 0);
    ASSERT((string != NULL) || (string_len == 0));

    for (int32 i = 0; i < string_len; i += 1) {
        if (string[i] == ' ') {
            out[written] = '_';
        } else {
            out[written] = ascii_normalization_upper(string[i]);
        }
        written += 1;
    }
    out[written] = '\0';
    return written;
}

int32
ascii_normalize_upper_compact(char *out, char *string, int32 string_len) {
    int32 written = 0;

    ASSERT(out != NULL);
    ASSERT(string_len >= 0);
    ASSERT((string != NULL) || (string_len == 0));

    for (int32 i = 0; i < string_len; i += 1) {
        if (string[i] == ' ') {
            continue;
        }
        out[written] = ascii_normalization_upper(string[i]);
        written += 1;
    }
    out[written] = '\0';
    return written;
}

int32
ascii_normalize_camel_compact(char *out, char *string, int32 string_len) {
    bool capitalize = true;
    int32 written = 0;

    ASSERT(out != NULL);
    ASSERT(string_len >= 0);
    ASSERT((string != NULL) || (string_len == 0));

    for (int32 i = 0; i < string_len; i += 1) {
        if (string[i] == ' ') {
            capitalize = true;
            continue;
        }
        if (capitalize) {
            out[written] = ascii_normalization_upper(string[i]);
            capitalize = false;
        } else {
            out[written] = ascii_normalization_lower(string[i]);
        }
        written += 1;
    }
    out[written] = '\0';
    return written;
}

#endif /* ASCII_NORMALIZATION_C */
