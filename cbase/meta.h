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

#define C_KEYWORD_FIELDS                          \
    XX(C_KEYWORD_ALIGNAS,       _Alignas)         \
    XX(C_KEYWORD_ALIGNOF,       _Alignof)         \
    XX(C_KEYWORD_ATOMIC,        _Atomic)          \
    XX(C_KEYWORD_BOOL,          _Bool)            \
    XX(C_KEYWORD_COMPLEX,       _Complex)         \
    XX(C_KEYWORD_GENERIC,       _Generic)         \
    XX(C_KEYWORD_IMAGINARY,     _Imaginary)       \
    XX(C_KEYWORD_NORETURN,      _Noreturn)        \
    XX(C_KEYWORD_STATIC_ASSERT, _Static_assert)   \
    XX(C_KEYWORD_THREAD_LOCAL,  _Thread_local)    \
    XX(C_KEYWORD_AUTO,          auto)             \
    XX(C_KEYWORD_BREAK,         break)            \
    XX(C_KEYWORD_CASE,          case)             \
    XX(C_KEYWORD_CHAR,          char)             \
    XX(C_KEYWORD_CONST,         const)            \
    XX(C_KEYWORD_CONTINUE,      continue)         \
    XX(C_KEYWORD_DEFAULT,       default)          \
    XX(C_KEYWORD_DO,            do)               \
    XX(C_KEYWORD_DOUBLE,        double)           \
    XX(C_KEYWORD_ELSE,          else)             \
    XX(C_KEYWORD_ENUM,          enum)             \
    XX(C_KEYWORD_EXTERN,        extern)           \
    XX(C_KEYWORD_FLOAT,         float)            \
    XX(C_KEYWORD_FOR,           for)              \
    XX(C_KEYWORD_GOTO,          goto)             \
    XX(C_KEYWORD_IF,            if)               \
    XX(C_KEYWORD_INLINE,        inline)           \
    XX(C_KEYWORD_INT,           int)              \
    XX(C_KEYWORD_LONG,          long)             \
    XX(C_KEYWORD_REGISTER,      register)         \
    XX(C_KEYWORD_RESTRICT,      restrict)         \
    XX(C_KEYWORD_RETURN,        return)           \
    XX(C_KEYWORD_SHORT,         short)            \
    XX(C_KEYWORD_SIGNED,        signed)           \
    XX(C_KEYWORD_SIZEOF,        sizeof)           \
    XX(C_KEYWORD_STATIC,        static)           \
    XX(C_KEYWORD_STRUCT,        struct)           \
    XX(C_KEYWORD_SWITCH,        switch)           \
    XX(C_KEYWORD_TYPEDEF,       typedef)          \
    XX(C_KEYWORD_UNION,         union)            \
    XX(C_KEYWORD_UNSIGNED,      unsigned)         \
    XX(C_KEYWORD_VOID,          void)             \
    XX(C_KEYWORD_VOLATILE,      volatile)         \
    XX(C_KEYWORD_WHILE,         while)

#define C_ASSIGN_OP_FIELDS             \
    XX(C_ASSIGN_OP_ASSIGN,    =)       \
    XX(C_ASSIGN_OP_ADD,       +=)      \
    XX(C_ASSIGN_OP_SUB,       -=)      \
    XX(C_ASSIGN_OP_MUL,       *=)      \
    XX(C_ASSIGN_OP_DIV,       /=)      \
    XX(C_ASSIGN_OP_MOD,       %=)      \
    XX(C_ASSIGN_OP_SHL,       <<=)     \
    XX(C_ASSIGN_OP_SHR,       >>=)     \
    XX(C_ASSIGN_OP_BIT_AND,   &=)      \
    XX(C_ASSIGN_OP_BIT_XOR,   ^=)      \
    XX(C_ASSIGN_OP_BIT_OR,    |=)

#define C_BINARY_OP_FIELDS              \
    XX(C_BINARY_OP_MUL,         *)      \
    XX(C_BINARY_OP_DIV,         /)      \
    XX(C_BINARY_OP_MOD,         %)      \
    XX(C_BINARY_OP_ADD,         +)      \
    XX(C_BINARY_OP_SUB,         -)      \
    XX(C_BINARY_OP_SHL,         <<)     \
    XX(C_BINARY_OP_SHR,         >>)     \
    XX(C_BINARY_OP_LT,          <)      \
    XX(C_BINARY_OP_LE,          <=)     \
    XX(C_BINARY_OP_GT,          >)      \
    XX(C_BINARY_OP_GE,          >=)     \
    XX(C_BINARY_OP_EQ,          ==)     \
    XX(C_BINARY_OP_NE,          !=)     \
    XX(C_BINARY_OP_BIT_AND,     &)      \
    XX(C_BINARY_OP_BIT_XOR,     ^)      \
    XX(C_BINARY_OP_BIT_OR,      |)      \
    XX(C_BINARY_OP_LOGICAL_AND, &&)     \
    XX(C_BINARY_OP_LOGICAL_OR,  ||)

