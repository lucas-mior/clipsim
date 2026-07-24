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

static void
TOKEN_str_free(char *string) {
    (void)string;
    return;
}

static char *
TOKEN_str(enum TokenKind value) {
    switch (value) {
    case TOKEN_UNKNOWN:
        return "TOKEN_UNKNOWN";
    case TOKEN_SPACE:
        return "TOKEN_SPACE";
    case TOKEN_NEWLINE:
        return "TOKEN_NEWLINE";
    case TOKEN_IDENT:
        return "TOKEN_IDENT";
    case TOKEN_LITERAL:
        return "TOKEN_LITERAL";
    case TOKEN_COMMENT:
        return "TOKEN_COMMENT";
    case TOKEN_OPERATOR:
        return "TOKEN_OPERATOR";
    case TOKEN_PUNCT:
        return "TOKEN_PUNCT";
    case TOKEN_PREPROC:
        return "TOKEN_PREPROC";
    case TOKEN_LAST:
        return "TOKEN_LAST";
    default:
        return "Invalid enum value";
    }
}

static bool
TOKEN_token_equals(char *token, int32 token_len, char *name) {
    int32 name_len = strlen32(name);

    if (token_len != name_len) {
        return false;
    }
    return !strncmp32(token, name, token_len);
}

static bool
TOKEN_token_equals_enum_name(char *token, int32 token_len, char *name) {
    char *prefix = "TOKEN_";
    int32 prefix_len = strlen32(prefix);

    if (TOKEN_token_equals(token, token_len, name)) {
        return true;
    }
    if (strncmp32(name, prefix, prefix_len)) {
        return false;
    }
    return TOKEN_token_equals(token, token_len, name + prefix_len);
}

static enum TokenKind
TOKEN_parse(char *string) {
    enum TokenKind result = TOKEN_UNKNOWN;
    char *p = string;

    if (p == NULL) {
        return result;
    }
    while (*p != '\0') {
        char *token;
        int32 token_len;
        bool matched;

        while ((*p == ' ') || (*p == '\t') || (*p == '\n')
               || (*p == '\r') || (*p == '|') || (*p == '(')
               || (*p == ')')) {
            p += 1;
        }
        if (*p == '\0') {
            break;
        }

        token = p;
        while (is_ident_char(*p)) {
            p += 1;
        }
        token_len = (int32)(p - token);
        if (token_len <= 0) {
            error2("Error: invalid enum parse character '%c' in %s.\n",
                   *p, string);
            TRAP();
        }

        matched = false;
        if (TOKEN_token_equals_enum_name(token, token_len, "TOKEN_UNKNOWN")) {
            result = (enum TokenKind)(result | TOKEN_UNKNOWN);
            matched = true;
        } else if (TOKEN_token_equals_enum_name(token, token_len,
                                                "TOKEN_SPACE")) {
            result = (enum TokenKind)(result | TOKEN_SPACE);
            matched = true;
        } else if (TOKEN_token_equals_enum_name(token, token_len,
                                                "TOKEN_NEWLINE")) {
            result = (enum TokenKind)(result | TOKEN_NEWLINE);
            matched = true;
        } else if (TOKEN_token_equals_enum_name(token, token_len,
                                                "TOKEN_IDENT")) {
            result = (enum TokenKind)(result | TOKEN_IDENT);
            matched = true;
        } else if (TOKEN_token_equals_enum_name(token, token_len,
                                                "TOKEN_LITERAL")) {
            result = (enum TokenKind)(result | TOKEN_LITERAL);
            matched = true;
        } else if (TOKEN_token_equals_enum_name(token, token_len,
                                                "TOKEN_COMMENT")) {
            result = (enum TokenKind)(result | TOKEN_COMMENT);
            matched = true;
        } else if (TOKEN_token_equals_enum_name(token, token_len,
                                                "TOKEN_OPERATOR")) {
            result = (enum TokenKind)(result | TOKEN_OPERATOR);
            matched = true;
        } else if (TOKEN_token_equals_enum_name(token, token_len,
                                                "TOKEN_PUNCT")) {
            result = (enum TokenKind)(result | TOKEN_PUNCT);
            matched = true;
        } else if (TOKEN_token_equals_enum_name(token, token_len,
                                                "TOKEN_PREPROC")) {
            result = (enum TokenKind)(result | TOKEN_PREPROC);
            matched = true;
        } else if (TOKEN_token_equals(token, token_len, "TOKEN_LAST")
                   || TOKEN_token_equals(token, token_len, "LAST")) {
            result = (enum TokenKind)(result | TOKEN_LAST);
            matched = true;
        }

        if (!matched) {
            error2("Error: unknown enum token '%.*s' while parsing %s.\n",
                   token_len, token, string);
            TRAP();
        }
    }
    return result;
}

static void
TOKEN_functions_sink(void) {
    (void)TOKEN_str;
    (void)TOKEN_str_free;
    (void)TOKEN_parse;
    return;
}

static int32
token_is_val(Token token, char *what) {
    return STREQUAL(token.text, token.len, what);
}

static int32
token_is_ptr(Token *token, char *what) {
    return STREQUAL(token->text, token->len, what);
}

static int32
token_is_val_len(Token token, char *what, int32 what_len) {
    return STREQUAL(token.text, token.len, what, what_len);
}

static int32
token_is_ptr_len(Token *token, char *what, int32 what_len) {
    return STREQUAL(token->text, token->len, what, what_len);
}

static int32
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
