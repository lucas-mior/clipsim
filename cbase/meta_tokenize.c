// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(META_TOKENIZE_C)
#define META_TOKENIZE_C

#define TOKENIZE_INITIAL_TOKEN_CAPACITY 32

#if !defined(TESTING_meta_tokenize)
#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_meta_tokenize 1
#else
#define TESTING_meta_tokenize 0
#endif
#endif

#include "cbase.h"

bool
char_is_alpha(char c) {
    if (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'))) {
        return true;
    }
    return false;
}

bool
char_is_digit(char c) {
    if ((c >= '0') && (c <= '9')) {
        return true;
    }
    return false;
}

bool
token_is_number(Token *token) {
    if ((token->kind == TOKEN_LITERAL) && (token->len > 0)
        && (char_is_digit(token->text[0])
            || ((token->text[0] == '.') && (token->len > 1)
                && char_is_digit(token->text[1])))) {
        return true;
    }
    return false;
}

bool
char_is_identifier_start(char c) {
    if (char_is_alpha(c) || (c == '_')) {
        return true;
    }
    return false;
}

bool
char_is_identifier_body(char c) {
    if (char_is_identifier_start(c) || char_is_digit(c)) {
        return true;
    }
    return false;
}

bool
char_is_horizontal_space(char c) {
    switch (c) {
    case ' ':
    case '\t':
    case '\v':
    case '\f':
    case '\r':
        return true;
    default:
        return false;
    }
}

bool
char_is_number_body(char c) {
    if (char_is_identifier_body(c)
        || char_is_digit(c)
        || (c == '.')
        || (c == '\'')) {
        return true;
    }
    return false;
}

int32
scan_number_literal(char *text, int32 text_len, int32 start) {
    int32 result;

    result = 1;
    if ((text[start] == '.') && ((start + 1) < text_len)
        && char_is_digit(text[start + 1])) {
        result = 2;
    }

    while ((start + result) < text_len) {
        char c = text[start + result];
        char prev = text[start + result - 1];

        if (char_is_number_body(c)) {
            result += 1;
        } else if (((c == '+') || (c == '-'))
                   && ((prev == 'e') || (prev == 'E') || (prev == 'p')
                       || (prev == 'P'))) {
            result += 1;
        } else {
            break;
        }
    }
    return result;
}

bool
line_starts_preprocessor(char *text, int32 len) {
    for (int32 i = 0; i < len; i += 1) {
        if (char_is_horizontal_space(text[i])) {
            continue;
        }
        if (text[i] == '#') {
            return true;
        }
        break;
    }
    return false;
}

static void
tokenization_add_token(Tokenization *tokenization, enum TokenKind category,
                        char *text, int32 len, int32 offset) {
    Token *token;

    if (len <= 0) {
        return;
    }

    if (tokenization->token_count >= tokenization->token_capacity) {
        int32 new_capacity;

        if (tokenization->token_capacity > 0) {
            new_capacity = tokenization->token_capacity*2;
        } else {
            new_capacity = TOKENIZE_INITIAL_TOKEN_CAPACITY;
        }
        if (tokenization->tokens) {
            tokenization->tokens = realloc2(tokenization->tokens,
                                            tokenization->token_capacity,
                                            new_capacity,
                                            SIZEOF(*tokenization->tokens));
        } else {
            tokenization->tokens =
                malloc2(new_capacity*SIZEOF(*tokenization->tokens));
        }
        tokenization->token_capacity = new_capacity;
    }

    token = &tokenization->tokens[tokenization->token_count];
    token->kind = category;
    token->len = len;
    token->column = offset;
    token->offset = offset;
    token->text = text;

    tokenization->token_count += 1;
    return;
}

int32
literal_quote_index(char *text, int32 text_len, int32 start) {
    int32 result = -1;

    switch (text[start]) {
    case '\'':
    case '"':
        result = start;
        break;
    case 'L':
    case 'U':
        if (((start + 1) < text_len)
            && ((text[start + 1] == '\'') || (text[start + 1] == '"'))) {
            result = start + 1;
        }
        break;
    case 'u':
        if (((start + 1) < text_len)
            && ((text[start + 1] == '\'') || (text[start + 1] == '"'))) {
            result = start + 1;
        } else if (((start + 2) < text_len)
                   && (text[start + 1] == '8')
                   && ((text[start + 2] == '\'') || (text[start + 2] == '"'))) {
            result = start + 2;
        }
        break;
    default:
        break;
    }

    return result;
}

int32
scan_literal_token(char *text, int32 text_len, int32 start) {
    char quote;
    int32 quote_index;
    int32 i;
    int32 result;

    result = 0;
    quote_index = literal_quote_index(text, text_len, start);
    if (quote_index < 0) {
        return result;
    }

    quote = text[quote_index];
    i = quote_index + 1;
    while (i < text_len) {
        if (text[i] == '\\') {
            if ((i + 1) < text_len) {
                i += 2;
            } else {
                i = text_len;
            }
            continue;
        }
        if (text[i] == quote) {
            i += 1;
            break;
        }
        if (text[i] == '\n') {
            break;
        }
        i += 1;
    }

    result = i - start;
    return result;
}

int32
scan_line_comment(char *text, int32 text_len, int32 start) {
    int32 i;

    i = start;
    while ((i < text_len) && (text[i] != '\n')) {
        i += 1;
    }
    return i - start;
}

int32
scan_block_comment(char *text, int32 len, int32 start, bool *in_block_comment) {
    int32 i;

    i = start;
    if (!*in_block_comment) {
        i += 2;
    }
    while (i < len) {
        if (((i + 1) < len) && (text[i] == '*') && (text[i + 1] == '/')) {
            i += 2;
            *in_block_comment = false;
            return i - start;
        }
        if (text[i] == '\n') {
            *in_block_comment = true;
            return i - start;
        }
        i += 1;
    }

    *in_block_comment = true;
    return i - start;
}

enum TokenKind
operator_or_punct_category(char *text, int32 len, int32 start, int32 *out_len) {
    enum TokenKind result;
    char a;
    char b;
    char c;
    char d;

    result = TOKEN_OPERATOR;
    *out_len = 1;
    a = text[start];
    b = '\0';
    c = '\0';
    d = '\0';
    if ((start + 1) < len) {
        b = text[start + 1];
    }
    if ((start + 2) < len) {
        c = text[start + 2];
    }
    if ((start + 3) < len) {
        d = text[start + 3];
    }

    switch (a) {
    case '%':
        if ((b == ':') && (c == '%') && (d == ':')) {
            *out_len = 4;
        } else if (b == '=') {
            *out_len = 2;
        } else if (b == ':') {
            *out_len = 2;
        } else if (b == '>') {
            *out_len = 2;
            result = TOKEN_PUNCT;
        }
        break;
    case '<':
        if ((b == '<') && (c == '=')) {
            *out_len = 3;
        } else if ((b == '=') || (b == '<')) {
            *out_len = 2;
        } else if ((b == ':') || (b == '%')) {
            *out_len = 2;
            result = TOKEN_PUNCT;
        }
        break;
    case '>':
        if ((b == '>') && (c == '=')) {
            *out_len = 3;
        } else if ((b == '=') || (b == '>')) {
            *out_len = 2;
        }
        break;
    case '.':
        if ((b == '.') && (c == '.')) {
            *out_len = 3;
            result = TOKEN_PUNCT;
        } else {
            result = TOKEN_PUNCT;
        }
        break;
    case '+':
        if ((b == '+') || (b == '=')) {
            *out_len = 2;
        }
        break;
    case '-':
        if ((b == '-') || (b == '=') || (b == '>')) {
            *out_len = 2;
        }
        break;
    case '*':
    case '/':
    case '=':
    case '!':
    case '^':
        if (b == '=') {
            *out_len = 2;
        }
        break;
    case '&':
        if ((b == '&') || (b == '=')) {
            *out_len = 2;
        }
        break;
    case '|':
        if ((b == '|') || (b == '=')) {
            *out_len = 2;
        }
        break;
    case ':':
        if (b == ':') {
            *out_len = 2;
        } else if (b == '>') {
            *out_len = 2;
            result = TOKEN_PUNCT;
        } else {
            result = TOKEN_PUNCT;
        }
        break;
    case '#':
        if (b == '#') {
            *out_len = 2;
        }
        break;
    case ',':
    case ';':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
        result = TOKEN_PUNCT;
        break;
    default:
        break;
    }
    return result;
}

bool
char_is_operator_or_punct(char c) {
    switch (c) {
    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
    case '=':
    case '!':
    case '<':
    case '>':
    case '&':
    case '|':
    case '^':
    case '~':
    case '?':
    case ':':
    case '.':
    case ',':
    case ';':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '#':
        return true;
    default:
        return false;
    }
}

