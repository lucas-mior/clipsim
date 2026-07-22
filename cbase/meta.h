#if !defined(META_H)
#define META_H

#include <stdbool.h>

#include "primitives.h"
#include "base_macros.h"
#include "util.c"

#define ENUM_PREFIX_ TOKEN_
#define ENUM_NAME TokenKind
#define ENUM_BITFLAGS 0
#define ENUM_FIELDS \
    X(TOKEN_UNKNOWN) \
    X(TOKEN_SPACE) \
    X(TOKEN_NEWLINE) \
    X(TOKEN_IDENT) \
    X(TOKEN_LITERAL) \
    X(TOKEN_COMMENT) \
    X(TOKEN_OPERATOR) \
    X(TOKEN_PUNCT) \
    X(TOKEN_PREPROC)
#include "xenums.c"

enum TokenizeFlags {
    TOKENIZE_DEFAULT = 0,
    TOKENIZE_PREPROCESSOR_LINES = 1 << 0,
    TOKENIZE_SKIP_WHITESPACE = 1 << 1,
};

typedef struct Token {
    char *text;
    enum TokenKind kind;
    int32 len;
    int32 column;
    int32 offset;
} Token;

INLINE int32
token_is_val(Token token, char *what) {
    return STREQUAL(token.text, token.len, what);
}

INLINE int32
token_is_ptr(Token *token, char *what) {
    return STREQUAL(token->text, token->len, what);
}

INLINE int32
token_is_val_len(Token token, char *what, int32 what_len) {
    return STREQUAL(token.text, token.len, what, what_len);
}

INLINE int32
token_is_ptr_len(Token *token, char *what, int32 what_len) {
    return STREQUAL(token->text, token->len, what, what_len);
}

#define token_is_2(TOKEN, WHAT) \
  _Generic((TOKEN), \
    Token:   token_is_val, \
    Token *: token_is_ptr \
  )((TOKEN), (WHAT))

#define token_is_3(TOKEN, WHAT, WHAT_LEN) \
  _Generic((TOKEN), \
    Token:   token_is_val_len, \
    Token *: token_is_ptr_len \
  )((TOKEN), (WHAT), (WHAT_LEN))

#define TOKEN_IS(...) SELECT_ON_NUM_ARGS(token_is_, __VA_ARGS__)

typedef struct Tokenization {
    char *text;
    Token *tokens;

    int32 text_len;
    int32 token_count;
    int32 token_capacity;
    int32 padding;
} Tokenization;

typedef struct Line {
    Token *tokens;
    char *text;

    int32 len;
    int32 token_count;
    int32 token_capacity;
    int32 padding;
} Line;

typedef struct Document {
    Line *lines;
    int32 line_count;
    int32 capacity;
} Document;

static int32
precedence_of(char *op, int32 op_len) {

#define OP_IS(s) STREQUAL(op, op_len, s)

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

#endif /* META_H */