#define C_MEMBER_OP_FIELDS      \
    XX(C_MEMBER_OP_DOT,   .)    \
    XX(C_MEMBER_OP_ARROW, ->)

#if defined(CBASE_H)
  #define ENUM_NAME TokenKind
  #define ENUM_BITFLAGS 0
  #define ENUM_PREFIX_ TOKEN_
  #define ENUM_PARSE_STRICT
  #define ENUM_FIELDS TOKEN_KIND_FIELDS
  #define XENUMS_DECLARE_ONLY 1
  #define XENUMS_NO_TESTS 1
  #include "xenums.c"
  #undef XENUMS_NO_TESTS

  #define ENUM_NAME CKeyword
  #define ENUM_BITFLAGS 0
  #define ENUM_PREFIX_ C_KEYWORD_
  #define ENUM_PARSE_STRICT
  #define ENUM_FIELDS C_KEYWORD_FIELDS
  #define XENUMS_DECLARE_ONLY 1
  #define XENUMS_NO_TESTS 1
  #include "xenums.c"
  #undef XENUMS_NO_TESTS

  #define ENUM_NAME CAssignOp
  #define ENUM_BITFLAGS 0
  #define ENUM_PREFIX_ C_ASSIGN_OP_
  #define ENUM_PARSE_STRICT
  #define ENUM_FIELDS C_ASSIGN_OP_FIELDS
  #define XENUMS_DECLARE_ONLY 1
  #define XENUMS_NO_TESTS 1
  #include "xenums.c"
  #undef XENUMS_NO_TESTS

  #define ENUM_NAME CBinaryOp
  #define ENUM_BITFLAGS 0
  #define ENUM_PREFIX_ C_BINARY_OP_
  #define ENUM_PARSE_STRICT
  #define ENUM_FIELDS C_BINARY_OP_FIELDS
  #define XENUMS_DECLARE_ONLY 1
  #define XENUMS_NO_TESTS 1
  #include "xenums.c"
  #undef XENUMS_NO_TESTS

  #define ENUM_NAME CMemberOp
  #define ENUM_BITFLAGS 0
  #define ENUM_PREFIX_ C_MEMBER_OP_
  #define ENUM_PARSE_STRICT
  #define ENUM_FIELDS C_MEMBER_OP_FIELDS
  #define XENUMS_DECLARE_ONLY 1
  #define XENUMS_NO_TESTS 1
  #include "xenums.c"
  #undef XENUMS_NO_TESTS
