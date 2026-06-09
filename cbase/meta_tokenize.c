#if !defined(META_TOKENIZE_C)
#define META_TOKENIZE_C

#include "meta.h"
#include "util.c"

#define TOKENIZE_INITIAL_TOKEN_CAPACITY 32

static bool
char_is_alpha(char c) {
    bool result;

    result = false;
    if (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'))) {
        result = true;
    }
    return result;
}

static bool
char_is_digit(char c) {
    bool result;

    result = false;
    if ((c >= '0') && (c <= '9')) {
        result = true;
    }
    return result;
}

static bool
token_is_number(Token *token) {
    bool result;

    result = false;
    if ((token->kind == TOKEN_LITERAL) && (token->len > 0)
    && (char_is_digit(token->text[0])
    || ((token->text[0] == '.') && (token->len > 1)
    && char_is_digit(token->text[1])))) {
        result = true;
    }
    return result;
}

static bool
char_is_identifier_start(char c) {
    bool result;

    result = false;
    if (char_is_alpha(c) || (c == '_')) {
        result = true;
    }
    return result;
}

static bool
char_is_identifier_body(char c) {
    bool result;

    result = false;
    if (char_is_identifier_start(c) || char_is_digit(c)) {
        result = true;
    }
    return result;
}

static bool
char_is_horizontal_space(char c) {
    bool result;

    result = false;
    if ((c == ' ') || (c == '\t') || (c == '\v') || (c == '\f')
    || (c == '\r')) {
        result = true;
    }
    return result;
}

static bool
char_is_number_body(char c) {
    bool result;

    result = false;
    if (char_is_identifier_body(c) || char_is_digit(c) || (c == '.')
    || (c == '\'')) {
        result = true;
    }
    return result;
}

