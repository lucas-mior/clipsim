// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(META_H)
#define META_H

#include "platform_detection.h"
#include "warnings.h"
#include "primitives.h"
#include "base_macros.h"

#define TOKEN_KIND_FIELDS  \
    XX(TOKEN_UNKNOWN)      \
    XX(TOKEN_SPACE)        \
    XX(TOKEN_NEWLINE)      \
    XX(TOKEN_IDENT)        \
    XX(TOKEN_LITERAL)      \
    XX(TOKEN_COMMENT)      \
    XX(TOKEN_OPERATOR)     \
    XX(TOKEN_PUNCT)        \
    XX(TOKEN_PREPROC)

#if defined(CBASE_H)
  #define ENUM_NAME TokenKind
  #define ENUM_BITFLAGS 0
  #define ENUM_PREFIX_ TOKEN_
  #define ENUM_FIELDS TOKEN_KIND_FIELDS
  #define XENUMS_DECLARE_ONLY 1
  #define XENUMS_NO_TESTS 1
  #include "xenums.c"
  #undef XENUMS_NO_TESTS
#else
  enum TokenKind {
      #define XX(E) E,
      TOKEN_KIND_FIELDS
      #undef XX
      TOKEN_COUNT,
  };
  typedef struct StrBuilder StrBuilder;
#endif

enum TokenizeFlags {
    TOKENIZE_DEFAULT = 0,
    TOKENIZE_PREPROCESSOR_LINES = 1 << 0,
    TOKENIZE_SKIP_WHITESPACE = 1 << 1,
};



enum CAssignOp {
    C_ASSIGN_OP_INVALID = 0,
    C_ASSIGN_OP_ASSIGN,
    C_ASSIGN_OP_ADD,
    C_ASSIGN_OP_SUB,
    C_ASSIGN_OP_MUL,
    C_ASSIGN_OP_DIV,
    C_ASSIGN_OP_MOD,
    C_ASSIGN_OP_SHL,
    C_ASSIGN_OP_SHR,
    C_ASSIGN_OP_BIT_AND,
    C_ASSIGN_OP_BIT_XOR,
    C_ASSIGN_OP_BIT_OR,
};

enum CBinaryOp {
    C_BINARY_OP_INVALID = 0,
    C_BINARY_OP_MUL,
    C_BINARY_OP_DIV,
    C_BINARY_OP_MOD,
    C_BINARY_OP_ADD,
    C_BINARY_OP_SUB,
    C_BINARY_OP_SHL,
    C_BINARY_OP_SHR,
    C_BINARY_OP_LT,
    C_BINARY_OP_LE,
    C_BINARY_OP_GT,
    C_BINARY_OP_GE,
    C_BINARY_OP_EQ,
    C_BINARY_OP_NE,
    C_BINARY_OP_BIT_AND,
    C_BINARY_OP_BIT_XOR,
    C_BINARY_OP_BIT_OR,
    C_BINARY_OP_LOGICAL_AND,
    C_BINARY_OP_LOGICAL_OR,
};

enum CUnaryOp {
    C_UNARY_OP_INVALID = 0,
    C_UNARY_OP_PLUS,
    C_UNARY_OP_MINUS,
    C_UNARY_OP_LOGICAL_NOT,
    C_UNARY_OP_BIT_NOT,
    C_UNARY_OP_DEREFERENCE,
    C_UNARY_OP_ADDRESS,
    C_UNARY_OP_PRE_INCREMENT,
    C_UNARY_OP_PRE_DECREMENT,
    C_UNARY_OP_POST_INCREMENT,
    C_UNARY_OP_POST_DECREMENT,
};

enum CMemberOp {
    C_MEMBER_OP_INVALID = 0,
    C_MEMBER_OP_DOT,
    C_MEMBER_OP_ARROW,
};