Tokenization
tokenize_with_flags(char *text, int32 text_len, int32 flags) {
    Tokenization result = {0};
    bool in_block_comment = false;
    int32 i;

    result.text = text;
    result.text_len = text_len;

    if ((flags & TOKENIZE_PREPROCESSOR_LINES)
        && line_starts_preprocessor(text, text_len)) {
        tokenization_add_token(&result, TOKEN_PREPROC, text, text_len, 0);
        return result;
    }

    i = 0;
    while (i < text_len) {
        enum TokenKind category;
        int32 token_len;

        if (in_block_comment) {
            if (text[i] == '\n') {
                if ((flags & TOKENIZE_SKIP_WHITESPACE) == 0) {
                    tokenization_add_token(&result, TOKEN_NEWLINE,
                                           text + i, 1, i);
                }
                i += 1;
                continue;
            }
            token_len = scan_block_comment(text, text_len, i,
                                           &in_block_comment);
            tokenization_add_token(&result, TOKEN_COMMENT,
                                   text + i, token_len, i);
            i += token_len;
            continue;
        }

        if (text[i] == '\n') {
            if ((flags & TOKENIZE_SKIP_WHITESPACE) == 0) {
                tokenization_add_token(&result, TOKEN_NEWLINE, text + i, 1, i);
            }
            i += 1;
        } else if (char_is_horizontal_space(text[i])) {
            token_len = 1;
            while (((i + token_len) < text_len)
                   && char_is_horizontal_space(text[i + token_len])) {
                token_len += 1;
            }
            if ((flags & TOKENIZE_SKIP_WHITESPACE) == 0) {
                tokenization_add_token(&result, TOKEN_SPACE,
                                       text + i, token_len, i);
            }
            i += token_len;
        } else if (((i + 1) < text_len) && (text[i] == '/')
                   && (text[i + 1] == '/')) {
            token_len = scan_line_comment(text, text_len, i);
            tokenization_add_token(&result, TOKEN_COMMENT,
                                   text + i, token_len, i);
            i += token_len;
        } else if (((i + 1) < text_len) && (text[i] == '/')
                   && (text[i + 1] == '*')) {
            token_len = scan_block_comment(text, text_len, i,
                                           &in_block_comment);
            tokenization_add_token(&result, TOKEN_COMMENT,
                                   text + i, token_len, i);
            i += token_len;
        } else if ((text[i] == '\'') || (text[i] == '"')) {
            token_len = scan_literal_token(text, text_len, i);
            tokenization_add_token(&result, TOKEN_LITERAL,
                                   text + i, token_len, i);
            i += token_len;
        } else if (((text[i] == 'L') || (text[i] == 'U'))
                   && ((i + 1) < text_len)
                   && ((text[i + 1] == '\'')
                       || (text[i + 1] == '"'))) {
            token_len = scan_literal_token(text, text_len, i);
            tokenization_add_token(&result, TOKEN_LITERAL,
                                   text + i, token_len, i);
            i += token_len;
        } else if ((text[i] == 'u')
                   && ((((i + 1) < text_len)
                        && ((text[i + 1] == '\'')
                            || (text[i + 1] == '"')))
                       || (((i + 2) < text_len)
                           && (text[i + 1] == '8')
                           && ((text[i + 2] == '\'')
                               || (text[i + 2] == '"'))))) {
            token_len = scan_literal_token(text, text_len, i);
            tokenization_add_token(&result, TOKEN_LITERAL,
                                   text + i, token_len, i);
            i += token_len;
        } else if (char_is_identifier_start(text[i])) {
            token_len = 1;
            while (((i + token_len) < text_len)
                   && char_is_identifier_body(text[i + token_len])) {
                token_len += 1;
            }
            tokenization_add_token(&result, TOKEN_IDENT,
                                   text + i, token_len, i);
            i += token_len;
        } else if (char_is_digit(text[i])
                   || ((text[i] == '.') && ((i + 1) < text_len)
                       && char_is_digit(text[i + 1]))) {
            token_len = scan_number_literal(text, text_len, i);
            tokenization_add_token(&result, TOKEN_LITERAL,
                                   text + i, token_len, i);
            i += token_len;
        } else {
            switch (text[i]) {
            case '+':
            case '-':
            case '*':
            case '/':
            case '%':
            case '=':
            case '!':
            case '<':
            case '>':
            case '&':
            case '|':
            case '^':
            case '~':
            case '?':
            case ':':
            case '.':
            case ',':
            case ';':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '#':
                category = operator_or_punct_category(text, text_len, i,
                                                      &token_len);
                tokenization_add_token(&result, category,
                                       text + i, token_len, i);
                i += token_len;
                break;
            default:
                tokenization_add_token(&result, TOKEN_UNKNOWN, text + i, 1, i);
                i += 1;
                break;
            }
        }
    }
    return result;
}

bool
token_is_trivia(Token *token) {
    switch (token->kind) {
    case TOKEN_SPACE:
    case TOKEN_NEWLINE:
    case TOKEN_COMMENT:
        return true;
    case TOKEN_UNKNOWN:
    case TOKEN_IDENT:
    case TOKEN_LITERAL:
    case TOKEN_OPERATOR:
    case TOKEN_PUNCT:
    case TOKEN_PREPROC:
    case TOKEN_COUNT:
    default:
        return false;
    }
}

enum TokenDelimiterKind {
    TOKEN_DELIMITER_NONE = 0,
    TOKEN_DELIMITER_PAREN,
    TOKEN_DELIMITER_BRACKET,
    TOKEN_DELIMITER_BRACE,
};

#define TOKEN_DELIMITER_STACK_LOCAL_CAPACITY 32

typedef struct TokenDelimiterStack {
    enum TokenDelimiterKind local[TOKEN_DELIMITER_STACK_LOCAL_CAPACITY];
    enum TokenDelimiterKind *items;
    enum TokenDelimiterKind *heap;
    int32 count;
    int32 capacity;
    int32 max_capacity;
} TokenDelimiterStack;

static enum TokenDelimiterKind
token_open_delimiter_kind(Token *token) {
    if (token == NULL) {
        return TOKEN_DELIMITER_NONE;
    }
    if (TOKEN_IS(token, "(")) {
        return TOKEN_DELIMITER_PAREN;
    }
    if (TOKEN_IS(token, "[")) {
        return TOKEN_DELIMITER_BRACKET;
    }
    if (TOKEN_IS(token, "{")) {
        return TOKEN_DELIMITER_BRACE;
    }
    return TOKEN_DELIMITER_NONE;
}

static enum TokenDelimiterKind
token_close_delimiter_kind(Token *token) {
    if (token == NULL) {
        return TOKEN_DELIMITER_NONE;
    }
    if (TOKEN_IS(token, ")")) {
        return TOKEN_DELIMITER_PAREN;
    }
    if (TOKEN_IS(token, "]")) {
        return TOKEN_DELIMITER_BRACKET;
    }
    if (TOKEN_IS(token, "}")) {
        return TOKEN_DELIMITER_BRACE;
    }
    return TOKEN_DELIMITER_NONE;
}

bool
token_is_open_delimiter(Token *token) {
    return token_open_delimiter_kind(token) != TOKEN_DELIMITER_NONE;
}

bool
token_is_close_delimiter(Token *token) {
    return token_close_delimiter_kind(token) != TOKEN_DELIMITER_NONE;
}

bool
token_delimiter_depth_equal(TokenDelimiterDepth left,
                            TokenDelimiterDepth right) {
    return (left.paren == right.paren)
           && (left.bracket == right.bracket)
           && (left.brace == right.brace);
}

bool
token_delimiter_depth_is_zero(TokenDelimiterDepth depth) {
    return token_delimiter_depth_equal(depth, (TokenDelimiterDepth){0});
}

static bool
token_delimiter_depth_is_valid(TokenDelimiterDepth depth) {
    return (depth.paren >= 0) && (depth.bracket >= 0) && (depth.brace >= 0);
}

static void
token_delimiter_depth_increment(TokenDelimiterDepth *depth,
                                enum TokenDelimiterKind kind) {
    switch (kind) {
    case TOKEN_DELIMITER_PAREN:
        depth->paren += 1;
        break;
    case TOKEN_DELIMITER_BRACKET:
        depth->bracket += 1;
        break;
    case TOKEN_DELIMITER_BRACE:
        depth->brace += 1;
        break;
    case TOKEN_DELIMITER_NONE:
    default:
        break;
    }
    return;
}

static void
token_delimiter_depth_decrement(TokenDelimiterDepth *depth,
                                enum TokenDelimiterKind kind) {
    switch (kind) {
    case TOKEN_DELIMITER_PAREN:
        depth->paren -= 1;
        break;
    case TOKEN_DELIMITER_BRACKET:
        depth->bracket -= 1;
        break;
    case TOKEN_DELIMITER_BRACE:
        depth->brace -= 1;
        break;
    case TOKEN_DELIMITER_NONE:
    default:
        break;
    }
    return;
}

static void
token_delimiter_stack_init(TokenDelimiterStack *stack, int32 max_capacity) {
    *stack = (TokenDelimiterStack){0};
    stack->items = stack->local;
    stack->capacity = TOKEN_DELIMITER_STACK_LOCAL_CAPACITY;
    stack->max_capacity = max_capacity;
    return;
}

static void
token_delimiter_stack_free(TokenDelimiterStack *stack) {
    if (stack->heap != NULL) {
        free2(stack->heap, stack->max_capacity*SIZEOF(*stack->heap));
    }
    stack->items = NULL;
    stack->heap = NULL;
    stack->count = 0;
    stack->capacity = 0;
    stack->max_capacity = 0;
    return;
}

static bool
token_delimiter_stack_push(TokenDelimiterStack *stack,
                           enum TokenDelimiterKind kind) {
    if (stack->count >= stack->max_capacity) {
        return false;
    }
    if (stack->count == stack->capacity) {
        enum TokenDelimiterKind *heap;

        heap = malloc2(stack->max_capacity*SIZEOF(*heap));
        memcpy64(heap, stack->items, stack->count*SIZEOF(*heap));
        stack->items = heap;
        stack->heap = heap;
        stack->capacity = stack->max_capacity;
    }
    stack->items[stack->count] = kind;
    stack->count += 1;
    return true;
}

static bool
token_delimiter_stack_consume_forward(TokenDelimiterStack *stack,
                                      TokenDelimiterDepth *depth,
                                      Token *token) {
    enum TokenDelimiterKind kind;

    kind = token_open_delimiter_kind(token);
    if (kind != TOKEN_DELIMITER_NONE) {
        if (!token_delimiter_stack_push(stack, kind)) {
            return false;
        }
        token_delimiter_depth_increment(depth, kind);
        return true;
    }

    kind = token_close_delimiter_kind(token);
    if (kind == TOKEN_DELIMITER_NONE) {
        return true;
    }
    if ((stack->count == 0) || (stack->items[stack->count - 1] != kind)) {
        return false;
    }
    stack->count -= 1;
    token_delimiter_depth_decrement(depth, kind);
    return true;
}

static bool
token_delimiter_stack_consume_reverse(TokenDelimiterStack *stack,
                                      Token *token) {
    enum TokenDelimiterKind kind;

    kind = token_close_delimiter_kind(token);
    if (kind != TOKEN_DELIMITER_NONE) {
        return token_delimiter_stack_push(stack, kind);
    }

    kind = token_open_delimiter_kind(token);
    if (kind == TOKEN_DELIMITER_NONE) {
        return true;
    }
    if ((stack->count == 0) || (stack->items[stack->count - 1] != kind)) {
        return false;
    }
    stack->count -= 1;
    return true;
}

bool
token_range_is_valid(Tokenization *tokenization, TokenRange range) {
    if (tokenization == NULL) {
        return false;
    }
    return (range.first >= 0)
           && (range.first <= range.end)
           && (range.end <= tokenization->token_count);
}

int32
token_range_first_significant(Tokenization *tokenization, TokenRange range) {
    if (!token_range_is_valid(tokenization, range)) {
        return -1;
    }
    for (int32 i = range.first; i < range.end; i += 1) {
        if (!token_is_trivia(&tokenization->tokens[i])) {
            return i;
        }
    }
    return -1;
}

int32
token_range_last_significant(Tokenization *tokenization, TokenRange range) {
    if (!token_range_is_valid(tokenization, range)) {
        return -1;
    }
    for (int32 i = range.end - 1; i >= range.first; i -= 1) {
        if (!token_is_trivia(&tokenization->tokens[i])) {
            return i;
        }
    }
    return -1;
}

/*
 * Range navigation treats token_index as a cursor. Forward searches begin
 * after it and return range.end when exhausted. Reverse searches begin before
 * it and return range.first - 1 when exhausted. Cursors outside the range are
 * clamped to the corresponding edge.
 */
