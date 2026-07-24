// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(META_H)
#define META_H

#include "primitives.h"

enum TokenKind {
    TOKEN_UNKNOWN,
    TOKEN_SPACE,
    TOKEN_NEWLINE,
    TOKEN_IDENT,
    TOKEN_LITERAL,
    TOKEN_COMMENT,
    TOKEN_OPERATOR,
    TOKEN_PUNCT,
    TOKEN_PREPROC,
    TOKEN_LAST,
};

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

static char *TOKEN_str(enum TokenKind);
static void TOKEN_str_free(char *);
static enum TokenKind TOKEN_parse(char *);
static bool TOKEN_token_equals(char *, int32, char *);
static bool TOKEN_token_equals_enum_name(char *, int32, char *);
static void TOKEN_functions_sink(void);

static int32 token_is_val(Token, char *);
static int32 token_is_ptr(Token *, char *);
static int32 token_is_val_len(Token, char *, int32);
static int32 token_is_ptr_len(Token *, char *, int32);
static int32 precedence_of(char *, int32);

static bool char_is_alpha(char);
static bool char_is_digit(char);
static bool char_is_horizontal_space(char);
static bool char_is_identifier_body(char);
static bool char_is_identifier_start(char);
static bool char_is_number_body(char);
static bool char_is_operator_or_punct(char);
static void free_line_tokens(Line *);
static void free_tokenization(Tokenization *);
static void line_add_token(Line *, enum TokenKind, char *, int32, int32);
static void line_reserve_tokens(Line *, int32);
static bool line_starts_preprocessor(char *, int32);
static int32 literal_quote_index(char *, int32, int32);
static void meta_tokenize_sink(void);
static enum TokenKind operator_or_punct_category(char *, int32, int32, int32 *);
static int32 scan_block_comment(char *, int32, int32, bool *);
static int32 scan_line_comment(char *, int32, int32);
static int32 scan_literal_token(char *, int32, int32);
static int32 scan_number_literal(char *, int32, int32);
static bool token_is_number(Token *);
static bool token_is_trivia(Token *);
static int32 tokenization_find_matching(Tokenization *, int32);
static bool tokenization_is_in_preprocessor_define(Tokenization *, int32);
static int32 tokenization_logical_line_start_offset(Tokenization *, int32);
static int32 tokenization_next_significant(Tokenization *, int32);
static int32 tokenization_previous_significant(Tokenization *, int32);
static int32 tokenization_significant_at_or_after(Tokenization *, int32);
static int32 tokenization_token_at_or_after_offset(Tokenization *, int32);
static Tokenization tokenize(char *, int32);
static void tokenize_cstyle_line(Line *, bool *);
static void tokenize_line(Line *, bool *);
static void tokenize_line_with_flags(Line *, bool *, int32);
static Line tokenize_text_with_flags(char *, int32, int32);
static Tokenization tokenize_with_flags(char *, int32, int32);
static void document_add_line(Document *, char *, int32, bool *, int32);
static void document_reserve_lines(Document *, int32);
static void free_document(Document *);
static void free_line(Line *);
static Document *parse_c_text(char *, int32);
static Document *parse_text(char *, int32);
static Document *parse_text_with_flags(char *, int32, int32);
static void c_emit_wrapped_expr(StrBuilder *, char *, char *, char *, char *);
static StrBuilder c_identifier(char *, int32);
static bool c_identifier_is_keyword(char *);
static StrBuilder c_string_literal(char *, int32);
static void emit_int_array_initializer(StrBuilder *, char *, int32 *, int32);
static void emit_lens_initializer(
    StrBuilder *,
    char *,
    char **,
    int32 *,
    int32,
    char *
);
static void emit_string_array_initializer(
    StrBuilder *,
    char *,
    char **,
    int32 *,
    int32,
    char *
);
static void emit_u64_array_initializer(StrBuilder *, char *, uint64 *, int32);

#define token_is_2(TOKEN, WHAT) \
_Generic((TOKEN), \
    Token: token_is_val, \
    Token *: token_is_ptr \
)((TOKEN), (WHAT))

#define token_is_3(TOKEN, WHAT, WHAT_LEN) \
_Generic((TOKEN), \
    Token: token_is_val_len, \
    Token *: token_is_ptr_len \
)((TOKEN), (WHAT), (WHAT_LEN))

#define TOKEN_IS(...) SELECT_ON_NUM_ARGS(token_is_, __VA_ARGS__)

#endif /* META_H */
