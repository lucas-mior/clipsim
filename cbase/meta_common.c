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
    return STREQUAL(token.text, token.len, what, strlen32(what));
}

int32
token_is_ptr(Token *token, char *what) {
    return STREQUAL(token->text, token->len, what, strlen32(what));
}

int32
token_is_val_len(Token token, char *what, int32 what_len) {
    return STREQUAL(token.text, token.len, what, what_len);
}

int32
token_is_ptr_len(Token *token, char *what, int32 what_len) {
    return STREQUAL(token->text, token->len, what, what_len);
}

enum CAssignOp
c_assign_op_from_text(char *text, int32 text_len) {
    if (STREQUAL(text, text_len, "=")) {
        return C_ASSIGN_OP_ASSIGN;
    }
    if (STREQUAL(text, text_len, "+=")) {
        return C_ASSIGN_OP_ADD;
    }
    if (STREQUAL(text, text_len, "-=")) {
        return C_ASSIGN_OP_SUB;
    }
    if (STREQUAL(text, text_len, "*=")) {
        return C_ASSIGN_OP_MUL;
    }
    if (STREQUAL(text, text_len, "/=")) {
        return C_ASSIGN_OP_DIV;
    }
    if (STREQUAL(text, text_len, "%=")) {
        return C_ASSIGN_OP_MOD;
    }
    if (STREQUAL(text, text_len, "<<=")) {
        return C_ASSIGN_OP_SHL;
    }
    if (STREQUAL(text, text_len, ">>=")) {
        return C_ASSIGN_OP_SHR;
    }
    if (STREQUAL(text, text_len, "&=")) {
        return C_ASSIGN_OP_BIT_AND;
    }
    if (STREQUAL(text, text_len, "^=")) {
        return C_ASSIGN_OP_BIT_XOR;
    }
    if (STREQUAL(text, text_len, "|=")) {
        return C_ASSIGN_OP_BIT_OR;
    }
    return C_ASSIGN_OP_INVALID;
}

enum CAssignOp
c_token_assign_op(Token *token) {
    if (token->kind != TOKEN_OPERATOR) {
        return C_ASSIGN_OP_INVALID;
    }
    return c_assign_op_from_text(token->text, token->len);
}

enum CBinaryOp
c_binary_op_from_text(char *text, int32 text_len) {
    if (STREQUAL(text, text_len, "*")) {
        return C_BINARY_OP_MUL;
    }
    if (STREQUAL(text, text_len, "/")) {
        return C_BINARY_OP_DIV;
    }
    if (STREQUAL(text, text_len, "%")) {
        return C_BINARY_OP_MOD;
    }
    if (STREQUAL(text, text_len, "+")) {
        return C_BINARY_OP_ADD;
    }
    if (STREQUAL(text, text_len, "-")) {
        return C_BINARY_OP_SUB;
    }
    if (STREQUAL(text, text_len, "<<")) {
        return C_BINARY_OP_SHL;
    }
    if (STREQUAL(text, text_len, ">>")) {
        return C_BINARY_OP_SHR;
    }
    if (STREQUAL(text, text_len, "<")) {
        return C_BINARY_OP_LT;
    }
    if (STREQUAL(text, text_len, "<=")) {
        return C_BINARY_OP_LE;
    }
    if (STREQUAL(text, text_len, ">")) {
        return C_BINARY_OP_GT;
    }
    if (STREQUAL(text, text_len, ">=")) {
        return C_BINARY_OP_GE;
    }
    if (STREQUAL(text, text_len, "==")) {
        return C_BINARY_OP_EQ;
    }
    if (STREQUAL(text, text_len, "!=")) {
        return C_BINARY_OP_NE;
    }
    if (STREQUAL(text, text_len, "&")) {
        return C_BINARY_OP_BIT_AND;
    }
    if (STREQUAL(text, text_len, "^")) {
        return C_BINARY_OP_BIT_XOR;
    }
    if (STREQUAL(text, text_len, "|")) {
        return C_BINARY_OP_BIT_OR;
    }
    if (STREQUAL(text, text_len, "&&")) {
        return C_BINARY_OP_LOGICAL_AND;
    }
    if (STREQUAL(text, text_len, "||")) {
        return C_BINARY_OP_LOGICAL_OR;
    }
    return C_BINARY_OP_INVALID;
}