static int32
token_range_next_start(TokenRange range, int32 token_index) {
    if (token_index < range.first) {
        return range.first;
    }
    if (token_index >= (range.end - 1)) {
        return range.end;
    }
    return token_index + 1;
}

static int32
token_range_previous_start(TokenRange range, int32 token_index) {
    if (token_index > range.end) {
        return range.end - 1;
    }
    if (token_index <= range.first) {
        return range.first - 1;
    }
    return token_index - 1;
}

int32
token_range_next_significant(Tokenization *tokenization, TokenRange range,
                             int32 token_index) {
    int32 i;

    if (!token_range_is_valid(tokenization, range)) {
        return -1;
    }
    i = token_range_next_start(range, token_index);
    while ((i < range.end) && token_is_trivia(&tokenization->tokens[i])) {
        i += 1;
    }
    return i;
}

int32
token_range_previous_significant(Tokenization *tokenization, TokenRange range,
                                 int32 token_index) {
    int32 i;

    if (!token_range_is_valid(tokenization, range)) {
        return -1;
    }
    i = token_range_previous_start(range, token_index);
    while ((i >= range.first) && token_is_trivia(&tokenization->tokens[i])) {
        i -= 1;
    }
    return i;
}

int32
token_range_next_kind(Tokenization *tokenization, TokenRange range,
                      int32 token_index, enum TokenKind kind) {
    int32 i;

    if (!token_range_is_valid(tokenization, range)) {
        return -1;
    }
    i = token_range_next_start(range, token_index);
    while ((i < range.end) && (tokenization->tokens[i].kind != kind)) {
        i += 1;
    }
    return i;
}

int32
token_range_previous_kind(Tokenization *tokenization, TokenRange range,
                          int32 token_index, enum TokenKind kind) {
    int32 i;

    if (!token_range_is_valid(tokenization, range)) {
        return -1;
    }
    i = token_range_previous_start(range, token_index);
    while ((i >= range.first) && (tokenization->tokens[i].kind != kind)) {
        i -= 1;
    }
    return i;
}

int32
token_range_next_text(Tokenization *tokenization, TokenRange range,
                      int32 token_index, char *text, int32 text_len) {
    int32 i;

    if (!token_range_is_valid(tokenization, range) || (text_len < 0)) {
        return -1;
    }
    i = token_range_next_start(range, token_index);
    while ((i < range.end)
           && !TOKEN_IS(&tokenization->tokens[i], text, text_len)) {
        i += 1;
    }
    return i;
}

int32
token_range_previous_text(Tokenization *tokenization, TokenRange range,
                          int32 token_index, char *text, int32 text_len) {
    int32 i;

    if (!token_range_is_valid(tokenization, range) || (text_len < 0)) {
        return -1;
    }
    i = token_range_previous_start(range, token_index);
    while ((i >= range.first)
           && !TOKEN_IS(&tokenization->tokens[i], text, text_len)) {
        i -= 1;
    }
    return i;
}

bool
token_range_delimiter_depth_before(Tokenization *tokenization,
                                   TokenRange range, int32 token_index,
                                   TokenDelimiterDepth *result) {
    TokenDelimiterStack stack;
    TokenDelimiterDepth depth = {0};
    bool valid = true;

    if (result != NULL) {
        *result = (TokenDelimiterDepth){0};
    }
    if (!token_range_is_valid(tokenization, range) || (result == NULL)
        || (token_index < range.first) || (token_index > range.end)) {
        return false;
    }

    token_delimiter_stack_init(&stack, range.end - range.first);
    for (int32 i = range.first; i < token_index; i += 1) {
        if (!token_delimiter_stack_consume_forward(&stack, &depth,
                                                   &tokenization->tokens[i])) {
            valid = false;
            break;
        }
    }
    token_delimiter_stack_free(&stack);
    if (valid) {
        *result = depth;
    }
    return valid;
}

bool
token_range_is_balanced(Tokenization *tokenization, TokenRange range) {
    TokenDelimiterStack stack;
    TokenDelimiterDepth depth = {0};
    bool result = true;

    if (!token_range_is_valid(tokenization, range)) {
        return false;
    }

    token_delimiter_stack_init(&stack, range.end - range.first);
    for (int32 i = range.first; i < range.end; i += 1) {
        if (!token_delimiter_stack_consume_forward(&stack, &depth,
                                                   &tokenization->tokens[i])) {
            result = false;
            break;
        }
    }
    if (stack.count != 0) {
        result = false;
    }
    token_delimiter_stack_free(&stack);
    return result;
}

int32
token_range_matching_delimiter_forward(Tokenization *tokenization,
                                       TokenRange range, int32 open_index) {
    TokenDelimiterStack stack;
    TokenDelimiterDepth depth = {0};
    int32 result = -1;

    if (!token_range_is_valid(tokenization, range)
        || (open_index < range.first) || (open_index >= range.end)
        || !token_is_open_delimiter(&tokenization->tokens[open_index])) {
        return -1;
    }

    token_delimiter_stack_init(&stack, range.end - open_index);
    for (int32 i = open_index; i < range.end; i += 1) {
        if (!token_delimiter_stack_consume_forward(&stack, &depth,
                                                   &tokenization->tokens[i])) {
            break;
        }
        if (stack.count == 0) {
            result = i;
            break;
        }
    }
    token_delimiter_stack_free(&stack);
    return result;
}

int32
token_range_matching_delimiter_reverse(Tokenization *tokenization,
                                       TokenRange range, int32 close_index) {
    TokenDelimiterStack stack;
    int32 result = -1;

    if (!token_range_is_valid(tokenization, range)
        || (close_index < range.first) || (close_index >= range.end)
        || !token_is_close_delimiter(&tokenization->tokens[close_index])) {
        return -1;
    }

    token_delimiter_stack_init(&stack, close_index - range.first + 1);
    for (int32 i = close_index; i >= range.first; i -= 1) {
        if (!token_delimiter_stack_consume_reverse(&stack,
                                                   &tokenization->tokens[i])) {
            break;
        }
        if (stack.count == 0) {
            result = i;
            break;
        }
    }
    token_delimiter_stack_free(&stack);
    return result;
}

static bool
token_at_depth_matches(Token *token, bool match_text, enum TokenKind kind,
                       char *text, int32 text_len) {
    if (match_text) {
        return TOKEN_IS(token, text, text_len);
    }
    return token->kind == kind;
}

static int32
token_range_next_at_depth(Tokenization *tokenization, TokenRange range,
                          int32 token_index, TokenDelimiterDepth target,
                          bool match_text, enum TokenKind kind, char *text,
                          int32 text_len) {
    TokenDelimiterStack stack;
    TokenDelimiterDepth depth = {0};
    int32 start;
    int32 result;

    if (!token_range_is_valid(tokenization, range)
        || !token_delimiter_depth_is_valid(target)
        || (match_text && (text_len < 0))) {
        return -1;
    }

    start = token_range_next_start(range, token_index);
    result = range.end;
    token_delimiter_stack_init(&stack, range.end - range.first);
    for (int32 i = range.first; i < range.end; i += 1) {
        Token *token = &tokenization->tokens[i];
        bool matches;

        matches = (i >= start) && token_delimiter_depth_equal(depth, target)
                  && token_at_depth_matches(token, match_text, kind,
                                            text, text_len);
        if (!token_delimiter_stack_consume_forward(&stack, &depth, token)) {
            result = -1;
            break;
        }
        if (matches) {
            result = i;
            break;
        }
    }
    token_delimiter_stack_free(&stack);
    return result;
}

static int32
token_range_previous_at_depth(Tokenization *tokenization, TokenRange range,
                              int32 token_index, TokenDelimiterDepth target,
                              bool match_text, enum TokenKind kind, char *text,
                              int32 text_len) {
    TokenDelimiterStack stack;
    TokenDelimiterDepth depth = {0};
    int32 limit;
    int32 result;

    if (!token_range_is_valid(tokenization, range)
        || !token_delimiter_depth_is_valid(target)
        || (match_text && (text_len < 0))) {
        return -1;
    }

    limit = token_range_previous_start(range, token_index);
    result = range.first - 1;
    if (limit < range.first) {
        return result;
    }

    token_delimiter_stack_init(&stack, range.end - range.first);
    for (int32 i = range.first; i <= limit; i += 1) {
        Token *token = &tokenization->tokens[i];
        bool matches;

        matches = token_delimiter_depth_equal(depth, target)
                  && token_at_depth_matches(token, match_text, kind,
                                            text, text_len);
        if (!token_delimiter_stack_consume_forward(&stack, &depth, token)) {
            result = -1;
            break;
        }
        if (matches) {
            result = i;
        }
    }
    token_delimiter_stack_free(&stack);
    return result;
}

int32
token_range_next_kind_at_depth(Tokenization *tokenization, TokenRange range,
                               int32 token_index, TokenDelimiterDepth depth,
                               enum TokenKind kind) {
    return token_range_next_at_depth(tokenization, range, token_index, depth,
                                     false, kind, NULL, 0);
}

int32
token_range_next_text_at_depth(Tokenization *tokenization, TokenRange range,
                               int32 token_index, TokenDelimiterDepth depth,
                               char *text, int32 text_len) {
    return token_range_next_at_depth(tokenization, range, token_index, depth,
                                     true, TOKEN_UNKNOWN, text, text_len);
}

int32
token_range_previous_kind_at_depth(Tokenization *tokenization,
                                   TokenRange range, int32 token_index,
                                   TokenDelimiterDepth depth,
                                   enum TokenKind kind) {
    return token_range_previous_at_depth(tokenization, range, token_index,
                                         depth, false, kind, NULL, 0);
}

int32
token_range_previous_text_at_depth(Tokenization *tokenization,
                                   TokenRange range, int32 token_index,
                                   TokenDelimiterDepth depth, char *text,
                                   int32 text_len) {
    return token_range_previous_at_depth(tokenization, range, token_index,
                                         depth, true, TOKEN_UNKNOWN,
                                         text, text_len);
}

bool
token_range_significant_equal(Tokenization *left, TokenRange left_range,
                              Tokenization *right, TokenRange right_range) {
    int32 left_i;
    int32 right_i;

    if (!token_range_is_valid(left, left_range)
        || !token_range_is_valid(right, right_range)) {
        return false;
    }

    left_i = token_range_next_significant(left, left_range,
                                          left_range.first - 1);
    right_i = token_range_next_significant(right, right_range,
                                           right_range.first - 1);
    while ((left_i < left_range.end) && (right_i < right_range.end)) {
        Token *left_token = &left->tokens[left_i];
        Token *right_token = &right->tokens[right_i];

        if ((left_token->kind != right_token->kind)
            || !TOKEN_IS(left_token, right_token->text, right_token->len)) {
            return false;
        }
        left_i = token_range_next_significant(left, left_range, left_i);
        right_i = token_range_next_significant(right, right_range, right_i);
    }
    return (left_i == left_range.end) && (right_i == right_range.end);
}