#else
  enum TokenKind_XenumIndices {
      #define XX(E) CAT(E, _XENUM_INDEX),
      TOKEN_KIND_FIELDS
      #undef XX
  };
  enum TokenKind {
      #define XX(E) E = CAT(E, _XENUM_INDEX) + 1,
      TOKEN_KIND_FIELDS
      #undef XX
      TOKEN_COUNT,
  };
  enum CKeyword_XenumIndices {
      #define XX_1(E) CAT(E, _XENUM_INDEX),
      #define XX_2(E, alias) CAT(E, _XENUM_INDEX),
      #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)
      C_KEYWORD_FIELDS
      #undef XX
      #undef XX_1
      #undef XX_2
  };
  enum CKeyword {
      #define XX_1(E) E = CAT(E, _XENUM_INDEX) + 1,
      #define XX_2(E, alias) E = CAT(E, _XENUM_INDEX) + 1,
      #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)
      C_KEYWORD_FIELDS
      #undef XX
      #undef XX_1
      #undef XX_2
      C_KEYWORD_COUNT,
  };
  enum CAssignOp_XenumIndices {
      #define XX_1(E) CAT(E, _XENUM_INDEX),
      #define XX_2(E, alias) CAT(E, _XENUM_INDEX),
      #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)
      C_ASSIGN_OP_FIELDS
      #undef XX
      #undef XX_1
      #undef XX_2
  };
  enum CAssignOp {
      #define XX_1(E) E = CAT(E, _XENUM_INDEX) + 1,
      #define XX_2(E, alias) E = CAT(E, _XENUM_INDEX) + 1,
      #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)
      C_ASSIGN_OP_FIELDS
      #undef XX
      #undef XX_1
      #undef XX_2
      C_ASSIGN_OP_COUNT,
  };
  enum CBinaryOp_XenumIndices {
      #define XX_1(E) CAT(E, _XENUM_INDEX),
      #define XX_2(E, alias) CAT(E, _XENUM_INDEX),
      #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)
      C_BINARY_OP_FIELDS
      #undef XX
      #undef XX_1
      #undef XX_2
  };
  enum CBinaryOp {
      #define XX_1(E) E = CAT(E, _XENUM_INDEX) + 1,
      #define XX_2(E, alias) E = CAT(E, _XENUM_INDEX) + 1,
      #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)
      C_BINARY_OP_FIELDS
      #undef XX
      #undef XX_1
      #undef XX_2
      C_BINARY_OP_COUNT,
  };
  enum CMemberOp_XenumIndices {
      #define XX_1(E) CAT(E, _XENUM_INDEX),
      #define XX_2(E, alias) CAT(E, _XENUM_INDEX),
      #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)
      C_MEMBER_OP_FIELDS
      #undef XX
      #undef XX_1
      #undef XX_2
  };
  enum CMemberOp {
      #define XX_1(E) E = CAT(E, _XENUM_INDEX) + 1,
      #define XX_2(E, alias) E = CAT(E, _XENUM_INDEX) + 1,
      #define XX(...) SELECT_ON_NUM_ARGS(XX_, __VA_ARGS__)
      C_MEMBER_OP_FIELDS
      #undef XX
      #undef XX_1
      #undef XX_2
      C_MEMBER_OP_COUNT,
  };
  typedef struct String String;
#endif

enum TokenizeFlags {
    TOKENIZE_DEFAULT = 0,
    TOKENIZE_PREPROCESSOR_LINES = 1 << 0,
    TOKENIZE_SKIP_WHITESPACE = 1 << 1,
};

