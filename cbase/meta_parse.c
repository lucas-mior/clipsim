// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(META_PARSE_C)
#define META_PARSE_C

#include "cbase.h"

void
free_line(Line *line) {
    free_line_tokens(line);
    free2(line->text, line->len + 1);
    line->text = NULL;
    line->len = 0;
    return;
}

#endif /* META_PARSE_C */