bool
token_range_is_empty(Tokenization *tokenization, TokenRange range) {
    if (!token_range_is_valid(tokenization, range)) {
        return false;
    }
    return token_range_first_significant(tokenization, range) < 0;
}

TokenRange
token_range_trim_trivia(Tokenization *tokenization, TokenRange range) {
    TokenRange result = {-1, -1};
    int32 first;
    int32 last;

    if (!token_range_is_valid(tokenization, range)) {
        return result;
    }
    first = token_range_first_significant(tokenization, range);
    if (first < 0) {
        result.first = range.end;
        result.end = range.end;
        return result;
    }
    last = token_range_last_significant(tokenization, range);
    result.first = first;
    result.end = last + 1;
    return result;
}

SourceRange
token_range_source_range(Tokenization *tokenization, TokenRange range) {
    SourceRange result = {-1, -1};
    Token *first;
    Token *last;

    if (!token_range_is_valid(tokenization, range)) {
        return result;
    }
    if (range.first == range.end) {
        if (range.first == tokenization->token_count) {
            result.start = tokenization->text_len;
        } else {
            result.start = tokenization->tokens[range.first].offset;
        }
        result.end = result.start;
        return result;
    }

    first = &tokenization->tokens[range.first];
    last = &tokenization->tokens[range.end - 1];
    result.start = first->offset;
    result.end = last->offset + last->len;
    return result;
}

static SourceRange
tokenization_source_between(Tokenization *tokenization, int32 left_index,
                            int32 right_index) {
    SourceRange result = {-1, -1};
    Token *left;
    Token *right;

    if ((tokenization == NULL) || (left_index < 0)
        || (right_index <= left_index)
        || (right_index >= tokenization->token_count)) {
        return result;
    }
    left = &tokenization->tokens[left_index];
    right = &tokenization->tokens[right_index];
    result.start = left->offset + left->len;
    result.end = right->offset;
    if (result.start > result.end) {
        result = (SourceRange){-1, -1};
    }
    return result;
}

bool
tokenization_newline_between(Tokenization *tokenization, int32 left_index,
                             int32 right_index) {
    SourceRange source;

    source = tokenization_source_between(tokenization, left_index, right_index);
    if (source.start < 0) {
        return false;
    }
    for (int32 i = source.start; i < source.end; i += 1) {
        if (tokenization->text[i] == '\n') {
            return true;
        }
    }
    return false;
}

bool
tokenization_comment_between(Tokenization *tokenization, int32 left_index,
                             int32 right_index) {
    if ((tokenization == NULL) || (left_index < 0)
        || (right_index <= left_index)
        || (right_index >= tokenization->token_count)) {
        return false;
    }
    for (int32 i = left_index + 1; i < right_index; i += 1) {
        if (tokenization->tokens[i].kind == TOKEN_COMMENT) {
            return true;
        }
    }
    return false;
}

bool
tokenization_blank_line_between(Tokenization *tokenization, int32 left_index,
                                int32 right_index) {
    SourceRange source;
    int32 i;

    source = tokenization_source_between(tokenization, left_index, right_index);
    if (source.start < 0) {
        return false;
    }

    i = source.start;
    while ((i < source.end) && (tokenization->text[i] != '\n')) {
        i += 1;
    }
    while (i < source.end) {
        bool blank = true;

        i += 1;
        while ((i < source.end) && (tokenization->text[i] != '\n')) {
            if (!char_is_horizontal_space(tokenization->text[i])) {
                blank = false;
            }
            i += 1;
        }
        if ((i < source.end) && blank) {
            return true;
        }
    }
    return false;
}

bool
tokenization_line_continuation_between(Tokenization *tokenization,
                                       int32 left_index,
                                       int32 right_index) {
    SourceRange source;

    source = tokenization_source_between(tokenization, left_index, right_index);
    if (source.start < 0) {
        return false;
    }
    for (int32 i = source.start; (i + 1) < source.end; i += 1) {
        if ((tokenization->text[i] == '\\')
            && (tokenization->text[i + 1] == '\n')) {
            return true;
        }
        if ((tokenization->text[i] == '\\')
            && ((i + 2) < source.end)
            && (tokenization->text[i + 1] == '\r')
            && (tokenization->text[i + 2] == '\n')) {
            return true;
        }
    }
    return false;
}

static void
tokenization_build_line_index(Tokenization *tokenization) {
    int32 line_count;
    int32 line_index;

    if (tokenization->line_starts != NULL) {
        return;
    }

    line_count = 1;
    for (int32 i = 0; i < tokenization->text_len; i += 1) {
        if (tokenization->text[i] == '\n') {
            line_count += 1;
        }
    }

    tokenization->line_starts = malloc2(line_count
                                        *SIZEOF(*tokenization->line_starts));
    tokenization->line_count = line_count;
    tokenization->line_capacity = line_count;
    tokenization->line_starts[0] = 0;

    line_index = 1;
    for (int32 i = 0; i < tokenization->text_len; i += 1) {
        if (tokenization->text[i] == '\n') {
            tokenization->line_starts[line_index] = i + 1;
            line_index += 1;
        }
    }
    ASSERT_EQ(line_index, line_count);
    return;
}

int32
tokenization_physical_line_count(Tokenization *tokenization) {
    if (tokenization == NULL) {
        return 0;
    }
    tokenization_build_line_index(tokenization);
    return tokenization->line_count;
}

int32
tokenization_physical_line_start_offset(Tokenization *tokenization,
                                          int32 line) {
    if (tokenization == NULL) {
        return -1;
    }
    tokenization_build_line_index(tokenization);
    if ((line <= 0) || (line > tokenization->line_count)) {
        return -1;
    }
    return tokenization->line_starts[line - 1];
}

int32
tokenization_physical_line_end_offset(Tokenization *tokenization, int32 line) {
    if (tokenization == NULL) {
        return -1;
    }
    tokenization_build_line_index(tokenization);
    if ((line <= 0) || (line > tokenization->line_count)) {
        return -1;
    }
    if (line == tokenization->line_count) {
        return tokenization->text_len;
    }
    return tokenization->line_starts[line];
}

SourceLocation
tokenization_source_location(Tokenization *tokenization, int32 offset) {
    SourceLocation result = {.offset = -1};
    int32 first;
    int32 end;

    if ((tokenization == NULL) || (offset < 0)
        || (offset > tokenization->text_len)) {
        return result;
    }
    tokenization_build_line_index(tokenization);

    first = 0;
    end = tokenization->line_count;
    while ((first + 1) < end) {
        int32 middle = first + (end - first)/2;

        if (tokenization->line_starts[middle] <= offset) {
            first = middle;
        } else {
            end = middle;
        }
    }

    result.offset = offset;
    result.line = first + 1;
    result.column = offset - tokenization->line_starts[first];
    return result;
}

SourceLocation
tokenization_token_location(Tokenization *tokenization, int32 token_index) {
    SourceLocation result = {.offset = -1};
    int32 offset;

    if ((tokenization == NULL) || (token_index < 0)
        || (token_index >= tokenization->token_count)) {
        return result;
    }
    offset = tokenization->tokens[token_index].offset;
    return tokenization_source_location(tokenization, offset);
}

int32
tokenization_significant_at_or_after(Tokenization *tokenization,
                                     int32 token_index) {
    int32 result = token_index;
    ASSERT_GE(token_index, 0);
    while ((result < tokenization->token_count)
           && token_is_trivia(&tokenization->tokens[result])) {
        result += 1;
    }
    return result;
}

int32
tokenization_next_significant(Tokenization *tokenization, int32 token_index) {
    return tokenization_significant_at_or_after(tokenization, token_index + 1);
}

int32
tokenization_previous_significant(Tokenization *tokenization,
                                  int32 token_index) {
    int32 result;

    result = token_index - 1;
    while ((result >= 0) && token_is_trivia(&tokenization->tokens[result])) {
        result -= 1;
    }
    return result;
}

int32
tokenization_token_at_or_after_offset(Tokenization *tokenization,
                                      int32 offset) {
    for (int32 i = 0; i < tokenization->token_count; i += 1) {
        Token *token = &tokenization->tokens[i];

        if (offset < token->offset + token->len) {
            return i;
        }
    }
    return tokenization->token_count;
}

int32
tokenization_logical_line_start_offset(Tokenization *tokenization,
                                       int32 offset) {
    int32 result;

    result = offset;
    while (result > 0) {
        int32 newline;
        int32 previous;

        newline = result - 1;
        while ((newline >= 0) && (tokenization->text[newline] != '\n')) {
            newline -= 1;
        }
        result = newline + 1;
        if (newline < 0) {
            break;
        }
        previous = newline - 1;
        if ((previous >= 0) && (tokenization->text[previous] == '\r')) {
            previous -= 1;
        }
        if ((previous < 0) || (tokenization->text[previous] != '\\')) {
            break;
        }
        result = previous;
    }
    return result;
}

bool
tokenization_is_in_preprocessor_define(Tokenization *tokenization,
                                       int32 token_index) {
    int32 line_start_offset;
    int32 offset;
    int32 i;

    if ((token_index < 0) || (token_index >= tokenization->token_count)) {
        return false;
    }
    offset = tokenization->tokens[token_index].offset;
    line_start_offset = tokenization_logical_line_start_offset(tokenization,
                                                               offset);
    i = tokenization_token_at_or_after_offset(tokenization, line_start_offset);
    while ((i < tokenization->token_count)
           && ((tokenization->tokens[i].kind == TOKEN_SPACE)
               || (tokenization->tokens[i].kind == TOKEN_COMMENT))) {
        i += 1;
    }
    if ((i >= tokenization->token_count)
        || !TOKEN_IS(&tokenization->tokens[i], "#")) {
        return false;
    }
    i += 1;
    while ((i < tokenization->token_count)
           && ((tokenization->tokens[i].kind == TOKEN_SPACE)
               || (tokenization->tokens[i].kind == TOKEN_COMMENT))) {
        i += 1;
    }
    return (i < tokenization->token_count)
           && TOKEN_IS(&tokenization->tokens[i], "define");
}