enum CKeyword {
    C_KEYWORD_INVALID = 0,
    C_KEYWORD_ALIGNAS,
    C_KEYWORD_ALIGNOF,
    C_KEYWORD_ATOMIC,
    C_KEYWORD_BOOL,
    C_KEYWORD_COMPLEX,
    C_KEYWORD_GENERIC,
    C_KEYWORD_IMAGINARY,
    C_KEYWORD_NORETURN,
    C_KEYWORD_STATIC_ASSERT,
    C_KEYWORD_THREAD_LOCAL,
    C_KEYWORD_AUTO,
    C_KEYWORD_BREAK,
    C_KEYWORD_CASE,
    C_KEYWORD_CHAR,
    C_KEYWORD_CONST,
    C_KEYWORD_CONTINUE,
    C_KEYWORD_DEFAULT,
    C_KEYWORD_DO,
    C_KEYWORD_DOUBLE,
    C_KEYWORD_ELSE,
    C_KEYWORD_ENUM,
    C_KEYWORD_EXTERN,
    C_KEYWORD_FLOAT,
    C_KEYWORD_FOR,
    C_KEYWORD_GOTO,
    C_KEYWORD_IF,
    C_KEYWORD_INLINE,
    C_KEYWORD_INT,
    C_KEYWORD_LONG,
    C_KEYWORD_REGISTER,
    C_KEYWORD_RESTRICT,
    C_KEYWORD_RETURN,
    C_KEYWORD_SHORT,
    C_KEYWORD_SIGNED,
    C_KEYWORD_SIZEOF,
    C_KEYWORD_STATIC,
    C_KEYWORD_STRUCT,
    C_KEYWORD_SWITCH,
    C_KEYWORD_TYPEDEF,
    C_KEYWORD_UNION,
    C_KEYWORD_UNSIGNED,
    C_KEYWORD_VOID,
    C_KEYWORD_VOLATILE,
    C_KEYWORD_WHILE,
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

char *TOKEN_str(enum TokenKind);
void TOKEN_str_free(char *);
enum TokenKind TOKEN_parse(char *);

int32 token_is_val(Token, char *);
int32 token_is_ptr(Token *, char *);
int32 token_is_val_len(Token, char *, int32);
int32 token_is_ptr_len(Token *, char *, int32);
enum CAssignOp c_assign_op_from_text(char *, int32);
enum CAssignOp c_token_assign_op(Token *);
enum CBinaryOp c_binary_op_from_text(char *, int32);
enum CBinaryOp c_token_binary_op(Token *);
enum CUnaryOp c_unary_op_from_text(char *, int32);
enum CUnaryOp c_token_unary_op(Token *);
enum CUnaryOp c_token_postfix_unary_op(Token *);
enum CMemberOp c_member_op_from_text(char *, int32);
enum CMemberOp c_token_member_op(Token *);
enum CKeyword c_keyword_from_text(char *, int32);
enum CKeyword c_token_keyword(Token *);
bool c_text_is_type_word(char *, int32);
bool c_text_is_declaration_prefix(char *, int32);
bool c_token_is_type_word(Token *);
bool c_token_is_declaration_prefix(Token *);
int32 c_binary_op_precedence(enum CBinaryOp);
int32 precedence_of(char *, int32);

bool char_is_alpha(char);
bool char_is_digit(char);
bool char_is_horizontal_space(char);
bool char_is_identifier_body(char);
bool char_is_identifier_start(char);
bool char_is_number_body(char);
bool char_is_operator_or_punct(char);
void free_line_tokens(Line *);
void free_tokenization(Tokenization *);
void line_add_token(Line *, enum TokenKind, char *, int32, int32);
void line_reserve_tokens(Line *, int32);
bool line_starts_preprocessor(char *, int32);
int32 literal_quote_index(char *, int32, int32);
enum TokenKind operator_or_punct_category(char *, int32, int32, int32 *);
int32 scan_block_comment(char *, int32, int32, bool *);
int32 scan_line_comment(char *, int32, int32);
int32 scan_literal_token(char *, int32, int32);
int32 scan_number_literal(char *, int32, int32);
bool token_is_number(Token *);
bool token_is_trivia(Token *);
int32 tokenization_find_matching(Tokenization *, int32);
bool tokenization_is_in_preprocessor_define(Tokenization *, int32);
int32 tokenization_logical_line_start_offset(Tokenization *, int32);
int32 tokenization_next_significant(Tokenization *, int32);
int32 tokenization_previous_significant(Tokenization *, int32);
int32 tokenization_significant_at_or_after(Tokenization *, int32);
int32 tokenization_token_at_or_after_offset(Tokenization *, int32);
Tokenization tokenize(char *, int32);
void tokenize_cstyle_line(Line *, bool *);
void tokenize_line(Line *, bool *);
void tokenize_line_with_flags(Line *, bool *, int32);
Line tokenize_text_with_flags(char *, int32, int32);
Tokenization tokenize_with_flags(char *, int32, int32);
void document_add_line(Document *, char *, int32, bool *, int32);
void document_reserve_lines(Document *, int32);
void free_document(Document *);
void free_line(Line *);
Document *parse_c_text(char *, int32);
Document *parse_text(char *, int32);
Document *parse_text_with_flags(char *, int32, int32);
void c_emit_wrapped_expr(StrBuilder *, char *, char *, char *, char *);
StrBuilder c_identifier(char *, int32);
bool c_identifier_is_keyword(char *);
StrBuilder c_string_literal(char *, int32);
void emit_int_array_initializer(StrBuilder *, char *, int32 *, int32);
void emit_lens_initializer(
    StrBuilder *,
    char *,
    char **,
    int32 *,
    int32,
    char *
);
void emit_string_array_initializer(
    StrBuilder *,
    char *,
    char **,
    int32 *,
    int32,
    char *
);
void emit_u64_array_initializer(StrBuilder *, char *, uint64 *, int32);

#define token_is_2(TOKEN, WHAT)                \
_Generic((TOKEN),                              \
    Token: token_is_val,                       \
    Token *: token_is_ptr                      \
)((TOKEN), (WHAT))

#define token_is_3(TOKEN, WHAT, WHAT_LEN)      \
_Generic((TOKEN),                              \
    Token: token_is_val_len,                   \
    Token *: token_is_ptr_len                  \
)((TOKEN), (WHAT), (WHAT_LEN))

#define TOKEN_IS(...) SELECT_ON_NUM_ARGS(token_is_, __VA_ARGS__)

#endif /* META_H */