enum CBinaryOp
c_token_binary_op(Token *token) {
    if (token->kind != TOKEN_OPERATOR) {
        return C_BINARY_OP_INVALID;
    }
    return c_binary_op_from_text(token->text, token->len);
}

enum CUnaryOp
c_unary_op_from_text(char *text, int32 text_len) {
    if (STREQUAL(text, text_len, "+")) {
        return C_UNARY_OP_PLUS;
    }
    if (STREQUAL(text, text_len, "-")) {
        return C_UNARY_OP_MINUS;
    }
    if (STREQUAL(text, text_len, "!")) {
        return C_UNARY_OP_LOGICAL_NOT;
    }
    if (STREQUAL(text, text_len, "~")) {
        return C_UNARY_OP_BIT_NOT;
    }
    if (STREQUAL(text, text_len, "*")) {
        return C_UNARY_OP_DEREFERENCE;
    }
    if (STREQUAL(text, text_len, "&")) {
        return C_UNARY_OP_ADDRESS;
    }
    if (STREQUAL(text, text_len, "++")) {
        return C_UNARY_OP_PRE_INCREMENT;
    }
    if (STREQUAL(text, text_len, "--")) {
        return C_UNARY_OP_PRE_DECREMENT;
    }
    return C_UNARY_OP_INVALID;
}

enum CUnaryOp
c_token_unary_op(Token *token) {
    if (token->kind != TOKEN_OPERATOR) {
        return C_UNARY_OP_INVALID;
    }
    return c_unary_op_from_text(token->text, token->len);
}

enum CUnaryOp
c_token_postfix_unary_op(Token *token) {
    if (token->kind != TOKEN_OPERATOR) {
        return C_UNARY_OP_INVALID;
    }
    if (STREQUAL(token->text, token->len, "++")) {
        return C_UNARY_OP_POST_INCREMENT;
    }
    if (STREQUAL(token->text, token->len, "--")) {
        return C_UNARY_OP_POST_DECREMENT;
    }
    return C_UNARY_OP_INVALID;
}

enum CMemberOp
c_member_op_from_text(char *text, int32 text_len) {
    if (STREQUAL(text, text_len, ".")) {
        return C_MEMBER_OP_DOT;
    }
    if (STREQUAL(text, text_len, "->")) {
        return C_MEMBER_OP_ARROW;
    }
    return C_MEMBER_OP_INVALID;
}

enum CMemberOp
c_token_member_op(Token *token) {
    if ((token->kind != TOKEN_OPERATOR) && (token->kind != TOKEN_PUNCT)) {
        return C_MEMBER_OP_INVALID;
    }
    return c_member_op_from_text(token->text, token->len);
}