int32
tokenization_find_matching(Tokenization *tokenization, int32 open_index) {
    char *close;
    Token *open;
    int32 depth;

    if ((open_index < 0) || (open_index >= tokenization->token_count)) {
        return -1;
    }

    open = &tokenization->tokens[open_index];
    close = NULL;
    if (TOKEN_IS(&tokenization->tokens[open_index], "(")) {
        close = ")";
    } else if (TOKEN_IS(&tokenization->tokens[open_index], "[")) {
        close = "]";
    } else if (TOKEN_IS(&tokenization->tokens[open_index], "{")) {
        close = "}";
    }
    if (close == NULL) {
        return -1;
    }

    depth = 0;
    for (int32 i = open_index; i < tokenization->token_count; i += 1) {
        Token *token;

        token = &tokenization->tokens[i];
        if (TOKEN_IS(token, open->text, open->len)) {
            depth += 1;
        } else if (TOKEN_IS(token, close)) {
            depth -= 1;
            if (depth == 0) {
                return i;
            }
        }
    }
    return -1;
}

Tokenization
tokenize(char *text, int32 text_len) {
    return tokenize_with_flags(text, text_len, TOKENIZE_DEFAULT);
}

void
free_tokenization(Tokenization *tokenization) {
    if (tokenization == NULL) {
        return;
    }

    free2(tokenization->tokens,
          tokenization->token_capacity*SIZEOF(*tokenization->tokens));
    free2(tokenization->line_starts,
          tokenization->line_capacity*SIZEOF(*tokenization->line_starts));

    tokenization->tokens = NULL;
    tokenization->token_count = 0;
    tokenization->token_capacity = 0;
    tokenization->text = NULL;
    tokenization->text_len = 0;
    tokenization->line_starts = NULL;
    tokenization->line_count = 0;
    tokenization->line_capacity = 0;

    return;
}

#if 0 == TESTING_meta_tokenize
static inline void
meta_tokenize_sink(void) {
    (void)meta_tokenize_sink;
    (void)free_tokenization;
    (void)token_is_number;
    (void)token_is_open_delimiter;
    (void)token_is_close_delimiter;
    (void)token_delimiter_depth_equal;
    (void)token_delimiter_depth_is_zero;
    (void)token_range_delimiter_depth_before;
    (void)token_range_is_balanced;
    (void)token_range_is_empty;
    (void)token_range_matching_delimiter_forward;
    (void)token_range_matching_delimiter_reverse;
    (void)token_range_next_kind_at_depth;
    (void)token_range_next_text_at_depth;
    (void)token_range_previous_kind_at_depth;
    (void)token_range_previous_text_at_depth;
    (void)token_range_source_range;
    (void)token_range_trim_trivia;
    (void)tokenization_find_matching;
    (void)tokenization_is_in_preprocessor_define;
    (void)tokenization_next_significant;
    (void)tokenization_physical_line_count;
    (void)tokenization_physical_line_end_offset;
    (void)tokenization_physical_line_start_offset;
    (void)tokenization_previous_significant;
    (void)tokenization_source_location;
    (void)tokenization_token_location;
    (void)tokenize;
}
#endif

#if TESTING_meta_tokenize
#define CBASE_IMPLEMENT
#include "cbase.h"

static void
test_assert_token(Token *token, enum TokenKind kind, char *text, int32 column) {
    ASSERT(token->kind == kind);
    ASSERT(TOKEN_IS(token, text));
    ASSERT_EQ(token->len, strlen32(text));
    ASSERT_EQ(token->column, column);
    ASSERT_EQ(token->offset, column);
    return;
}

static int32
test_find_token(Tokenization *tokenization, char *text) {
    for (int32 i = 0; i < tokenization->token_count; i += 1) {
        if (TOKEN_IS(&tokenization->tokens[i], text)) {
            return i;
        }
    }
    return -1;
}

static void
test_token_predicates(void) {
    Token op = {.kind = TOKEN_OPERATOR, .text = "+", .len = 1};
    Token punct = {.kind = TOKEN_PUNCT, .text = ";", .len = 1};
    Token literal = {.kind = TOKEN_LITERAL, .text = "1", .len = 1};
    Token comment = {.kind = TOKEN_COMMENT, .text = "// c", .len = 4};
    Token space = {.kind = TOKEN_SPACE, .text = " ", .len = 1};

    ASSERT(op.kind == TOKEN_OPERATOR);
    ASSERT(!(punct.kind == TOKEN_OPERATOR));
    ASSERT(punct.kind == TOKEN_PUNCT);
    ASSERT(!(op.kind == TOKEN_PUNCT));
    ASSERT(literal.kind == TOKEN_LITERAL);
    ASSERT(!(comment.kind == TOKEN_LITERAL));
    ASSERT(comment.kind == TOKEN_COMMENT);
    ASSERT(!(space.kind == TOKEN_COMMENT));
    ASSERT(token_is_number(&literal));
    ASSERT(!(token_is_number(&comment)));
    return;
}

static void
test_character_classifiers(void) {
    ASSERT(char_is_alpha('a'));
    ASSERT(char_is_alpha('Z'));
    ASSERT(!char_is_alpha('_'));
    ASSERT(char_is_digit('0'));
    ASSERT(char_is_digit('9'));
    ASSERT(!char_is_digit('x'));
    ASSERT(char_is_identifier_start('_'));
    ASSERT(char_is_identifier_start('A'));
    ASSERT(!char_is_identifier_start('1'));
    ASSERT(char_is_identifier_body('1'));
    ASSERT(!char_is_identifier_body('-'));
    ASSERT(char_is_horizontal_space('\t'));
    ASSERT(char_is_horizontal_space('\r'));
    ASSERT(!char_is_horizontal_space('\n'));
    ASSERT(char_is_number_body('\''));
    ASSERT(char_is_number_body('.'));
    ASSERT(!char_is_number_body('-'));
    ASSERT(char_is_operator_or_punct('+'));
    ASSERT(char_is_operator_or_punct('}'));
    ASSERT(!char_is_operator_or_punct('a'));
    return;
}

static void
test_scan_number_literal(void) {
    ASSERT_EQ(scan_number_literal(STRLIT("123 "), 0), 3);
    ASSERT_EQ(scan_number_literal(STRLIT(".5f "), 0), 3);
    ASSERT_EQ(scan_number_literal(STRLIT("3.14e+2;"), 0), 7);
    ASSERT_EQ(scan_number_literal(STRLIT("0x1.fp-2,"), 0), 8);
    ASSERT_EQ(scan_number_literal(STRLIT("1'000u"), 0), 6);
    return;
}

static void
test_literal_scanners(void) {
    char *literal = "\"a\\\"b\" tail";

    ASSERT_ZERO(literal_quote_index(STRLIT("'x'"), 0));
    ASSERT_EQ(literal_quote_index(STRLIT("L\"abc\""), 0), 1);
    ASSERT_EQ(literal_quote_index(STRLIT("u8\"abc\""), 0), 2);
    ASSERT_EQ(literal_quote_index(STRLIT("name"), 0), -1);
    ASSERT_EQ(scan_literal_token(literal, strlen32(literal), 0), 6);
    ASSERT_EQ(scan_literal_token(STRLIT("u8\"xy\";"), 0), 6);
    ASSERT_ZERO(scan_literal_token(STRLIT("name"), 0));

    {
        char trailing_escape[] = {'\"', 'a', '\\'};

        ASSERT_EQ(scan_literal_token(trailing_escape,
                                     LENGTH(trailing_escape), 0),
                  LENGTH(trailing_escape));
    }
    return;
}

static void
test_comment_scanners(void) {
    bool in_block_comment = false;

    ASSERT_EQ(scan_line_comment(STRLIT("// abc\nx"), 0), 6);
    ASSERT_EQ(scan_block_comment(STRLIT("/* abc */x"), 0,
                                 &in_block_comment),
              9);
    ASSERT(!in_block_comment);
    ASSERT_EQ(scan_block_comment(STRLIT("/* abc\nx"), 0,
                                 &in_block_comment),
              6);
    ASSERT(in_block_comment);
    ASSERT_EQ(scan_block_comment(STRLIT("continued */"), 0,
                                 &in_block_comment),
              12);
    ASSERT(!in_block_comment);
    return;
}

static void
test_operator_or_punct_category(void) {
    enum TokenKind kind;
    int32 len;

    kind = operator_or_punct_category(STRLIT(">>="), 0, &len);
    ASSERT(kind == TOKEN_OPERATOR);
    ASSERT_EQ(len, 3);

    kind = operator_or_punct_category(STRLIT("..."), 0, &len);
    ASSERT(kind == TOKEN_PUNCT);
    ASSERT_EQ(len, 3);

    kind = operator_or_punct_category(STRLIT("<:"), 0, &len);
    ASSERT(kind == TOKEN_PUNCT);
    ASSERT_EQ(len, 2);

    kind = operator_or_punct_category(STRLIT("&&"), 0, &len);
    ASSERT(kind == TOKEN_OPERATOR);
    ASSERT_EQ(len, 2);

    kind = operator_or_punct_category(STRLIT("("), 0, &len);
    ASSERT(kind == TOKEN_PUNCT);
    ASSERT_EQ(len, 1);
    return;
}

static void
test_line_starts_preprocessor(void) {
    ASSERT(line_starts_preprocessor(STRLIT("  #define XX\n")));
    ASSERT(!line_starts_preprocessor(STRLIT("  int x;\n")));
    ASSERT(!line_starts_preprocessor("", 0));
    return;
}

static void
test_tokenize_default(void) {
    char *text = "int x = foo(1, \"a\"); // c\n";
    Tokenization line = tokenize(text, strlen32(text));

    ASSERT_EQ(line.token_count, 17);
    test_assert_token(&line.tokens[0], TOKEN_IDENT, "int", 0);
    test_assert_token(&line.tokens[1], TOKEN_SPACE, " ", 3);
    test_assert_token(&line.tokens[4], TOKEN_OPERATOR, "=", 6);
    test_assert_token(&line.tokens[6], TOKEN_IDENT, "foo", 8);
    test_assert_token(&line.tokens[7], TOKEN_PUNCT, "(", 11);
    test_assert_token(&line.tokens[8], TOKEN_LITERAL, "1", 12);
    test_assert_token(&line.tokens[10], TOKEN_SPACE, " ", 14);
    test_assert_token(&line.tokens[11], TOKEN_LITERAL, "\"a\"", 15);
    test_assert_token(&line.tokens[14], TOKEN_SPACE, " ", 20);
    test_assert_token(&line.tokens[15], TOKEN_COMMENT, "// c", 21);
    test_assert_token(&line.tokens[16], TOKEN_NEWLINE, "\n", 25);
    free_tokenization(&line);
    return;
}