static int32
scan_number_literal(char *text, int32 text_len, int32 start) {
    int32 result;

    result = 1;
    if ((text[start] == '.') && ((start + 1) < text_len)
    && char_is_digit(text[start + 1])) {
        result = 2;
    }

    while ((start + result) < text_len) {
        char c;
        char prev;

        c = text[start + result];
        prev = text[start + result - 1];
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

static bool
line_starts_preprocessor(char *text, int32 len) {
    bool result;

    result = false;
    for (int32 i = 0; i < len; i += 1) {
        if (char_is_horizontal_space(text[i])) {
            continue;
        }
        if (text[i] == '#') {
            result = true;
        }
        break;
    }
    return result;
}

static void
line_reserve_tokens(Line *line, int32 extra) {
    int32 need;
    int32 new_capacity;
    Token *new_tokens;

    need = line->token_count + extra;
    if (need <= line->token_capacity) {
        return;
    }

    if (line->token_capacity > 0) {
        new_capacity = line->token_capacity;
    } else {
        new_capacity = TOKENIZE_INITIAL_TOKEN_CAPACITY;
    }
    while (new_capacity < need) {
        new_capacity *= 2;
    }

    if (line->tokens) {
        new_tokens = realloc2(line->tokens, line->token_capacity,
        new_capacity, SIZEOF(*line->tokens));
    } else {
        new_tokens = malloc2(new_capacity*SIZEOF(*new_tokens));
    }
    line->tokens = new_tokens;
    line->token_capacity = new_capacity;
    return;
}

static void
line_add_token(Line *line,
enum TokenKind category,
char *text,
int32 len,
int32 column) {
    Token *token;

    if (len <= 0) {
        return;
    }

    line_reserve_tokens(line, 1);
    token = &line->tokens[line->token_count];
    token->kind = category;
    token->len = len;
    token->column = column;
    token->offset = column;
    token->text = malloc2(len + 1);
    memcpy64(token->text, text, len);
    token->text[len] = '\0';
    line->token_count += 1;
    return;
}

static int32
literal_quote_index(char *text, int32 text_len, int32 start) {
    int32 result;

    result = -1;
    if ((text[start] == '\'') || (text[start] == '"')) {
        result = start;
    } else if ((text[start] == 'L') || (text[start] == 'u')
    || (text[start] == 'U')) {
        if (((start + 1) < text_len)
        && ((text[start + 1] == '\'') || (text[start + 1] == '"'))) {
            result = start + 1;
        } else if ((text[start] == 'u') && ((start + 2) < text_len)
        && (text[start + 1] == '8')
        && ((text[start + 2] == '\'') || (text[start + 2] == '"'))) {
            result = start + 2;
        }
    }
    return result;
}

static int32
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
            i += 2;
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

static int32
scan_line_comment(char *text, int32 text_len, int32 start) {
    int32 i;

    i = start;
    while ((i < text_len) && (text[i] != '\n')) {
        i += 1;
    }
    return i - start;
}

static int32
scan_block_comment(char *text,
int32 len,
int32 start,
bool *in_block_comment) {
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

static enum TokenKind
operator_or_punct_category(char *text,
int32 len,
int32 start,
int32 *out_len) {
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

    if ((a == '%') && (b == ':') && (c == '%') && (d == ':')) {
        *out_len = 4;
    } else if (((a == '<') && (b == '<') && (c == '='))
    || ((a == '>') && (b == '>') && (c == '='))) {
        *out_len = 3;
    } else if ((a == '.') && (b == '.') && (c == '.')) {
        *out_len = 3;
        result = TOKEN_PUNCT;
    } else if (((a == '+') && ((b == '+') || (b == '=')))
    || ((a == '-') && ((b == '-') || (b == '=') || (b == '>')))
    || ((a == '*') && (b == '='))
    || ((a == '/') && (b == '='))
    || ((a == '%') && (b == '='))
    || ((a == '=') && (b == '='))
    || ((a == '!') && (b == '='))
    || ((a == '<') && ((b == '=') || (b == '<')))
    || ((a == '>') && ((b == '=') || (b == '>')))
    || ((a == '&') && ((b == '&') || (b == '=')))
    || ((a == '|') && ((b == '|') || (b == '=')))
    || ((a == '^') && (b == '='))
    || ((a == ':') && (b == ':'))
    || ((a == '#') && (b == '#'))
    || ((a == '%') && (b == ':'))) {
        *out_len = 2;
    } else if (((a == '<') && ((b == ':') || (b == '%')))
    || ((a == ':') && (b == '>'))
    || ((a == '%') && (b == '>'))) {
        *out_len = 2;
        result = TOKEN_PUNCT;
    } else if ((a == ':') || (a == '.') || (a == ',') || (a == ';')
    || (a == '(') || (a == ')') || (a == '[') || (a == ']')
    || (a == '{') || (a == '}')) {
        result = TOKEN_PUNCT;
    }
    return result;
}

static bool
char_is_operator_or_punct(char c) {
    bool result;

    result = false;
    if ((c == '+')
    || (c == '-')
    || (c == '*')
    || (c == '/')
    || (c == '%')
    || (c == '=')
    || (c == '!')
    || (c == '<')
    || (c == '>')
    || (c == '&')
    || (c == '|')
    || (c == '^')
    || (c == '~')
    || (c == '?')
    || (c == ':')
    || (c == '.')
    || (c == ',')
    || (c == ';')
    || (c == '(')
    || (c == ')')
    || (c == '[')
    || (c == ']')
    || (c == '{')
    || (c == '}')
    || (c == '#')) {
        result = true;
    }
    return result;
}

static void
tokenize_line_with_flags(Line *line, bool *in_block_comment, int32 flags) {
    int32 i;

    if (((flags & TOKENIZE_PREPROCESSOR_LINES) != 0)
    && line_starts_preprocessor(line->text, line->len)) {
        line_add_token(line, TOKEN_PREPROC, line->text,
        line->len, 0);
        return;
    }

    i = 0;
    while (i < line->len) {
        enum TokenKind category;
        int32 token_len;

        if (*in_block_comment) {
            if (line->text[i] == '\n') {
                if ((flags & TOKENIZE_SKIP_WHITESPACE) == 0) {
                    line_add_token(line, TOKEN_NEWLINE,
                    line->text + i, 1, i);
                }
                i += 1;
                continue;
            }
            token_len = scan_block_comment(line->text, line->len, i,
            in_block_comment);
            line_add_token(line, TOKEN_COMMENT, line->text + i,
            token_len, i);
            i += token_len;
            continue;
        }

        if (line->text[i] == '\n') {
            if ((flags & TOKENIZE_SKIP_WHITESPACE) == 0) {
                line_add_token(line, TOKEN_NEWLINE, line->text + i,
                1, i);
            }
            i += 1;
        } else if (char_is_horizontal_space(line->text[i])) {
            token_len = 1;
            while (((i + token_len) < line->len)
            && char_is_horizontal_space(line->text[i + token_len])) {
                token_len += 1;
            }
            if ((flags & TOKENIZE_SKIP_WHITESPACE) == 0) {
                line_add_token(line, TOKEN_SPACE, line->text + i,
                token_len, i);
            }
            i += token_len;
        } else if (((i + 1) < line->len)
        && (line->text[i] == '/')
        && (line->text[i + 1] == '/')) {
            token_len = scan_line_comment(line->text, line->len, i);
            line_add_token(line, TOKEN_COMMENT, line->text + i,
            token_len, i);
            i += token_len;
        } else if (((i + 1) < line->len)
        && (line->text[i] == '/')
        && (line->text[i + 1] == '*')) {
            token_len = scan_block_comment(line->text, line->len, i,
            in_block_comment);
            line_add_token(line, TOKEN_COMMENT, line->text + i,
            token_len, i);
            i += token_len;
        } else if ((token_len = scan_literal_token(line->text, line->len,
        i)) > 0) {
            line_add_token(line, TOKEN_LITERAL, line->text + i,
            token_len, i);
            i += token_len;
        } else if (char_is_identifier_start(line->text[i])) {
            token_len = 1;
            while (((i + token_len) < line->len)
            && char_is_identifier_body(line->text[i + token_len])) {
                token_len += 1;
            }
            line_add_token(line, TOKEN_IDENT, line->text + i,
            token_len, i);
            i += token_len;
        } else if (char_is_digit(line->text[i])
        || ((line->text[i] == '.') && ((i + 1) < line->len)
        && char_is_digit(line->text[i + 1]))) {
            token_len = scan_number_literal(line->text, line->len, i);
            line_add_token(line, TOKEN_LITERAL, line->text + i,
            token_len, i);
            i += token_len;
        } else if (char_is_operator_or_punct(line->text[i])) {
            category = operator_or_punct_category(line->text, line->len,
            i, &token_len);
            line_add_token(line, category, line->text + i, token_len, i);
            i += token_len;
        } else {
            line_add_token(line, TOKEN_UNKNOWN, line->text + i, 1, i);
            i += 1;
        }
    }
    return;
}

static Line
tokenize_text_with_flags(char *text, int32 text_len, int32 flags) {
    bool in_block_comment;
    Line result = {0};

    in_block_comment = false;
    result.text = text;
    result.len = text_len;
    tokenize_line_with_flags(&result, &in_block_comment, flags);
    return result;
}

static void
tokenize_line(Line *line, bool *in_block_comment) {
    tokenize_line_with_flags(line, in_block_comment, TOKENIZE_DEFAULT);
    return;
}

static void
tokenize_cstyle_line(Line *line, bool *in_block_comment) {
    tokenize_line_with_flags(line, in_block_comment,
    TOKENIZE_PREPROCESSOR_LINES);
    return;
}

static void
free_line_tokens(Line *line) {
    for (int32 i = 0; i < line->token_count; i += 1) {
        free2(line->tokens[i].text, line->tokens[i].len + 1);
    }
    if (line->tokens) {
        free2(line->tokens, line->token_capacity*SIZEOF(*line->tokens));
    }
    line->tokens = NULL;
    line->token_count = 0;
    line->token_capacity = 0;
    return;
}

static bool
token_is_trivia(Token *token) {
    bool result;

    result = false;
    if ((token->kind == TOKEN_SPACE) || (token->kind == TOKEN_NEWLINE)
    || (token->kind == TOKEN_COMMENT)) {
        result = true;
    }
    return result;
}

static int32
tokenization_significant_at_or_after(Tokenization *tokenization,
                                     int32 token_index) {
    int32 result;

    result = token_index;
    while ((result < tokenization->token_count)
    && token_is_trivia(&tokenization->tokens[result])) {
        result += 1;
    }
    return result;
}

static int32
tokenization_next_significant(Tokenization *tokenization, int32 token_index) {
    return tokenization_significant_at_or_after(tokenization, token_index + 1);
}

static int32
tokenization_previous_significant(Tokenization *tokenization,
                                  int32 token_index) {
    int32 result;

    result = token_index - 1;
    while ((result >= 0) && token_is_trivia(&tokenization->tokens[result])) {
        result -= 1;
    }
    return result;
}

static int32
tokenization_token_at_or_after_offset(Tokenization *tokenization,
                                      int32 offset) {
    for (int32 i = 0; i < tokenization->token_count; i += 1) {
        Token *token;

        token = &tokenization->tokens[i];
        if (offset < token->offset + token->len) {
            return i;
        }
    }
    return tokenization->token_count;
}

static int32
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

static bool
tokenization_is_in_preprocessor_define(Tokenization *tokenization,
                                       int32 token_index) {
    int32 line_start_offset;
    int32 i;

    if ((token_index < 0) || (token_index >= tokenization->token_count)) {
        return false;
    }
    line_start_offset = tokenization_logical_line_start_offset(
        tokenization, tokenization->tokens[token_index].offset);
    i = tokenization_token_at_or_after_offset(tokenization,
                                               line_start_offset);
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

static int32
tokenization_find_matching(Tokenization *tokenization, int32 open_index) {
    char *close;
    char *open;
    int32 depth;

    if ((open_index < 0) || (open_index >= tokenization->token_count)) {
        return -1;
    }

    open = tokenization->tokens[open_index].text;
    close = NULL;
    if (TOKEN_IS(&tokenization->tokens[open_index], "(")) {
        close = ")";
    } else if (TOKEN_IS(&tokenization->tokens[open_index], "[")) {
        close = "]";
    } else if (TOKEN_IS(&tokenization->tokens[open_index], "{")) {
        close = "}";
    }
    if (!close) {
        return -1;
    }

    depth = 0;
    for (int32 i = open_index; i < tokenization->token_count; i += 1) {
        Token *token;

        token = &tokenization->tokens[i];
        if (TOKEN_IS(token, open)) {
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

static Tokenization
tokenize_with_flags(char *text, int32 text_len, int32 flags) {
    Line line = tokenize_text_with_flags(text, text_len, flags);
    Tokenization result = {0};

    result.text = text;
    result.text_len = text_len;
    result.tokens = line.tokens;
    result.token_count = line.token_count;
    result.token_capacity = line.token_capacity;

    return result;
}

static Tokenization
tokenize(char *text, int32 text_len) {
    return tokenize_with_flags(text, text_len, TOKENIZE_DEFAULT);
}

static void
free_tokenization(Tokenization *tokenization) {
    for (int32 i = 0; i < tokenization->token_count; i += 1) {
        Token *token = &tokenization->tokens[i];
        free2(token->text, token->len + 1);
    }

    free2(tokenization->tokens,
          tokenization->token_capacity*SIZEOF(*tokenization->tokens));

    tokenization->tokens = NULL;
    tokenization->token_count = 0;
    tokenization->token_capacity = 0;
    tokenization->text = NULL;
    tokenization->text_len = 0;

    return;
}

#if 0 == TESTING_meta_tokenize
static inline void
meta_tokenize_sink(void) {
    (void)tokenize_line;
    (void)tokenize_cstyle_line;
    (void)free_line_tokens;
}
#endif

#if TESTING_meta_tokenize

static void
test_assert_token(Token *token, enum TokenKind kind, char *text, int32 column) {
    ASSERT(token->kind == kind);
    ASSERT(TOKEN_IS(token, text));
    ASSERT_EQUAL(token->len, strlen32(text));
    ASSERT_EQUAL(token->column, column);
    ASSERT_EQUAL(token->offset, column);
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
    ASSERT_EQUAL(scan_number_literal("123 ", strlen32("123 "), 0), 3);
    ASSERT_EQUAL(scan_number_literal(".5f ", strlen32(".5f "), 0), 3);
    ASSERT_EQUAL(scan_number_literal("3.14e+2;", strlen32("3.14e+2;"), 0), 7);
    ASSERT_EQUAL(scan_number_literal("0x1.fp-2,", strlen32("0x1.fp-2,"), 0),
                 8);
    ASSERT_EQUAL(scan_number_literal("1'000u", strlen32("1'000u"), 0), 6);
    return;
}

static void
test_literal_scanners(void) {
    char *literal = "\"a\\\"b\" tail";

    ASSERT_EQUAL(literal_quote_index("'x'", strlen32("'x'"), 0), 0);
    ASSERT_EQUAL(literal_quote_index("L\"abc\"", strlen32("L\"abc\""), 0),
                 1);
    ASSERT_EQUAL(literal_quote_index("u8\"abc\"", strlen32("u8\"abc\""), 0),
                 2);
    ASSERT_EQUAL(literal_quote_index("name", strlen32("name"), 0), -1);
    ASSERT_EQUAL(scan_literal_token(literal, strlen32(literal), 0), 6);
    ASSERT_EQUAL(scan_literal_token("u8\"xy\";", strlen32("u8\"xy\";"), 0),
                 6);
    ASSERT_EQUAL(scan_literal_token("name", strlen32("name"), 0), 0);
    return;
}

static void
test_comment_scanners(void) {
    bool in_block_comment = false;

    ASSERT_EQUAL(scan_line_comment("// abc\nx", strlen32("// abc\nx"), 0),
                 6);
    ASSERT_EQUAL(scan_block_comment("/* abc */x", strlen32("/* abc */x"),
                                    0, &in_block_comment), 9);
    ASSERT(!in_block_comment);
    ASSERT_EQUAL(scan_block_comment("/* abc\nx", strlen32("/* abc\nx"),
                                    0, &in_block_comment), 6);
    ASSERT(in_block_comment);
    ASSERT_EQUAL(scan_block_comment("continued */", strlen32("continued */"),
                                    0, &in_block_comment), 12);
    ASSERT(!in_block_comment);
    return;
}

static void
test_operator_or_punct_category(void) {
    enum TokenKind kind;
    int32 len;

    kind = operator_or_punct_category(">>=", strlen32(">>="), 0, &len);
    ASSERT(kind == TOKEN_OPERATOR);
    ASSERT_EQUAL(len, 3);

    kind = operator_or_punct_category("...", strlen32("..."), 0, &len);
    ASSERT(kind == TOKEN_PUNCT);
    ASSERT_EQUAL(len, 3);

    kind = operator_or_punct_category("<:", strlen32("<:"), 0, &len);
    ASSERT(kind == TOKEN_PUNCT);
    ASSERT_EQUAL(len, 2);

    kind = operator_or_punct_category("&&", strlen32("&&"), 0, &len);
    ASSERT(kind == TOKEN_OPERATOR);
    ASSERT_EQUAL(len, 2);

    kind = operator_or_punct_category("(", strlen32("("), 0, &len);
    ASSERT(kind == TOKEN_PUNCT);
    ASSERT_EQUAL(len, 1);
    return;
}

static void
test_line_starts_preprocessor(void) {
    ASSERT(line_starts_preprocessor("  #define X\n", strlen32("  #define X\n")));
    ASSERT(!line_starts_preprocessor("  int x;\n", strlen32("  int x;\n")));
    ASSERT(!line_starts_preprocessor("", 0));
    return;
}

static void
test_tokenize_line_default(void) {
    char *text = "int x = foo(1, \"a\"); // c\n";
    bool in_block_comment = false;
    Line line = {0};

    line.text = text;
    line.len = strlen32(text);
    tokenize_line(&line, &in_block_comment);
    ASSERT_EQUAL(line.token_count, 17);
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
    ASSERT(!in_block_comment);
    free_line_tokens(&line);
    return;
}

static void
test_tokenize_preprocessor_and_skip_whitespace(void) {
    char *preproc_text = "  #include <x>\n";
    char *skip_text = "a b\n";
    bool in_block_comment = false;
    Line line = {0};
    Line skipped;

    line.text = preproc_text;
    line.len = strlen32(preproc_text);
    tokenize_cstyle_line(&line, &in_block_comment);
    ASSERT_EQUAL(line.token_count, 1);
    test_assert_token(&line.tokens[0], TOKEN_PREPROC, preproc_text, 0);
    free_line_tokens(&line);

    skipped = tokenize_text_with_flags(skip_text, strlen32(skip_text),
                                       TOKENIZE_SKIP_WHITESPACE);
    ASSERT_EQUAL(skipped.token_count, 2);
    test_assert_token(&skipped.tokens[0], TOKEN_IDENT, "a", 0);
    test_assert_token(&skipped.tokens[1], TOKEN_IDENT, "b", 2);
    free_line_tokens(&skipped);
    return;
}

static void
test_tokenize_block_comment_across_lines(void) {
    bool in_block_comment = false;
    Line first = {0};
    Line second = {0};

    first.text = "/* hello\n";
    first.len = strlen32(first.text);
    tokenize_line_with_flags(&first, &in_block_comment, TOKENIZE_DEFAULT);
    ASSERT(in_block_comment);
    ASSERT_EQUAL(first.token_count, 2);
    test_assert_token(&first.tokens[0], TOKEN_COMMENT, "/* hello", 0);
    test_assert_token(&first.tokens[1], TOKEN_NEWLINE, "\n", 8);

    second.text = "world */ int x;";
    second.len = strlen32(second.text);
    tokenize_line_with_flags(&second, &in_block_comment, TOKENIZE_DEFAULT);
    ASSERT(!in_block_comment);
    ASSERT_EQUAL(second.token_count, 6);
    test_assert_token(&second.tokens[0], TOKEN_COMMENT, "world */", 0);
    test_assert_token(&second.tokens[2], TOKEN_IDENT, "int", 9);

    free_line_tokens(&first);
    free_line_tokens(&second);
    return;
}

static void
test_tokenization_navigation(void) {
    char *text = "a /*c*/ + \n b";
    Tokenization tokenization;

    tokenization = tokenize(text, strlen32(text));
    ASSERT_EQUAL(tokenization.token_count, 9);
    ASSERT_EQUAL(tokenization_significant_at_or_after(&tokenization, 1), 4);
    ASSERT_EQUAL(tokenization_next_significant(&tokenization, 0), 4);
    ASSERT_EQUAL(tokenization_previous_significant(&tokenization, 8), 4);
    ASSERT_EQUAL(tokenization_token_at_or_after_offset(&tokenization, 8), 4);
    ASSERT_EQUAL(tokenization_token_at_or_after_offset(&tokenization,
                                                       strlen32(text)),
                 tokenization.token_count);
    ASSERT(token_is_trivia(&tokenization.tokens[1]));
    ASSERT(token_is_trivia(&tokenization.tokens[2]));
    ASSERT(!token_is_trivia(&tokenization.tokens[4]));
    free_tokenization(&tokenization);
    return;
}

static void
test_tokenization_preprocessor_define_detection(void) {
    char *text = "#define X(a) \\\n    ((a) + 1)\nint z;\n";
    Tokenization tokenization;
    int32 plus;
    int32 int_token;

    tokenization = tokenize(text, strlen32(text));
    plus = test_find_token(&tokenization, "+");
    int_token = test_find_token(&tokenization, "int");
    ASSERT(plus >= 0);
    ASSERT(int_token >= 0);
    ASSERT_EQUAL(tokenization_logical_line_start_offset(
                 &tokenization, tokenization.tokens[plus].offset), 0);
    ASSERT(tokenization_is_in_preprocessor_define(&tokenization, plus));
    ASSERT(!tokenization_is_in_preprocessor_define(&tokenization, int_token));
    ASSERT(!tokenization_is_in_preprocessor_define(&tokenization, -1));
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
    ASSERT_EQUAL(tokenization_find_matching(&tokenization, paren), 11);
    ASSERT_EQUAL(tokenization_find_matching(&tokenization, bracket), 4);
    ASSERT_EQUAL(tokenization_find_matching(&tokenization, nested), 10);
    ASSERT_EQUAL(tokenization_find_matching(&tokenization, 1), -1);
    ASSERT_EQUAL(tokenization_find_matching(&tokenization, -1), -1);
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
    ASSERT_EQUAL(tokenization.text_len, strlen32(text));
    ASSERT_EQUAL(tokenization.token_count, 3);
    test_assert_token(&tokenization.tokens[0], TOKEN_IDENT, "x", 0);
    test_assert_token(&tokenization.tokens[1], TOKEN_OPERATOR, "+", 2);
    test_assert_token(&tokenization.tokens[2], TOKEN_IDENT, "y", 4);
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
    test_tokenize_line_default();
    test_tokenize_preprocessor_and_skip_whitespace();
    test_tokenize_block_comment_across_lines();
    test_tokenization_navigation();
    test_tokenization_preprocessor_define_detection();
    test_tokenization_find_matching();
    test_tokenize_with_flags_returns_source_metadata();
    return 0;
}

#endif /* TESTING_meta_tokenize */

#endif /* META_TOKENIZE_C */
