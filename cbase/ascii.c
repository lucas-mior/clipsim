// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(ASCII_C)
#define ASCII_C

#if !defined(TESTING_ascii)
#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_ascii 1
#else
#define TESTING_ascii 0
#endif
#endif

#include "cbase.h"

bool32
is_ascii(int32 c) {
    return (c >= 0) && (c < 128);
}

bool32
is_cntrl(int32 c) {
    return (c <= 0x1f) || (c == 0x7f);
}

bool32
is_blank(int32 c) {
    return (c == ' ') || (c == '\t');
}

bool32
is_space(int32 c) {
    return ((c >= '\t') && (c <= '\r')) || (c == ' ');
}

bool32
is_digit(int32 c) {
    return (c >= '0') && (c <= '9');
}

bool32
is_upper(int32 c) {
    return (c >= 'A') && (c <= 'Z');
}

bool32
is_lower(int32 c) {
    return (c >= 'a') && (c <= 'z');
}

bool32
is_alpha(int32 c) {
    return is_upper(c) || is_lower(c);
}

bool32
is_alnum(int32 c) {
    return is_alpha(c) || is_digit(c);
}

bool32
is_xdigit(int32 c) {
    return is_digit(c)
           || ((c >= 'A') && (c <= 'F'))
           || ((c >= 'a') && (c <= 'f'));
}

bool32
is_print(int32 c) {
    return (c >= 0x20) && (c <= 0x7e);
}

bool32
is_graph(int32 c) {
    return (c >= 0x21) && (c <= 0x7e);
}

bool32
is_punct(int32 c) {
    return is_graph(c) && !is_alnum(c);
}

#if TESTING_ascii

#define CBASE_IMPLEMENT
#include "cbase.h"

int
main(void) {
    exit(EXIT_SUCCESS);
}

#endif /* TESTING_ascii */

#endif /* ASCII_C */