static void
test_tokenize_first_byte_dispatch(void) {
    char *text = "u user UPPER Lvalue u8name u\"x\" U'x' L\"z\" .5 . + @";
    Tokenization line = tokenize(text, strlen32(text));

    test_assert_token(&line.tokens[0], TOKEN_IDENT, "u", 0);
    test_assert_token(&line.tokens[2], TOKEN_IDENT, "user", 2);
    test_assert_token(&line.tokens[4], TOKEN_IDENT, "UPPER", 7);
    test_assert_token(&line.tokens[6], TOKEN_IDENT, "Lvalue", 13);
    test_assert_token(&line.tokens[8], TOKEN_IDENT, "u8name", 20);
    test_assert_token(&line.tokens[10], TOKEN_LITERAL, "u\"x\"", 27);
    test_assert_token(&line.tokens[12], TOKEN_LITERAL, "U'x'", 32);
    test_assert_token(&line.tokens[14], TOKEN_LITERAL, "L\"z\"", 37);
    test_assert_token(&line.tokens[16], TOKEN_LITERAL, ".5", 42);
    test_assert_token(&line.tokens[18], TOKEN_PUNCT, ".", 45);
    test_assert_token(&line.tokens[20], TOKEN_OPERATOR, "+", 47);
    test_assert_token(&line.tokens[22], TOKEN_UNKNOWN, "@", 49);

    free_tokenization(&line);
    return;
}

static void
test_tokenize_preprocessor_and_skip_whitespace(void) {
    char *preproc_text = "  #include \"x\"\n";
    char *skip_text = "a b\n";
    Tokenization line;
    Tokenization skipped;

    line = tokenize_with_flags(preproc_text, strlen32(preproc_text),
                               TOKENIZE_PREPROCESSOR_LINES);
    ASSERT_EQ(line.token_count, 1);
    test_assert_token(&line.tokens[0], TOKEN_PREPROC, preproc_text, 0);
    ASSERT(line.text == preproc_text);
    ASSERT_EQ(line.text_len, strlen32(preproc_text));
    free_tokenization(&line);

    skipped = tokenize_with_flags(skip_text, strlen32(skip_text),
                                  TOKENIZE_SKIP_WHITESPACE);
    ASSERT_EQ(skipped.token_count, 2);
    test_assert_token(&skipped.tokens[0], TOKEN_IDENT, "a", 0);
    test_assert_token(&skipped.tokens[1], TOKEN_IDENT, "b", 2);
    free_tokenization(&skipped);
    return;
}

static void
test_tokenize_block_comment_across_lines(void) {
    char *text = "/* hello\nworld */ int x;";
    char *preproc_text = "/* hello\n# still a comment */ int x;\n";
    Tokenization tokenization;

    tokenization = tokenize(text, strlen32(text));
    ASSERT_EQ(tokenization.token_count, 8);
    test_assert_token(&tokenization.tokens[0], TOKEN_COMMENT, "/* hello", 0);
    test_assert_token(&tokenization.tokens[1], TOKEN_NEWLINE, "\n", 8);
    test_assert_token(&tokenization.tokens[2], TOKEN_COMMENT, "world */", 9);
    test_assert_token(&tokenization.tokens[4], TOKEN_IDENT, "int", 18);
    free_tokenization(&tokenization);

    tokenization = tokenize_with_flags(preproc_text, strlen32(preproc_text),
                                       TOKENIZE_PREPROCESSOR_LINES);
    ASSERT_EQ(tokenization.token_count, 9);
    test_assert_token(&tokenization.tokens[0], TOKEN_COMMENT, "/* hello", 0);
    test_assert_token(&tokenization.tokens[1], TOKEN_NEWLINE, "\n", 8);
    test_assert_token(&tokenization.tokens[2], TOKEN_COMMENT,
                      "# still a comment */", 9);
    test_assert_token(&tokenization.tokens[4], TOKEN_IDENT, "int", 30);
    free_tokenization(&tokenization);

    tokenization = tokenize_with_flags(text, strlen32(text),
                                       TOKENIZE_SKIP_WHITESPACE);
    ASSERT_EQ(tokenization.token_count, 5);
    test_assert_token(&tokenization.tokens[0], TOKEN_COMMENT, "/* hello", 0);
    test_assert_token(&tokenization.tokens[1], TOKEN_COMMENT, "world */", 9);
    test_assert_token(&tokenization.tokens[2], TOKEN_IDENT, "int", 18);
    free_tokenization(&tokenization);
    return;
}

static void
test_tokenization_navigation(void) {
    char *text = "a /*c*/ + \n b";
    Tokenization tokenization;

    tokenization = tokenize(text, strlen32(text));
    ASSERT_EQ(tokenization.token_count, 9);
    ASSERT_EQ(tokenization_significant_at_or_after(&tokenization, 1), 4);
    ASSERT_EQ(tokenization_next_significant(&tokenization, 0), 4);
    ASSERT_EQ(tokenization_previous_significant(&tokenization, 8), 4);
    ASSERT_EQ(tokenization_token_at_or_after_offset(&tokenization, 8), 4);
    ASSERT_EQ(tokenization_token_at_or_after_offset(&tokenization,
                                                    strlen32(text)),
              tokenization.token_count);
    ASSERT(token_is_trivia(&tokenization.tokens[1]));
    ASSERT(token_is_trivia(&tokenization.tokens[2]));
    ASSERT(!token_is_trivia(&tokenization.tokens[4]));
    free_tokenization(&tokenization);
    return;
}

static void
test_token_range_navigation(void) {
    char *text = "  a /* c */ +\n b ; c  ";
    Tokenization tokenization;
    TokenRange range;
    int32 a;
    int32 comment;
    int32 plus;
    int32 b;
    int32 semicolon;
    int32 c;

    tokenization = tokenize(text, strlen32(text));
    a = test_find_token(&tokenization, "a");
    comment = test_find_token(&tokenization, "/* c */");
    plus = test_find_token(&tokenization, "+");
    b = test_find_token(&tokenization, "b");
    semicolon = test_find_token(&tokenization, ";");
    c = test_find_token(&tokenization, "c");
    range = (TokenRange){a, semicolon + 1};

    ASSERT_EQ(token_range_next_significant(&tokenization, range, -1), a);
    ASSERT_EQ(token_range_next_significant(&tokenization, range, a), plus);
    ASSERT_EQ(token_range_next_significant(&tokenization, range, plus), b);
    ASSERT_EQ(token_range_next_significant(&tokenization, range, b),
              semicolon);
    ASSERT_EQ(token_range_next_significant(&tokenization, range, semicolon),
              range.end);
    ASSERT_EQ(token_range_next_significant(&tokenization, range, c),
              range.end);

    ASSERT_EQ(token_range_previous_significant(&tokenization, range,
                                               range.end),
              semicolon);
    ASSERT_EQ(token_range_previous_significant(&tokenization, range,
                                               semicolon),
              b);
    ASSERT_EQ(token_range_previous_significant(&tokenization, range, a),
              range.first - 1);
    ASSERT_EQ(token_range_previous_significant(&tokenization, range, -1),
              range.first - 1);

    ASSERT_EQ(token_range_next_kind(&tokenization, range, a, TOKEN_COMMENT),
              comment);
    ASSERT_EQ(token_range_next_kind(&tokenization, range, a, TOKEN_IDENT), b);
    ASSERT_EQ(token_range_next_kind(&tokenization, range, b, TOKEN_IDENT),
              range.end);
    ASSERT_EQ(token_range_previous_kind(&tokenization, range, semicolon,
                                        TOKEN_COMMENT),
              comment);
    ASSERT_EQ(token_range_previous_kind(&tokenization, range, a,
                                        TOKEN_COMMENT),
              range.first - 1);

    ASSERT_EQ(token_range_next_text(&tokenization, range, a, STRLIT("b")), b);
    ASSERT_EQ(token_range_next_text(&tokenization, range, b, STRLIT("c")),
              range.end);
    ASSERT_EQ(token_range_previous_text(&tokenization, range, semicolon,
                                        STRLIT("+")),
              plus);
    ASSERT_EQ(token_range_previous_text(&tokenization, range, plus,
                                        STRLIT("a")),
              a);

    range = (TokenRange){semicolon, semicolon};
    ASSERT_EQ(token_range_next_significant(&tokenization, range, -1),
              range.end);
    ASSERT_EQ(token_range_previous_significant(&tokenization, range,
                                               range.end),
              range.first - 1);
    ASSERT_EQ(token_range_next_kind(&tokenization, range, -1, TOKEN_IDENT),
              range.end);

    range = (TokenRange){-1, semicolon};
    ASSERT_EQ(token_range_next_significant(&tokenization, range, -1), -1);
    ASSERT_EQ(token_range_previous_significant(&tokenization, range,
                                               semicolon),
              -1);
    free_tokenization(&tokenization);
    return;
}

static void
test_token_range_significant_equal(void) {
    char *left_text = "foo (a, /* comment */ b)";
    char *right_text = "  foo(a,b)  ";
    char *different_text = "foo(a,c)";
    Tokenization left;
    Tokenization right;
    Tokenization different;
    TokenRange left_range;
    TokenRange right_range;
    TokenRange different_range;

    left = tokenize(left_text, strlen32(left_text));
    right = tokenize(right_text, strlen32(right_text));
    different = tokenize(different_text, strlen32(different_text));
    left_range = (TokenRange){0, left.token_count};
    right_range = (TokenRange){0, right.token_count};
    different_range = (TokenRange){0, different.token_count};

    ASSERT(token_range_significant_equal(&left, left_range,
                                         &right, right_range));
    ASSERT(!token_range_significant_equal(&left, left_range,
                                          &different, different_range));
    right_range = (TokenRange){0, test_find_token(&right, ")")};
    ASSERT(!token_range_significant_equal(&left, left_range,
                                          &right, right_range));
    ASSERT(!token_range_significant_equal(&left,
                                          (TokenRange){-1, left.token_count},
                                          &right, right_range));

    free_tokenization(&left);
    free_tokenization(&right);
    free_tokenization(&different);
    return;
}