enum CKeyword
c_keyword_from_text(char *text, int32 text_len) {
    if (STREQUAL(text, text_len, "_Alignas")) {
        return C_KEYWORD_ALIGNAS;
    }
    if (STREQUAL(text, text_len, "_Alignof")) {
        return C_KEYWORD_ALIGNOF;
    }
    if (STREQUAL(text, text_len, "_Atomic")) {
        return C_KEYWORD_ATOMIC;
    }
    if (STREQUAL(text, text_len, "_Bool")) {
        return C_KEYWORD_BOOL;
    }
    if (STREQUAL(text, text_len, "_Complex")) {
        return C_KEYWORD_COMPLEX;
    }
    if (STREQUAL(text, text_len, "_Generic")) {
        return C_KEYWORD_GENERIC;
    }
    if (STREQUAL(text, text_len, "_Imaginary")) {
        return C_KEYWORD_IMAGINARY;
    }
    if (STREQUAL(text, text_len, "_Noreturn")) {
        return C_KEYWORD_NORETURN;
    }
    if (STREQUAL(text, text_len, "_Static_assert")) {
        return C_KEYWORD_STATIC_ASSERT;
    }
    if (STREQUAL(text, text_len, "_Thread_local")) {
        return C_KEYWORD_THREAD_LOCAL;
    }
    if (STREQUAL(text, text_len, "auto")) {
        return C_KEYWORD_AUTO;
    }
    if (STREQUAL(text, text_len, "break")) {
        return C_KEYWORD_BREAK;
    }
    if (STREQUAL(text, text_len, "case")) {
        return C_KEYWORD_CASE;
    }
    if (STREQUAL(text, text_len, "char")) {
        return C_KEYWORD_CHAR;
    }
    if (STREQUAL(text, text_len, "const")) {
        return C_KEYWORD_CONST;
    }
    if (STREQUAL(text, text_len, "continue")) {
        return C_KEYWORD_CONTINUE;
    }
    if (STREQUAL(text, text_len, "default")) {
        return C_KEYWORD_DEFAULT;
    }
    if (STREQUAL(text, text_len, "do")) {
        return C_KEYWORD_DO;
    }
    if (STREQUAL(text, text_len, "double")) {
        return C_KEYWORD_DOUBLE;
    }
    if (STREQUAL(text, text_len, "else")) {
        return C_KEYWORD_ELSE;
    }
    if (STREQUAL(text, text_len, "enum")) {
        return C_KEYWORD_ENUM;
    }
    if (STREQUAL(text, text_len, "extern")) {
        return C_KEYWORD_EXTERN;
    }
    if (STREQUAL(text, text_len, "float")) {
        return C_KEYWORD_FLOAT;
    }
    if (STREQUAL(text, text_len, "for")) {
        return C_KEYWORD_FOR;
    }
    if (STREQUAL(text, text_len, "goto")) {
        return C_KEYWORD_GOTO;
    }
    if (STREQUAL(text, text_len, "if")) {
        return C_KEYWORD_IF;
    }
    if (STREQUAL(text, text_len, "inline")) {
        return C_KEYWORD_INLINE;
    }
    if (STREQUAL(text, text_len, "int")) {
        return C_KEYWORD_INT;
    }
    if (STREQUAL(text, text_len, "long")) {
        return C_KEYWORD_LONG;
    }
    if (STREQUAL(text, text_len, "register")) {
        return C_KEYWORD_REGISTER;
    }
    if (STREQUAL(text, text_len, "restrict")) {
        return C_KEYWORD_RESTRICT;
    }
    if (STREQUAL(text, text_len, "return")) {
        return C_KEYWORD_RETURN;
    }
    if (STREQUAL(text, text_len, "short")) {
        return C_KEYWORD_SHORT;
    }
    if (STREQUAL(text, text_len, "signed")) {
        return C_KEYWORD_SIGNED;
    }
    if (STREQUAL(text, text_len, "sizeof")) {
        return C_KEYWORD_SIZEOF;
    }
    if (STREQUAL(text, text_len, "static")) {
        return C_KEYWORD_STATIC;
    }
    if (STREQUAL(text, text_len, "struct")) {
        return C_KEYWORD_STRUCT;
    }
    if (STREQUAL(text, text_len, "switch")) {
        return C_KEYWORD_SWITCH;
    }
    if (STREQUAL(text, text_len, "typedef")) {
        return C_KEYWORD_TYPEDEF;
    }
    if (STREQUAL(text, text_len, "union")) {
        return C_KEYWORD_UNION;
    }
    if (STREQUAL(text, text_len, "unsigned")) {
        return C_KEYWORD_UNSIGNED;
    }
    if (STREQUAL(text, text_len, "void")) {
        return C_KEYWORD_VOID;
    }
    if (STREQUAL(text, text_len, "volatile")) {
        return C_KEYWORD_VOLATILE;
    }
    if (STREQUAL(text, text_len, "while")) {
        return C_KEYWORD_WHILE;
    }
    return C_KEYWORD_INVALID;
}

enum CKeyword
c_token_keyword(Token *token) {
    if (token->kind != TOKEN_IDENT) {
        return C_KEYWORD_INVALID;
    }
    return c_keyword_from_text(token->text, token->len);
}