enum CUnaryOp {
    C_UNARY_OP_PLUS = 1,
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

enum CPreprocessorDirectiveKind {
    C_PREPROCESSOR_DIRECTIVE_UNKNOWN = 0,
    C_PREPROCESSOR_DIRECTIVE_DEFINE,
    C_PREPROCESSOR_DIRECTIVE_UNDEF,
    C_PREPROCESSOR_DIRECTIVE_INCLUDE,
    C_PREPROCESSOR_DIRECTIVE_IF,
    C_PREPROCESSOR_DIRECTIVE_IFDEF,
    C_PREPROCESSOR_DIRECTIVE_IFNDEF,
    C_PREPROCESSOR_DIRECTIVE_ELIF,
    C_PREPROCESSOR_DIRECTIVE_ELSE,
    C_PREPROCESSOR_DIRECTIVE_ENDIF,
    C_PREPROCESSOR_DIRECTIVE_ERROR,
    C_PREPROCESSOR_DIRECTIVE_PRAGMA,
    C_PREPROCESSOR_DIRECTIVE_LINE,
};

typedef struct Token {
    char *text; /* Borrowed source span, not necessarily NUL-terminated. */
    enum TokenKind kind;
    int32 len;
    int32 column;
    int32 offset;
} Token;

/* Half-open token-index range [first, end). */
typedef struct TokenRange {
    int32 first;
    int32 end;
} TokenRange;

/* Half-open byte-offset range into Tokenization.text. */
typedef struct SourceRange {
    int32 start;
    int32 end;
} SourceRange;

/* Physical line is 1-based; byte offset and byte column are 0-based. */
typedef struct SourceLocation {
    int32 offset;
    int32 line;
    int32 column;
} SourceLocation;

/* Delimiter nesting relative to the beginning of a token range. */
typedef struct TokenDelimiterDepth {
    int32 paren;
    int32 bracket;
    int32 brace;
} TokenDelimiterDepth;

typedef struct CPreprocessorDirective {
    SourceRange source;
    TokenRange tokens;
    enum CPreprocessorDirectiveKind kind;
    int32 hash_token;
    int32 keyword_token;
} CPreprocessorDirective;

typedef struct CPreprocessorDefine {
    CPreprocessorDirective directive;
    SourceRange replacement_source;
    TokenRange parameter_list;
    TokenRange parameters;
    TokenRange replacement;
    int32 name_token;
    int32 open_paren_token;
    int32 close_paren_token;
    bool function_like;
} CPreprocessorDefine;

typedef struct Tokenization {
    char *text;
    Token *tokens;

    int32 text_len;
    int32 token_count;
    int32 token_capacity;
    int32 padding;

    /* Lazily allocated physical-line index owned by this tokenization. */
    int32 line_count;
    int32 line_capacity;
    int32 *line_starts;
} Tokenization;

/*
 * Borrowed top-level token splitter. Returned items refer to the original
 * Tokenization and contain all trivia. Trim them with token_range_trim_trivia.
 */
typedef struct TokenRangeSplit {
    Tokenization *tokenization;
    TokenRange range;
    char *separator;
    int32 separator_len;
    int32 cursor;
    int32 source_cursor;
    int32 source_end;
    bool finished;
} TokenRangeSplit;

typedef struct Line {
    Token *tokens;
    char *text;

    int32 len;
    int32 token_count;
    int32 token_capacity;
    int32 padding;
} Line;

char *TOKEN_str(enum TokenKind);
void TOKEN_str_free(char *);
enum TokenKind TOKEN_parse(char *, int32);

int32 token_is_val(Token, char *);
int32 token_is_ptr(Token *, char *);
int32 token_is_val_len(Token, char *, int32);
int32 token_is_ptr_len(Token *, char *, int32);
enum CAssignOp c_token_assign_op(Token *);
enum CBinaryOp c_token_binary_op(Token *);
enum CUnaryOp c_unary_op_from_text(char *, int32);
enum CUnaryOp c_token_unary_op(Token *);
enum CUnaryOp c_token_postfix_unary_op(Token *);
enum CMemberOp c_token_member_op(Token *);
enum CKeyword c_token_keyword(Token *);
bool c_text_is_type_qualifier(char *, int32);
bool c_text_is_type_word(char *, int32);
bool c_text_is_declaration_prefix(char *, int32);
bool c_token_is_type_qualifier(Token *);
bool c_token_is_type_word(Token *);
bool c_token_is_declaration_prefix(Token *);
int32 c_binary_op_precedence(enum CBinaryOp);

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
bool token_is_open_delimiter(Token *);
bool token_is_close_delimiter(Token *);
bool token_delimiter_depth_equal(TokenDelimiterDepth, TokenDelimiterDepth);
bool token_delimiter_depth_is_zero(TokenDelimiterDepth);
/* Empty token ranges contain no significant tokens after trimming trivia. */
bool token_range_is_empty(Tokenization *, TokenRange);
bool token_range_is_valid(Tokenization *, TokenRange);
int32 token_range_first_significant(Tokenization *, TokenRange);
int32 token_range_last_significant(Tokenization *, TokenRange);
/*
 * Range navigation is cursor-based. Forward searches return range.end when
 * exhausted; reverse searches return range.first - 1.
 */
int32 token_range_next_kind(Tokenization *, TokenRange, int32,
                            enum TokenKind);
int32 token_range_next_significant(Tokenization *, TokenRange, int32);
int32 token_range_next_text(Tokenization *, TokenRange, int32,
                            char *, int32);
int32 token_range_previous_kind(Tokenization *, TokenRange, int32,
                                enum TokenKind);
int32 token_range_previous_significant(Tokenization *, TokenRange, int32);
int32 token_range_previous_text(Tokenization *, TokenRange, int32,
                                char *, int32);
/*
 * Depth is measured before the token at token_index. token_index may equal
 * range.end, which returns the final depth. False means malformed nesting.
 */
bool token_range_delimiter_depth_before(Tokenization *, TokenRange, int32,
                                        TokenDelimiterDepth *);
bool token_range_is_balanced(Tokenization *, TokenRange);
int32 token_range_matching_delimiter_forward(Tokenization *, TokenRange,
                                             int32);
int32 token_range_matching_delimiter_reverse(Tokenization *, TokenRange,
                                             int32);
int32 token_range_next_kind_at_depth(Tokenization *, TokenRange, int32,
                                     TokenDelimiterDepth, enum TokenKind);
int32 token_range_next_text_at_depth(Tokenization *, TokenRange, int32,
                                     TokenDelimiterDepth, char *, int32);
int32 token_range_previous_kind_at_depth(Tokenization *, TokenRange, int32,
                                         TokenDelimiterDepth, enum TokenKind);
int32 token_range_previous_text_at_depth(Tokenization *, TokenRange, int32,
                                         TokenDelimiterDepth, char *, int32);
/* Compares exact token kind/text sequences after ignoring trivia. */
bool token_range_significant_equal(Tokenization *, TokenRange,
                                   Tokenization *, TokenRange);
SourceRange token_range_source_range(Tokenization *, TokenRange);
TokenRange token_range_trim_trivia(Tokenization *, TokenRange);
bool tokenization_blank_line_between(Tokenization *, int32, int32);
bool tokenization_comment_between(Tokenization *, int32, int32);
int32 tokenization_find_matching(Tokenization *, int32);
bool tokenization_is_in_preprocessor_define(Tokenization *, int32);
bool tokenization_line_continuation_between(Tokenization *, int32, int32);

/*
 * Split at exact separator tokens outside balanced (), [] and {}.
 * init checks the entire range for malformed nesting; false means invalid.
 * An empty or trivia-only range yields zero items. For nonempty ranges,
 * consecutive/leading/trailing separators yield empty items.
 * next returns false after exhaustion. Item token ranges retain trivia
 * tokens, if present. next_spans also returns exact untrimmed source byte
 * ranges, retaining inter-token whitespace even with SKIP_WHITESPACE.
 * The input source extent is that of the supplied token range.
 * The splitter borrows both the tokenization and separator for its lifetime.
 * collect writes up to capacity items (items may be NULL if capacity is 0),
 * returns the total number required, or -1 for invalid input. It never
 * allocates; call with NULL/0 to count before allocating a result array.
 */
bool token_range_split_init(TokenRangeSplit *, Tokenization *, TokenRange,
                            char *, int32);
bool token_range_split_next(TokenRangeSplit *, TokenRange *);
bool token_range_split_next_spans(TokenRangeSplit *, TokenRange *,
                                  SourceRange *);
int32 token_range_split_collect(Tokenization *, TokenRange, char *, int32,
                                TokenRange *, int32);

/*
 * Raw preprocessor structure. No directives are expanded. Directive source
 * ranges include the terminating physical newline when one exists.
 */
enum CPreprocessorDirectiveKind c_preprocessor_directive_kind(Token *);
bool c_preprocessor_directive_at(Tokenization *, int32,
                                 CPreprocessorDirective *);
bool c_preprocessor_directive_containing(Tokenization *, int32,
                                         CPreprocessorDirective *);
bool c_preprocessor_define_info(Tokenization *, CPreprocessorDirective *,
                                CPreprocessorDefine *);
int32 c_preprocessor_define_parameter_count(Tokenization *,
                                            CPreprocessorDefine *);
bool c_preprocessor_define_parameter(Tokenization *, CPreprocessorDefine *,
                                     int32, TokenRange *);
int32 tokenization_logical_line_start_offset(Tokenization *, int32);
bool tokenization_newline_between(Tokenization *, int32, int32);
int32 tokenization_next_significant(Tokenization *, int32);
int32 tokenization_physical_line_count(Tokenization *);
/* Physical line arguments are 1-based; end offsets are exclusive. */
int32 tokenization_physical_line_end_offset(Tokenization *, int32);
int32 tokenization_physical_line_start_offset(Tokenization *, int32);
int32 tokenization_previous_significant(Tokenization *, int32);
SourceLocation tokenization_source_location(Tokenization *, int32);
int32 tokenization_significant_at_or_after(Tokenization *, int32);
int32 tokenization_token_at_or_after_offset(Tokenization *, int32);
SourceLocation tokenization_token_location(Tokenization *, int32);
Tokenization tokenize(char *, int32);
void tokenize_cstyle_line(Line *, bool *);
void tokenize_line(Line *, bool *);
void tokenize_line_with_flags(Line *, bool *, int32);
Line tokenize_text_with_flags(char *, int32, int32);
Tokenization tokenize_with_flags(char *, int32, int32);
void free_line(Line *);
void c_emit_wrapped_expr(String *, char *, char *, char *, char *);
String c_identifier(char *, int32);
bool c_identifier_is_keyword(char *);
String c_string_literal(char *, int32);
void emit_int_array_init(String *, char *, int32 *, int32);
void emit_lens_init(String *, char *, char **, int32 *, int32, char *);
void emit_string_array_init(String *, char *, char **, int32 *, int32, char *);
void emit_u64_array_init(String *, char *, uint64 *, int32);

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