static void
test_tokenization_between_queries(void) {
    char *text = "a /* c */ b\n"
                 "c\n"
                 "   \n"
                 "d \\\n"
                 "  + e\n"
                 "f \\\r\n"
                 "  + g";
    Tokenization tokenization;
    TokenRange all;
    int32 a;
    int32 b;
    int32 c;
    int32 d;
    int32 e;
    int32 f;
    int32 g;

    tokenization = tokenize(text, strlen32(text));
    all = (TokenRange){0, tokenization.token_count};
    a = token_range_next_text(&tokenization, all, -1, STRLIT("a"));
    b = token_range_next_text(&tokenization, all, a, STRLIT("b"));
    c = token_range_next_text(&tokenization, all, b, STRLIT("c"));
    d = token_range_next_text(&tokenization, all, c, STRLIT("d"));
    e = token_range_next_text(&tokenization, all, d, STRLIT("e"));
    f = token_range_next_text(&tokenization, all, e, STRLIT("f"));
    g = token_range_next_text(&tokenization, all, f, STRLIT("g"));

    ASSERT(tokenization_comment_between(&tokenization, a, b));
    ASSERT(!tokenization_newline_between(&tokenization, a, b));
    ASSERT(!tokenization_blank_line_between(&tokenization, a, b));
    ASSERT(!tokenization_line_continuation_between(&tokenization, a, b));

    ASSERT(tokenization_newline_between(&tokenization, b, c));
    ASSERT(!tokenization_blank_line_between(&tokenization, b, c));
    ASSERT(tokenization_blank_line_between(&tokenization, c, d));
    ASSERT(!tokenization_comment_between(&tokenization, c, d));

    ASSERT(tokenization_newline_between(&tokenization, d, e));
    ASSERT(tokenization_line_continuation_between(&tokenization, d, e));
    ASSERT(tokenization_line_continuation_between(&tokenization, f, g));
    ASSERT(!tokenization_blank_line_between(&tokenization, d, e));

    ASSERT(!tokenization_newline_between(&tokenization, -1, b));
    ASSERT(!tokenization_comment_between(&tokenization, b, b));
    ASSERT(!tokenization_blank_line_between(&tokenization, g,
                                            tokenization.token_count));
    ASSERT(!tokenization_line_continuation_between(&tokenization, g,
                                                   tokenization.token_count));
    free_tokenization(&tokenization);

    tokenization = tokenize_with_flags(text, strlen32(text),
                                       TOKENIZE_SKIP_WHITESPACE);
    all = (TokenRange){0, tokenization.token_count};
    a = token_range_next_text(&tokenization, all, -1, STRLIT("a"));
    b = token_range_next_text(&tokenization, all, a, STRLIT("b"));
    c = token_range_next_text(&tokenization, all, b, STRLIT("c"));
    d = token_range_next_text(&tokenization, all, c, STRLIT("d"));
    e = token_range_next_text(&tokenization, all, d, STRLIT("e"));
    ASSERT(tokenization_comment_between(&tokenization, a, b));
    ASSERT(tokenization_newline_between(&tokenization, b, c));
    ASSERT(tokenization_blank_line_between(&tokenization, c, d));
    ASSERT(tokenization_line_continuation_between(&tokenization, d, e));
    free_tokenization(&tokenization);
    return;
}

static void
test_token_ranges(void) {
    char *text = "  foo /* c */ + bar  ";
    Tokenization tokenization;
    TokenRange range;
    TokenRange trimmed;
    SourceRange source;

    tokenization = tokenize(text, strlen32(text));
    range = (TokenRange){0, tokenization.token_count};
    ASSERT(token_range_is_valid(&tokenization, range));
    ASSERT(!token_range_is_valid(&tokenization, (TokenRange){-1, 0}));
    range = (TokenRange){0, tokenization.token_count + 1};
    ASSERT(!token_range_is_valid(&tokenization, range));
    range = (TokenRange){0, tokenization.token_count};
    ASSERT_EQ(token_range_first_significant(&tokenization, range), 1);
    ASSERT_EQ(token_range_last_significant(&tokenization, range),
              tokenization.token_count - 2);

    trimmed = token_range_trim_trivia(&tokenization, range);
    ASSERT_EQ(trimmed.first, 1);
    ASSERT_EQ(trimmed.end, tokenization.token_count - 1);
    source = token_range_source_range(&tokenization, trimmed);
    ASSERT_EQ(source.start, 2);
    ASSERT_EQ(source.end, strlen32(text) - 2);
    ASSERT_EQ(source.end - source.start,
              strlen32("foo /* c */ + bar"));
    ASSERT_EQ(tokenization.text + source.start, source.end - source.start,
              "foo /* c */ + bar");

    range = (TokenRange){0, 1};
    ASSERT(token_range_is_empty(&tokenization, range));
    trimmed = token_range_trim_trivia(&tokenization, range);
    ASSERT_EQ(trimmed.first, 1);
    ASSERT_EQ(trimmed.end, 1);
    source = token_range_source_range(&tokenization, trimmed);
    ASSERT_EQ(source.start, 2);
    ASSERT_EQ(source.end, 2);

    range = (TokenRange){tokenization.token_count,
                         tokenization.token_count};
    source = token_range_source_range(&tokenization, range);
    ASSERT_EQ(source.start, tokenization.text_len);
    ASSERT_EQ(source.end, tokenization.text_len);
    free_tokenization(&tokenization);
    return;
}

static void
test_tokenization_source_locations(void) {
    char *text = "abc\ndef\r\nlast\n";
    Tokenization tokenization;
    SourceLocation location;
    int32 def_token;
    int32 last_token;

    tokenization = tokenize(text, strlen32(text));
    ASSERT_NULL(tokenization.line_starts);
    ASSERT_ZERO(tokenization.line_count);
    ASSERT_EQ(tokenization_physical_line_count(&tokenization), 4);
    ASSERT(tokenization.line_starts != NULL);
    ASSERT_EQ(tokenization_physical_line_start_offset(&tokenization, 1), 0);
    ASSERT_EQ(tokenization_physical_line_end_offset(&tokenization, 1), 4);
    ASSERT_EQ(tokenization_physical_line_start_offset(&tokenization, 2), 4);
    ASSERT_EQ(tokenization_physical_line_end_offset(&tokenization, 2), 9);
    ASSERT_EQ(tokenization_physical_line_start_offset(&tokenization, 4),
              tokenization.text_len);
    ASSERT_EQ(tokenization_physical_line_end_offset(&tokenization, 4),
              tokenization.text_len);
    ASSERT_EQ(tokenization_physical_line_start_offset(&tokenization, 0), -1);
    ASSERT_EQ(tokenization_physical_line_end_offset(&tokenization, 5), -1);

    location = tokenization_source_location(&tokenization, 0);
    ASSERT_ZERO(location.offset);
    ASSERT_EQ(location.line, 1);
    ASSERT_ZERO(location.column);
    location = tokenization_source_location(&tokenization, 3);
    ASSERT_EQ(location.line, 1);
    ASSERT_EQ(location.column, 3);
    location = tokenization_source_location(&tokenization, 4);
    ASSERT_EQ(location.line, 2);
    ASSERT_ZERO(location.column);
    location = tokenization_source_location(&tokenization, 8);
    ASSERT_EQ(location.line, 2);
    ASSERT_EQ(location.column, 4);
    location = tokenization_source_location(&tokenization, 9);
    ASSERT_EQ(location.line, 3);
    ASSERT_ZERO(location.column);
    location = tokenization_source_location(&tokenization,
                                            tokenization.text_len);
    ASSERT_EQ(location.line, 4);
    ASSERT_ZERO(location.column);
    location = tokenization_source_location(&tokenization, -1);
    ASSERT_EQ(location.offset, -1);
    ASSERT_ZERO(location.line);
    ASSERT_ZERO(location.column);

    def_token = test_find_token(&tokenization, "def");
    last_token = test_find_token(&tokenization, "last");
    location = tokenization_token_location(&tokenization, def_token);
    ASSERT_EQ(location.line, 2);
    ASSERT_ZERO(location.column);
    location = tokenization_token_location(&tokenization, last_token);
    ASSERT_EQ(location.line, 3);
    ASSERT_ZERO(location.column);
    location = tokenization_token_location(&tokenization, -1);
    ASSERT_EQ(location.offset, -1);

    free_tokenization(&tokenization);
    ASSERT_NULL(tokenization.line_starts);
    ASSERT_ZERO(tokenization.line_count);
    ASSERT_ZERO(tokenization.line_capacity);
    return;
}

static void
test_tokenization_empty_source_location(void) {
    char *text = "";
    Tokenization tokenization;
    SourceLocation location;

    tokenization = tokenize(text, 0);
    ASSERT_EQ(tokenization_physical_line_count(&tokenization), 1);
    ASSERT_ZERO(tokenization_physical_line_start_offset(&tokenization, 1));
    ASSERT_ZERO(tokenization_physical_line_end_offset(&tokenization, 1));
    location = tokenization_source_location(&tokenization, 0);
    ASSERT_ZERO(location.offset);
    ASSERT_EQ(location.line, 1);
    ASSERT_ZERO(location.column);
    free_tokenization(&tokenization);
    return;
}

static void
test_tokenization_preprocessor_define_detection(void) {
    char *text = "#define XX(a) \\\n    ((a) + 1)\nint z;\n";
    Tokenization tokenization;
    int32 plus;
    int32 int_token;
    int32 offset;

    tokenization = tokenize(text, strlen32(text));
    plus = test_find_token(&tokenization, "+");
    int_token = test_find_token(&tokenization, "int");
    ASSERT_GE(plus, 0);
    ASSERT_GE(int_token, 0);
    offset = tokenization.tokens[plus].offset;
    ASSERT_ZERO(tokenization_logical_line_start_offset(&tokenization, offset));
    ASSERT(tokenization_is_in_preprocessor_define(&tokenization, plus));
    ASSERT(!tokenization_is_in_preprocessor_define(&tokenization, int_token));
    ASSERT(!tokenization_is_in_preprocessor_define(&tokenization, -1));
    free_tokenization(&tokenization);
    return;
}

static void
test_token_range_balanced_delimiters(void) {
    char *text = "call(a[2], (b + (C){.x = 1}));";
    Tokenization tokenization;
    TokenRange all;
    TokenRange truncated;
    int32 outer;
    int32 bracket;
    int32 nested;
    int32 brace;
    int32 close;

    tokenization = tokenize(text, strlen32(text));
    all = (TokenRange){0, tokenization.token_count};
    outer = token_range_next_text(&tokenization, all, -1, STRLIT("("));
    bracket = token_range_next_text(&tokenization, all, outer, STRLIT("["));
    nested = token_range_next_text(&tokenization, all, bracket, STRLIT("("));
    nested = token_range_next_text(&tokenization, all, nested, STRLIT("("));
    brace = token_range_next_text(&tokenization, all, nested, STRLIT("{"));

    ASSERT(token_is_open_delimiter(&tokenization.tokens[outer]));
    ASSERT(token_is_open_delimiter(&tokenization.tokens[bracket]));
    ASSERT(!token_is_open_delimiter(&tokenization.tokens[outer + 1]));
    close = token_range_matching_delimiter_forward(&tokenization, all, outer);
    ASSERT_GE(close, 0);
    ASSERT(TOKEN_IS(&tokenization.tokens[close], ")"));
    ASSERT_EQ(token_range_matching_delimiter_reverse(&tokenization, all, close),
              outer);

    close = token_range_matching_delimiter_forward(&tokenization, all,
                                                   bracket);
    ASSERT(TOKEN_IS(&tokenization.tokens[close], "]"));
    ASSERT_EQ(token_range_matching_delimiter_reverse(&tokenization, all, close),
              bracket);
    close = token_range_matching_delimiter_forward(&tokenization, all, nested);
    ASSERT(TOKEN_IS(&tokenization.tokens[close], ")"));
    close = token_range_matching_delimiter_forward(&tokenization, all, brace);
    ASSERT(TOKEN_IS(&tokenization.tokens[close], "}"));
    ASSERT(token_range_is_balanced(&tokenization, all));

    close = token_range_matching_delimiter_forward(&tokenization, all, outer);
    truncated = (TokenRange){outer, close};
    ASSERT_EQ(token_range_matching_delimiter_forward(&tokenization, truncated,
                                                     outer),
              -1);
    ASSERT_EQ(token_range_matching_delimiter_forward(&tokenization, all,
                                                     outer + 1),
              -1);
    ASSERT_EQ(token_range_matching_delimiter_reverse(&tokenization, all,
                                                     outer),
              -1);
    free_tokenization(&tokenization);
    return;
}