bool
c_text_is_type_qualifier(char *text, int32 text_len) {
    if (STREQUAL(text, text_len, "const")) {
        return true;
    }
    if (STREQUAL(text, text_len, "volatile")) {
        return true;
    }
    if (STREQUAL(text, text_len, "restrict")) {
        return true;
    }
    if (STREQUAL(text, text_len, "__restrict")) {
        return true;
    }
    if (STREQUAL(text, text_len, "__restrict__")) {
        return true;
    }
    return false;
}

static bool
c_text_is_type_identifier(char *text, int32 text_len) {
    if (STREQUAL(text, text_len, "void")) {
        return true;
    }
    if (STREQUAL(text, text_len, "bool")) {
        return true;
    }
    if (STREQUAL(text, text_len, "_Bool")) {
        return true;
    }
    if (STREQUAL(text, text_len, "char")) {
        return true;
    }
    if (STREQUAL(text, text_len, "signed")) {
        return true;
    }
    if (STREQUAL(text, text_len, "unsigned")) {
        return true;
    }
    if (STREQUAL(text, text_len, "short")) {
        return true;
    }
    if (STREQUAL(text, text_len, "int")) {
        return true;
    }
    if (STREQUAL(text, text_len, "long")) {
        return true;
    }
    if (STREQUAL(text, text_len, "float")) {
        return true;
    }
    if (STREQUAL(text, text_len, "double")) {
        return true;
    }
    if (STREQUAL(text, text_len, "size_t")) {
        return true;
    }
    if (STREQUAL(text, text_len, "ssize_t")) {
        return true;
    }
    if (STREQUAL(text, text_len, "intptr_t")) {
        return true;
    }
    if (STREQUAL(text, text_len, "uintptr_t")) {
        return true;
    }
    if (STREQUAL(text, text_len, "ptrdiff_t")) {
        return true;
    }
    if (STREQUAL(text, text_len, "int8_t")) {
        return true;
    }
    if (STREQUAL(text, text_len, "int16_t")) {
        return true;
    }
    if (STREQUAL(text, text_len, "int32_t")) {
        return true;
    }
    if (STREQUAL(text, text_len, "int64_t")) {
        return true;
    }
    if (STREQUAL(text, text_len, "uint8_t")) {
        return true;
    }
    if (STREQUAL(text, text_len, "uint16_t")) {
        return true;
    }
    if (STREQUAL(text, text_len, "uint32_t")) {
        return true;
    }
    if (STREQUAL(text, text_len, "uint64_t")) {
        return true;
    }
    if (STREQUAL(text, text_len, "int8")) {
        return true;
    }
    if (STREQUAL(text, text_len, "int16")) {
        return true;
    }
    if (STREQUAL(text, text_len, "int32")) {
        return true;
    }
    if (STREQUAL(text, text_len, "int64")) {
        return true;
    }
    if (STREQUAL(text, text_len, "uint8")) {
        return true;
    }
    if (STREQUAL(text, text_len, "uint16")) {
        return true;
    }
    if (STREQUAL(text, text_len, "uint32")) {
        return true;
    }
    if (STREQUAL(text, text_len, "uint64")) {
        return true;
    }
    if (STREQUAL(text, text_len, "schar")) {
        return true;
    }
    if (STREQUAL(text, text_len, "uchar")) {
        return true;
    }
    if (STREQUAL(text, text_len, "ushort")) {
        return true;
    }
    if (STREQUAL(text, text_len, "uint")) {
        return true;
    }
    if (STREQUAL(text, text_len, "ulong")) {
        return true;
    }
    if (STREQUAL(text, text_len, "llong")) {
        return true;
    }
    if (STREQUAL(text, text_len, "ldouble")) {
        return true;
    }
    return false;
}

bool
c_token_is_type_qualifier(Token *token) {
    if (token->kind != TOKEN_IDENT) {
        return false;
    }
    return c_text_is_type_qualifier(token->text, token->len);
}

bool
c_text_is_type_word(char *text, int32 text_len) {
    if (c_text_is_type_qualifier(text, text_len)) {
        return true;
    }
    if (c_text_is_type_identifier(text, text_len)) {
        return true;
    }
    return false;
}

