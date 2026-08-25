// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(META_COMMON_C)
#define META_COMMON_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_meta_common 1
#elif !defined(TESTING_meta_common)
#define TESTING_meta_common 0
#endif

#include "cbase.h"

#define ENUM_NAME TokenKind
#define ENUM_BITFLAGS 0
#define ENUM_PREFIX_ TOKEN_
#define ENUM_FIELDS TOKEN_KIND_FIELDS
#define XENUMS_FUNCTIONS_ONLY 1
#define XENUMS_NO_TESTS 1
#include "xenums.c"
#undef XENUMS_NO_TESTS

int32
token_is_val(Token token, char *what) {
    return STREQUAL(token.text, token.len, what);
}

int32
token_is_ptr(Token *token, char *what) {
    return STREQUAL(token->text, token->len, what);
}

int32
token_is_val_len(Token token, char *what, int32 what_len) {
    return STREQUAL(token.text, token.len, what, what_len);
}

int32
token_is_ptr_len(Token *token, char *what, int32 what_len) {
    return STREQUAL(token->text, token->len, what, what_len);
}

int32
precedence_of(char *op, int32 op_len) {
#define OP_IS(STRING) STREQUAL(op, op_len, STRING)

    if (OP_IS("||")) {
        return true;
    }
    if (OP_IS("&&")) {
        return 2;
    }
    if (OP_IS("|")) {
        return 3;
    }
    if (OP_IS("^")) {
        return 4;
    }
    if (OP_IS("&")) {
        return 5;
    }
    if (OP_IS("==") || OP_IS("!=")) {
        return 6;
    }
    if (OP_IS("<") || OP_IS("<=") || OP_IS(">") || OP_IS(">=")) {
        return 7;
    }
    if (OP_IS("<<") || OP_IS(">>")) {
        return 8;
    }
    if (OP_IS("+") || OP_IS("-")) {
        return 9;
    }
    if (OP_IS("*") || OP_IS("/") || OP_IS("%")) {
        return 10;
    }

#undef OP_IS

    return false;
}

#if TESTING_meta_common
#define CBASE_IMPLEMENT
#include "cbase.h"

int
main(void) {
    (void)precedence_of;
    exit(EXIT_SUCCESS);
}
#endif

#endif /* META_COMMON_C */