static void
test_token_range_malformed_delimiters(void) {
    char *text = "([)]";
    Tokenization tokenization;
    TokenRange all;
    TokenDelimiterDepth depth;
    int32 paren;
    int32 bracket;
    int32 close_bracket;

    tokenization = tokenize(text, strlen32(text));
    all = (TokenRange){0, tokenization.token_count};
    paren = token_range_next_text(&tokenization, all, -1, STRLIT("("));
    bracket = token_range_next_text(&tokenization, all, paren, STRLIT("["));
    close_bracket = token_range_next_text(&tokenization, all, bracket,
                                          STRLIT("]"));
    ASSERT(!token_range_is_balanced(&tokenization, all));
    ASSERT_EQ(token_range_matching_delimiter_forward(&tokenization, all,
                                                     paren),
              -1);
    ASSERT_EQ(token_range_matching_delimiter_forward(&tokenization, all,
                                                     bracket),
              -1);
    ASSERT_EQ(token_range_matching_delimiter_reverse(&tokenization, all,
                                                     close_bracket),
              -1);
    ASSERT(!token_range_delimiter_depth_before(&tokenization, all, all.end,
                                               &depth));
    free_tokenization(&tokenization);

    text = "(a[0]) (";
    tokenization = tokenize(text, strlen32(text));
    all = (TokenRange){0, tokenization.token_count};
    paren = token_range_next_text(&tokenization, all, -1, STRLIT("("));
    ASSERT_GE(token_range_matching_delimiter_forward(&tokenization, all,
                                                     paren),
              0);
    paren = token_range_next_text(&tokenization, all, paren, STRLIT("("));
    ASSERT_EQ(token_range_matching_delimiter_forward(&tokenization, all,
                                                     paren),
              -1);
    ASSERT(!token_range_is_balanced(&tokenization, all));
    ASSERT(token_range_delimiter_depth_before(&tokenization, all, all.end,
                                              &depth));
    ASSERT_EQ(depth.paren, 1);
    ASSERT_ZERO(depth.bracket);
    ASSERT_ZERO(depth.brace);
    free_tokenization(&tokenization);

    text = ")";
    tokenization = tokenize(text, strlen32(text));
    all = (TokenRange){0, tokenization.token_count};
    ASSERT(!token_range_is_balanced(&tokenization, all));
    ASSERT(!token_range_delimiter_depth_before(&tokenization, all, all.end,
                                               &depth));
    free_tokenization(&tokenization);
    return;
}

static void
test_token_range_delimiter_depth(void) {
    char *text = "a, foo(b, c[1, 2]), d;";
    Tokenization tokenization;
    TokenRange all;
    TokenRange args;
    TokenDelimiterDepth depth;
    TokenDelimiterDepth zero = {0};
    TokenDelimiterDepth paren_depth = {.paren = 1};
    int32 foo;
    int32 open;
    int32 close;
    int32 b;
    int32 one;
    int32 comma1;
    int32 comma2;
    int32 inner_comma;
    int32 a;
    int32 d;

    tokenization = tokenize(text, strlen32(text));
    all = (TokenRange){0, tokenization.token_count};
    foo = token_range_next_text(&tokenization, all, -1, STRLIT("foo"));
    open = token_range_next_text(&tokenization, all, foo, STRLIT("("));
    close = token_range_matching_delimiter_forward(&tokenization, all, open);
    b = token_range_next_text(&tokenization, all, open, STRLIT("b"));
    one = token_range_next_text(&tokenization, all, b, STRLIT("1"));

    ASSERT(token_range_delimiter_depth_before(&tokenization, all, b, &depth));
    ASSERT(token_delimiter_depth_equal(depth, paren_depth));
    ASSERT(token_range_delimiter_depth_before(&tokenization, all, one, &depth));
    ASSERT_EQ(depth.paren, 1);
    ASSERT_EQ(depth.bracket, 1);
    ASSERT_ZERO(depth.brace);
    ASSERT(token_range_delimiter_depth_before(&tokenization, all, all.end,
                                              &depth));
    ASSERT(token_delimiter_depth_is_zero(depth));

    comma1 = token_range_next_text_at_depth(&tokenization, all, -1, zero,
                                            STRLIT(","));
    comma2 = token_range_next_text_at_depth(&tokenization, all, comma1, zero,
                                            STRLIT(","));
    ASSERT_LT_VAR(comma1, foo);
    ASSERT_GT_VAR(comma2, close);
    ASSERT_EQ(token_range_next_text_at_depth(&tokenization, all, comma2, zero,
                                             STRLIT(",")),
              all.end);
    ASSERT_EQ(token_range_previous_text_at_depth(&tokenization, all, all.end,
                                                 zero, STRLIT(",")),
              comma2);
    ASSERT_EQ(token_range_previous_text_at_depth(&tokenization, all, comma2,
                                                 zero, STRLIT(",")),
              comma1);

    inner_comma = token_range_next_text_at_depth(&tokenization, all, open,
                                                 paren_depth, STRLIT(","));
    ASSERT_GT_VAR(inner_comma, b);
    ASSERT_LT_VAR(inner_comma, one);
    ASSERT_EQ(token_range_next_text_at_depth(&tokenization, all, inner_comma,
                                             paren_depth, STRLIT(",")),
              all.end);

    a = token_range_next_kind_at_depth(&tokenization, all, -1, zero,
                                       TOKEN_IDENT);
    d = token_range_previous_kind_at_depth(&tokenization, all, all.end, zero,
                                           TOKEN_IDENT);
    ASSERT(TOKEN_IS(&tokenization.tokens[a], "a"));
    ASSERT(TOKEN_IS(&tokenization.tokens[d], "d"));

    args = (TokenRange){open + 1, close};
    ASSERT(token_range_is_balanced(&tokenization, args));
    inner_comma = token_range_next_text_at_depth(&tokenization, args,
                                                 args.first - 1, zero,
                                                 STRLIT(","));
    ASSERT_GT_VAR(inner_comma, b);
    ASSERT_LT_VAR(inner_comma, one);
    free_tokenization(&tokenization);
    return;
}

static void
test_token_range_deep_delimiters(void) {
    enum { DEPTH = TOKEN_DELIMITER_STACK_LOCAL_CAPACITY + 8 };
    char text[DEPTH*2 + 2];
    Tokenization tokenization;
    TokenRange all;
    int32 close;

    for (int32 i = 0; i < DEPTH; i += 1) {
        text[i] = '(';
        text[DEPTH + i] = ')';
    }
    text[DEPTH*2] = ';';
    text[DEPTH*2 + 1] = '\0';

    tokenization = tokenize(text, DEPTH*2 + 1);
    all = (TokenRange){0, tokenization.token_count};
    ASSERT(token_range_is_balanced(&tokenization, all));
    close = token_range_matching_delimiter_forward(&tokenization, all, 0);
    ASSERT_GE(close, 0);
    ASSERT_EQ(token_range_matching_delimiter_reverse(&tokenization, all, close),
              0);
    free_tokenization(&tokenization);
    return;
}

static void
test_tokenization_find_matching(void) {
    char *text = "(a[2] + (b))";
    Tokenization tokenization;
    int32 paren;
    int32 bracket;
    int32 nested;

    tokenization = tokenize(text, strlen32(text));
    paren = test_find_token(&tokenization, "(");
    bracket = test_find_token(&tokenization, "[");
    nested = 8;
    ASSERT_EQ(tokenization_find_matching(&tokenization, paren), 11);
    ASSERT_EQ(tokenization_find_matching(&tokenization, bracket), 4);
    ASSERT_EQ(tokenization_find_matching(&tokenization, nested), 10);
    ASSERT_EQ(tokenization_find_matching(&tokenization, 1), -1);
    ASSERT_EQ(tokenization_find_matching(&tokenization, -1), -1);
    free_tokenization(&tokenization);
    return;
}

static void
test_tokenize_with_flags_returns_source_metadata(void) {
    char *text = "x + y";
    Tokenization tokenization;

    tokenization = tokenize_with_flags(text, strlen32(text),
                                       TOKENIZE_SKIP_WHITESPACE);
    ASSERT(tokenization.text == text);
    ASSERT_EQ(tokenization.text_len, strlen32(text));
    ASSERT_EQ(tokenization.token_count, 3);
    test_assert_token(&tokenization.tokens[0], TOKEN_IDENT, "x", 0);
    test_assert_token(&tokenization.tokens[1], TOKEN_OPERATOR, "+", 2);
    test_assert_token(&tokenization.tokens[2], TOKEN_IDENT, "y", 4);
    ASSERT(tokenization.tokens[0].text == text);
    ASSERT(tokenization.tokens[1].text == text + 2);
    ASSERT(tokenization.tokens[2].text == text + 4);
    free_tokenization(&tokenization);
    return;
}

int
main(void) {
    test_token_predicates();
    test_character_classifiers();
    test_scan_number_literal();
    test_literal_scanners();
    test_comment_scanners();
    test_operator_or_punct_category();
    test_line_starts_preprocessor();
    test_tokenize_default();
    test_tokenize_first_byte_dispatch();
    test_tokenize_preprocessor_and_skip_whitespace();
    test_tokenize_block_comment_across_lines();
    test_tokenization_navigation();
    test_token_range_navigation();
    test_token_range_significant_equal();
    test_tokenization_between_queries();
    test_token_ranges();
    test_tokenization_source_locations();
    test_tokenization_empty_source_location();
    test_tokenization_preprocessor_define_detection();
    test_token_range_balanced_delimiters();
    test_token_range_malformed_delimiters();
    test_token_range_delimiter_depth();
    test_token_range_deep_delimiters();
    test_tokenization_find_matching();
    test_tokenize_with_flags_returns_source_metadata();
    return 0;
}

#endif /* TESTING_meta_tokenize */

#endif /* META_TOKENIZE_C */