bool
c_text_is_declaration_prefix(char *text, int32 text_len) {
    if (STREQUAL(text, text_len, "auto")) {
        return true;
    }
    if (STREQUAL(text, text_len, "extern")) {
        return true;
    }
    if (STREQUAL(text, text_len, "register")) {
        return true;
    }
    if (STREQUAL(text, text_len, "static")) {
        return true;
    }
    if (STREQUAL(text, text_len, "inline")) {
        return true;
    }
    if (STREQUAL(text, text_len, "_Thread_local")) {
        return true;
    }
    if (c_text_is_type_qualifier(text, text_len)) {
        return true;
    }
    return false;
}

bool
c_token_is_type_word(Token *token) {
    if (token->kind != TOKEN_IDENT) {
        return false;
    }
    return c_text_is_type_word(token->text, token->len);
}

bool
c_token_is_declaration_prefix(Token *token) {
    if (token->kind != TOKEN_IDENT) {
        return false;
    }
    return c_text_is_declaration_prefix(token->text, token->len);
}

int32
c_binary_op_precedence(enum CBinaryOp op) {
    switch (op) {
    case C_BINARY_OP_LOGICAL_OR:
        return 1;
    case C_BINARY_OP_LOGICAL_AND:
        return 2;
    case C_BINARY_OP_BIT_OR:
        return 3;
    case C_BINARY_OP_BIT_XOR:
        return 4;
    case C_BINARY_OP_BIT_AND:
        return 5;
    case C_BINARY_OP_EQ:
    case C_BINARY_OP_NE:
        return 6;
    case C_BINARY_OP_LT:
    case C_BINARY_OP_LE:
    case C_BINARY_OP_GT:
    case C_BINARY_OP_GE:
        return 7;
    case C_BINARY_OP_SHL:
    case C_BINARY_OP_SHR:
        return 8;
    case C_BINARY_OP_ADD:
    case C_BINARY_OP_SUB:
        return 9;
    case C_BINARY_OP_MUL:
    case C_BINARY_OP_DIV:
    case C_BINARY_OP_MOD:
        return 10;
    case C_BINARY_OP_INVALID:
    default:
        return 0;
    }
}

int32
precedence_of(char *op, int32 op_len) {
    return c_binary_op_precedence(c_binary_op_from_text(op, op_len));
}

#if TESTING_meta_common
#define CBASE_IMPLEMENT
#include "cbase.h"

static Token
test_token(enum TokenKind kind, char *text) {
    Token token;

    token = (Token){0};
    token.kind = kind;
    token.text = text;
    token.len = strlen32(text);
    return token;
}

static void
test_c_assignment_ops(void) {
    Token token;

    token = test_token(TOKEN_OPERATOR, "+=");
    ASSERT(c_assign_op_from_text("=", STRLIT_LEN("="))
           == C_ASSIGN_OP_ASSIGN);
    ASSERT(c_assign_op_from_text("%=", STRLIT_LEN("%="))
           == C_ASSIGN_OP_MOD);
    ASSERT(c_assign_op_from_text("<<=", STRLIT_LEN("<<="))
           == C_ASSIGN_OP_SHL);
    ASSERT(c_assign_op_from_text(">>=", STRLIT_LEN(">>="))
           == C_ASSIGN_OP_SHR);
    ASSERT(c_token_assign_op(&token) == C_ASSIGN_OP_ADD);

    token = test_token(TOKEN_IDENT, "+=");
    ASSERT(c_token_assign_op(&token) == C_ASSIGN_OP_INVALID);
    return;
}

static void
test_c_binary_ops(void) {
    Token token;

    token = test_token(TOKEN_OPERATOR, "&&");
    ASSERT(c_binary_op_from_text("*", STRLIT_LEN("*"))
           == C_BINARY_OP_MUL);
    ASSERT(c_binary_op_from_text("<=", STRLIT_LEN("<="))
           == C_BINARY_OP_LE);
    ASSERT(c_binary_op_from_text("|", STRLIT_LEN("|"))
           == C_BINARY_OP_BIT_OR);
    ASSERT(c_token_binary_op(&token) == C_BINARY_OP_LOGICAL_AND);
    ASSERT(c_binary_op_precedence(C_BINARY_OP_LOGICAL_OR) == 1);
    ASSERT(c_binary_op_precedence(C_BINARY_OP_MUL) == 10);
    ASSERT(precedence_of("||", STRLIT_LEN("||")) == 1);
    ASSERT(precedence_of("?", STRLIT_LEN("?")) == 0);

    token = test_token(TOKEN_PUNCT, "+");
    ASSERT(c_token_binary_op(&token) == C_BINARY_OP_INVALID);
    return;
}

static void
test_c_unary_ops(void) {
    Token token;

    token = test_token(TOKEN_OPERATOR, "++");
    ASSERT(c_unary_op_from_text("+", STRLIT_LEN("+"))
           == C_UNARY_OP_PLUS);
    ASSERT(c_unary_op_from_text("*", STRLIT_LEN("*"))
           == C_UNARY_OP_DEREFERENCE);
    ASSERT(c_token_unary_op(&token) == C_UNARY_OP_PRE_INCREMENT);
    ASSERT(c_token_postfix_unary_op(&token) == C_UNARY_OP_POST_INCREMENT);

    token = test_token(TOKEN_OPERATOR, "--");
    ASSERT(c_token_unary_op(&token) == C_UNARY_OP_PRE_DECREMENT);
    ASSERT(c_token_postfix_unary_op(&token) == C_UNARY_OP_POST_DECREMENT);
    return;
}

static void
test_c_member_ops(void) {
    Token token;

    token = test_token(TOKEN_OPERATOR, "->");
    ASSERT(c_member_op_from_text(".", STRLIT_LEN(".")) == C_MEMBER_OP_DOT);
    ASSERT(c_token_member_op(&token) == C_MEMBER_OP_ARROW);

    token = test_token(TOKEN_PUNCT, ".");
    ASSERT(c_token_member_op(&token) == C_MEMBER_OP_DOT);
    return;
}

static void
test_c_keywords_and_type_words(void) {
    Token token;

    token = test_token(TOKEN_IDENT, "if");
    ASSERT(c_keyword_from_text("return", STRLIT_LEN("return"))
           == C_KEYWORD_RETURN);
    ASSERT(c_token_keyword(&token) == C_KEYWORD_IF);

    token = test_token(TOKEN_OPERATOR, "if");
    ASSERT(c_token_keyword(&token) == C_KEYWORD_INVALID);

    token = test_token(TOKEN_IDENT, "int32");
    ASSERT(c_text_is_type_word("double", STRLIT_LEN("double")));
    ASSERT(c_text_is_type_qualifier("__restrict__",
                                    STRLIT_LEN("__restrict__")));
    ASSERT(c_text_is_type_word("__restrict__", STRLIT_LEN("__restrict__")));
    ASSERT(c_token_is_type_word(&token));

    token = test_token(TOKEN_IDENT, "static");
    ASSERT(c_text_is_declaration_prefix("extern", STRLIT_LEN("extern")));
    ASSERT(c_text_is_declaration_prefix("restrict", STRLIT_LEN("restrict")));
    ASSERT(!c_token_is_type_qualifier(&token));
    ASSERT(c_token_is_declaration_prefix(&token));

    token = test_token(TOKEN_IDENT, "restrict");
    ASSERT(c_token_is_type_qualifier(&token));

    token = test_token(TOKEN_IDENT, "x");
    ASSERT(c_keyword_from_text("x", STRLIT_LEN("x")) == C_KEYWORD_INVALID);
    ASSERT(!c_token_is_type_word(&token));
    ASSERT(!c_token_is_declaration_prefix(&token));
    return;
}

int
main(void) {
    test_c_assignment_ops();
    test_c_binary_ops();
    test_c_unary_ops();
    test_c_member_ops();
    test_c_keywords_and_type_words();
    exit(EXIT_SUCCESS);
}
#endif

#endif /* META_COMMON_C */
