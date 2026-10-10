// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(META_PREPROC_C)
#define META_PREPROC_C

#if !defined(TESTING_meta_preproc)
#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_meta_preproc 1
#else
#define TESTING_meta_preproc 0
#endif
#endif

#include "cbase.h"

static int32
preprocessor_logical_line_end_offset(Tokenization *tokenization,
                                     int32 offset) {
    if ((tokenization == NULL) || (offset < 0)
        || (offset > tokenization->text_len)) {
        return -1;
    }

    for (int32 i = offset; i < tokenization->text_len; i += 1) {
        int32 previous;

        if (tokenization->text[i] != '\n') {
            continue;
        }
        previous = i - 1;
        if ((previous >= offset) && (tokenization->text[previous] == '\r')) {
            previous -= 1;
        }
        if ((previous >= offset) && (tokenization->text[previous] == '\\')) {
            continue;
        }
        return i + 1;
    }
    return tokenization->text_len;
}

static int32
preprocessor_directive_content_end(Tokenization *tokenization,
                                   SourceRange source) {
    int32 result;

    result = source.end;
    if ((result > source.start) && (tokenization->text[result - 1] == '\n')) {
        result -= 1;
        if ((result > source.start)
            && (tokenization->text[result - 1] == '\r')) {
            result -= 1;
        }
    }
    return result;
}

static TokenRange
preprocessor_source_token_range(Tokenization *tokenization,
                                SourceRange source) {
    TokenRange result;

    result.first = tokenization_token_at_or_after_offset(tokenization,
                                                         source.start);
    result.end = tokenization_token_at_or_after_offset(tokenization,
                                                       source.end);
    return result;
}

static bool
preprocessor_token_is_hash(Token *token) {
    return TOKEN_IS(token, "#") || TOKEN_IS(token, "%:");
}

static bool
preprocessor_token_is_line_splice(Tokenization *tokenization,
                                  int32 token_index) {
    Token *token;
    int32 offset;

    if ((token_index < 0) || (token_index >= tokenization->token_count)) {
        return false;
    }
    token = &tokenization->tokens[token_index];
    if (!TOKEN_IS(token, "\\")) {
        return false;
    }

    offset = token->offset + token->len;
    if (offset >= tokenization->text_len) {
        return false;
    }
    if (tokenization->text[offset] == '\n') {
        return true;
    }
    return ((offset + 1) < tokenization->text_len)
           && (tokenization->text[offset] == '\r')
           && (tokenization->text[offset + 1] == '\n');
}

static bool
preprocessor_token_is_trivia(Tokenization *tokenization, int32 token_index) {
    return token_is_trivia(&tokenization->tokens[token_index])
           || preprocessor_token_is_line_splice(tokenization, token_index);
}

static int32
preprocessor_next_token(Tokenization *tokenization, TokenRange range,
                        int32 token_index) {
    int32 result;

    if (!token_range_is_valid(tokenization, range)) {
        return -1;
    }
    if (token_index < range.first) {
        result = range.first;
    } else {
        result = token_index + 1;
    }
    while ((result < range.end)
           && preprocessor_token_is_trivia(tokenization, result)) {
        result += 1;
    }
    return result;
}

static int32
preprocessor_previous_token(Tokenization *tokenization, TokenRange range,
                            int32 token_index) {
    int32 result;

    if (!token_range_is_valid(tokenization, range)) {
        return -1;
    }
    if (token_index >= range.end) {
        result = range.end - 1;
    } else {
        result = token_index - 1;
    }
    while ((result >= range.first)
           && preprocessor_token_is_trivia(tokenization, result)) {
        result -= 1;
    }
    return result;
}

static TokenRange
preprocessor_trim_range(Tokenization *tokenization, TokenRange range) {
    int32 first;
    int32 last;

    if (!token_range_is_valid(tokenization, range)) {
        return (TokenRange){-1, -1};
    }
    first = preprocessor_next_token(tokenization, range, range.first - 1);
    if (first >= range.end) {
        return (TokenRange){range.end, range.end};
    }
    last = preprocessor_previous_token(tokenization, range, range.end);
    return (TokenRange){first, last + 1};
}

static bool
preprocessor_source_is_only_line_splices(Tokenization *tokenization,
                                         int32 start, int32 end) {
    int32 i;

    if ((start < 0) || (start > end) || (end > tokenization->text_len)) {
        return false;
    }
    i = start;
    while (i < end) {
        if (tokenization->text[i] != '\\') {
            return false;
        }
        i += 1;
        if ((i < end) && (tokenization->text[i] == '\r')) {
            i += 1;
        }
        if ((i >= end) || (tokenization->text[i] != '\n')) {
            return false;
        }
        i += 1;
    }
    return true;
}

enum CPreprocessorDirectiveKind
c_preprocessor_directive_kind(Token *token) {
    if ((token == NULL) || (token->kind != TOKEN_IDENT)) {
        return C_PREPROCESSOR_DIRECTIVE_UNKNOWN;
    }
    if (TOKEN_IS(token, "define")) {
        return C_PREPROCESSOR_DIRECTIVE_DEFINE;
    }
    if (TOKEN_IS(token, "undef")) {
        return C_PREPROCESSOR_DIRECTIVE_UNDEF;
    }
    if (TOKEN_IS(token, "include")) {
        return C_PREPROCESSOR_DIRECTIVE_INCLUDE;
    }
    if (TOKEN_IS(token, "if")) {
        return C_PREPROCESSOR_DIRECTIVE_IF;
    }
    if (TOKEN_IS(token, "ifdef")) {
        return C_PREPROCESSOR_DIRECTIVE_IFDEF;
    }
    if (TOKEN_IS(token, "ifndef")) {
        return C_PREPROCESSOR_DIRECTIVE_IFNDEF;
    }
    if (TOKEN_IS(token, "elif")) {
        return C_PREPROCESSOR_DIRECTIVE_ELIF;
    }
    if (TOKEN_IS(token, "else")) {
        return C_PREPROCESSOR_DIRECTIVE_ELSE;
    }
    if (TOKEN_IS(token, "endif")) {
        return C_PREPROCESSOR_DIRECTIVE_ENDIF;
    }
    if (TOKEN_IS(token, "error")) {
        return C_PREPROCESSOR_DIRECTIVE_ERROR;
    }
    if (TOKEN_IS(token, "pragma")) {
        return C_PREPROCESSOR_DIRECTIVE_PRAGMA;
    }
    if (TOKEN_IS(token, "line")) {
        return C_PREPROCESSOR_DIRECTIVE_LINE;
    }
    return C_PREPROCESSOR_DIRECTIVE_UNKNOWN;
}

static bool
preprocessor_directive_from_source(Tokenization *tokenization,
                                   SourceRange source,
                                   CPreprocessorDirective *result) {
    CPreprocessorDirective directive = {0};
    TokenRange tokens;
    Token *keyword_token;
    int32 first;
    int32 keyword;

    if ((result == NULL) || (source.start < 0)
        || (source.start > source.end)
        || (source.end > tokenization->text_len)) {
        return false;
    }
    tokens = preprocessor_source_token_range(tokenization, source);
    if (!token_range_is_valid(tokenization, tokens)) {
        return false;
    }
    first = preprocessor_next_token(tokenization, tokens, tokens.first - 1);
    if ((first >= tokens.end)
        || !preprocessor_token_is_hash(&tokenization->tokens[first])) {
        return false;
    }

    keyword = preprocessor_next_token(tokenization, tokens, first);
    directive.source = source;
    directive.tokens = tokens;
    directive.hash_token = first;
    directive.keyword_token = -1;
    directive.kind = C_PREPROCESSOR_DIRECTIVE_UNKNOWN;
    if (keyword < tokens.end) {
        directive.keyword_token = keyword;
        keyword_token = &tokenization->tokens[keyword];
        directive.kind = c_preprocessor_directive_kind(keyword_token);
    }
    *result = directive;
    return true;
}

bool
c_preprocessor_directive_at(Tokenization *tokenization, int32 token_index,
                            CPreprocessorDirective *result) {
    SourceRange source;
    Token *token;
    int32 start;
    int32 end;

    if ((tokenization == NULL) || (result == NULL)
        || (token_index < 0) || (token_index >= tokenization->token_count)) {
        return false;
    }
    token = &tokenization->tokens[token_index];
    if (!preprocessor_token_is_hash(token)) {
        return false;
    }
    start = tokenization_logical_line_start_offset(tokenization, token->offset);
    end = preprocessor_logical_line_end_offset(tokenization, start);
    if (end < 0) {
        return false;
    }
    source = (SourceRange){start, end};
    if (!preprocessor_directive_from_source(tokenization, source, result)) {
        return false;
    }
    return result->hash_token == token_index;
}

bool
c_preprocessor_directive_containing(Tokenization *tokenization,
                                    int32 token_index,
                                    CPreprocessorDirective *result) {
    SourceRange source;
    Token *token;
    int32 start;
    int32 end;

    if ((tokenization == NULL) || (result == NULL)
        || (token_index < 0) || (token_index >= tokenization->token_count)) {
        return false;
    }
    token = &tokenization->tokens[token_index];
    start = tokenization_logical_line_start_offset(tokenization, token->offset);
    end = preprocessor_logical_line_end_offset(tokenization, start);
    if (end < 0) {
        return false;
    }
    source = (SourceRange){start, end};
    return preprocessor_directive_from_source(tokenization, source, result);
}

bool
c_preprocessor_define_info(Tokenization *tokenization,
                           CPreprocessorDirective *directive,
                           CPreprocessorDefine *result) {
    CPreprocessorDefine info = {0};
    TokenRange tail;
    int32 name;
    int32 next;
    int32 syntax_end;
    int32 content_end;
    int32 name_end;
    int32 next_offset;
    bool adjacent;

    if ((tokenization == NULL) || (directive == NULL) || (result == NULL)
        || (directive->kind != C_PREPROCESSOR_DIRECTIVE_DEFINE)
        || !token_range_is_valid(tokenization, directive->tokens)) {
        return false;
    }
    name = preprocessor_next_token(tokenization, directive->tokens,
                                   directive->keyword_token);
    if ((name >= directive->tokens.end)
        || (tokenization->tokens[name].kind != TOKEN_IDENT)) {
        return false;
    }

    info.directive = *directive;
    info.name_token = name;
    info.open_paren_token = -1;
    info.close_paren_token = -1;
    info.parameter_list = (TokenRange){name + 1, name + 1};
    info.parameters = (TokenRange){name + 1, name + 1};
    next = preprocessor_next_token(tokenization, directive->tokens, name);
    name_end = tokenization->tokens[name].offset
               + tokenization->tokens[name].len;
    adjacent = false;
    if (next < directive->tokens.end) {
        next_offset = tokenization->tokens[next].offset;
        adjacent =
            preprocessor_source_is_only_line_splices(tokenization, name_end,
                                                     next_offset);
    }
    if ((next < directive->tokens.end)
        && TOKEN_IS(&tokenization->tokens[next], "(") && adjacent) {
        int32 close;

        close = token_range_matching_delimiter_forward(tokenization,
                                                       directive->tokens, next);
        if (close < 0) {
            return false;
        }
        info.function_like = true;
        info.open_paren_token = next;
        info.close_paren_token = close;
        info.parameter_list = (TokenRange){next, close + 1};
        info.parameters = (TokenRange){next + 1, close};
        syntax_end = close;
    } else {
        syntax_end = name;
    }

    tail = (TokenRange){syntax_end + 1, directive->tokens.end};
    info.replacement = preprocessor_trim_range(tokenization, tail);
    content_end = preprocessor_directive_content_end(tokenization,
                                                     directive->source);
    info.replacement_source.start = tokenization->tokens[syntax_end].offset
                                    + tokenization->tokens[syntax_end].len;
    info.replacement_source.end = content_end;
    *result = info;
    return true;
}

int32
c_preprocessor_define_parameter_count(Tokenization *tokenization,
                                      CPreprocessorDefine *define) {
    TokenDelimiterDepth zero = {0};
    TokenRange parameters;
    int32 count;
    int32 comma;

    if ((tokenization == NULL) || (define == NULL) || !define->function_like
        || !token_range_is_valid(tokenization, define->parameters)) {
        return 0;
    }
    parameters = preprocessor_trim_range(tokenization, define->parameters);
    if (parameters.first >= parameters.end) {
        return 0;
    }

    count = 1;
    comma = token_range_next_text_at_depth(tokenization, parameters,
                                           parameters.first - 1, zero,
                                           STRLIT(","));
    while (comma < parameters.end) {
        count += 1;
        comma = token_range_next_text_at_depth(tokenization, parameters,
                                               comma, zero, STRLIT(","));
    }
    return count;
}

bool
c_preprocessor_define_parameter(Tokenization *tokenization,
                                CPreprocessorDefine *define,
                                int32 parameter_index, TokenRange *result) {
    TokenDelimiterDepth zero = {0};
    TokenRange parameters;
    int32 first;

    if ((tokenization == NULL) || (define == NULL) || (result == NULL)
        || (parameter_index < 0) || !define->function_like
        || !token_range_is_valid(tokenization, define->parameters)) {
        return false;
    }
    parameters = preprocessor_trim_range(tokenization, define->parameters);
    if (parameters.first >= parameters.end) {
        return false;
    }

    first = parameters.first;
    for (int32 i = 0; ; i += 1) {
        TokenRange parameter;
        int32 comma;

        comma = token_range_next_text_at_depth(tokenization, parameters,
                                               first - 1, zero, STRLIT(","));
        parameter = (TokenRange){first, comma};
        if (comma >= parameters.end) {
            parameter.end = parameters.end;
        }
        parameter = preprocessor_trim_range(tokenization, parameter);
        if (i == parameter_index) {
            *result = parameter;
            return true;
        }
        if (comma >= parameters.end) {
            return false;
        }
        first = comma + 1;
    }
}

#if 0 == TESTING_meta_preproc
static inline void
meta_preproc_sink(void) {
    (void)c_preprocessor_directive_at;
    (void)c_preprocessor_directive_containing;
    (void)c_preprocessor_directive_kind;
    (void)c_preprocessor_define_info;
    (void)c_preprocessor_define_parameter;
    (void)c_preprocessor_define_parameter_count;
}
#endif

#if TESTING_meta_preproc
#define CBASE_IMPLEMENT
#include "cbase.h"

static int32
test_find_text(Tokenization *tokenization, char *text, int32 text_len) {
    TokenRange all;

    all = (TokenRange){0, tokenization->token_count};
    return token_range_next_text(tokenization, all, -1, text, text_len);
}

static void
test_assert_range_text(Tokenization *tokenization, TokenRange range,
                       char *expected, int32 expected_len) {
    SourceRange source;

    source = token_range_source_range(tokenization, range);
    ASSERT_EQ(source.end - source.start, expected_len);
    ASSERT_EQ(tokenization->text + source.start, expected_len, expected);
    return;
}

static void
test_preprocessor_directive_kinds(void) {
    struct DirectiveCase {
        char *name;
        enum CPreprocessorDirectiveKind kind;
    } cases[] = {
        {"define", C_PREPROCESSOR_DIRECTIVE_DEFINE},
        {"undef", C_PREPROCESSOR_DIRECTIVE_UNDEF},
        {"include", C_PREPROCESSOR_DIRECTIVE_INCLUDE},
        {"if", C_PREPROCESSOR_DIRECTIVE_IF},
        {"ifdef", C_PREPROCESSOR_DIRECTIVE_IFDEF},
        {"ifndef", C_PREPROCESSOR_DIRECTIVE_IFNDEF},
        {"elif", C_PREPROCESSOR_DIRECTIVE_ELIF},
        {"else", C_PREPROCESSOR_DIRECTIVE_ELSE},
        {"endif", C_PREPROCESSOR_DIRECTIVE_ENDIF},
        {"error", C_PREPROCESSOR_DIRECTIVE_ERROR},
        {"pragma", C_PREPROCESSOR_DIRECTIVE_PRAGMA},
        {"line", C_PREPROCESSOR_DIRECTIVE_LINE},
    };

    for (int32 i = 0; i < LENGTH(cases); i += 1) {
        Token token = {0};

        token.kind = TOKEN_IDENT;
        token.text = cases[i].name;
        token.len = strlen32(cases[i].name);
        ASSERT(c_preprocessor_directive_kind(&token) == cases[i].kind);
    }

    {
        Token token = {0};

        token.kind = TOKEN_IDENT;
        token.text = "warning";
        token.len = strlen32(token.text);
        ASSERT(c_preprocessor_directive_kind(&token)
               == C_PREPROCESSOR_DIRECTIVE_UNKNOWN);
        token.kind = TOKEN_LITERAL;
        ASSERT(c_preprocessor_directive_kind(&token)
               == C_PREPROCESSOR_DIRECTIVE_UNKNOWN);
    }
    ASSERT(c_preprocessor_directive_kind(NULL)
           == C_PREPROCESSOR_DIRECTIVE_UNKNOWN);
    return;
}

static void
test_preprocessor_logical_directive_range(void) {
    char *text = "  #define ADD(a, b) ((a) + \\\n"
                 "                    (b))\n"
                 "int x;\n";
    Tokenization tokenization;
    CPreprocessorDirective directive;
    TokenRange all;
    int32 hash;
    int32 b;
    int32 x;

    tokenization = tokenize(text, strlen32(text));
    hash = test_find_text(&tokenization, STRLIT("#"));
    b = test_find_text(&tokenization, STRLIT("b"));
    all = (TokenRange){0, tokenization.token_count};
    b = token_range_next_text(&tokenization, all, b, STRLIT("b"));
    x = test_find_text(&tokenization, STRLIT("int"));

    ASSERT(c_preprocessor_directive_at(&tokenization, hash, &directive));
    ASSERT(directive.kind == C_PREPROCESSOR_DIRECTIVE_DEFINE);
    ASSERT_ZERO(directive.source.start);
    ASSERT_EQ(directive.source.end,
              strlen32("  #define ADD(a, b) ((a) + \\\n"
                       "                    (b))\n"));
    ASSERT(c_preprocessor_directive_containing(&tokenization, b,
                                               &directive));
    ASSERT(directive.kind == C_PREPROCESSOR_DIRECTIVE_DEFINE);
    ASSERT(!c_preprocessor_directive_containing(&tokenization, x,
                                                &directive));
    ASSERT(!c_preprocessor_directive_at(&tokenization, b, &directive));
    free_tokenization(&tokenization);
    return;
}

static void
test_preprocessor_define_info(void) {
    char *text = "#define FN(a, b, ...) ((a) + (b))\n";
    Tokenization tokenization;
    CPreprocessorDirective directive;
    CPreprocessorDefine define;
    TokenRange parameter;
    int32 hash;

    tokenization = tokenize(text, strlen32(text));
    hash = test_find_text(&tokenization, STRLIT("#"));
    ASSERT(c_preprocessor_directive_at(&tokenization, hash, &directive));
    ASSERT(c_preprocessor_define_info(&tokenization, &directive, &define));
    ASSERT(define.function_like);
    ASSERT(TOKEN_IS(&tokenization.tokens[define.name_token], "FN"));
    ASSERT(TOKEN_IS(&tokenization.tokens[define.open_paren_token], "("));
    ASSERT(TOKEN_IS(&tokenization.tokens[define.close_paren_token], ")"));
    test_assert_range_text(&tokenization, define.parameter_list,
                           STRLIT("(a, b, ...)"));
    ASSERT_EQ(c_preprocessor_define_parameter_count(&tokenization, &define), 3);

    ASSERT(c_preprocessor_define_parameter(&tokenization, &define, 0,
                                           &parameter));
    test_assert_range_text(&tokenization, parameter, STRLIT("a"));
    ASSERT(c_preprocessor_define_parameter(&tokenization, &define, 1,
                                           &parameter));
    test_assert_range_text(&tokenization, parameter, STRLIT("b"));
    ASSERT(c_preprocessor_define_parameter(&tokenization, &define, 2,
                                           &parameter));
    test_assert_range_text(&tokenization, parameter, STRLIT("..."));
    ASSERT(!c_preprocessor_define_parameter(&tokenization, &define, 3,
                                            &parameter));
    test_assert_range_text(&tokenization, define.replacement,
                           STRLIT("((a) + (b))"));
    ASSERT_EQ(define.replacement_source.end - define.replacement_source.start,
              strlen32(" ((a) + (b))"));
    ASSERT_EQ(tokenization.text + define.replacement_source.start,
              define.replacement_source.end - define.replacement_source.start,
              " ((a) + (b))");
    free_tokenization(&tokenization);
    return;
}

static void
test_preprocessor_object_and_function_spacing(void) {
    char *text = "#define OBJ (x)\n"
                 "#define FN(x) x\n"
                 "#define SPLICE\\\n"
                 "(x) x\n"
                 "#define COMMENT/* c */(x) x\n";
    Tokenization tokenization;
    TokenRange all;
    int32 cursor;
    bool expected[] = {false, true, true, false};

    tokenization = tokenize(text, strlen32(text));
    all = (TokenRange){0, tokenization.token_count};
    cursor = -1;
    for (int32 i = 0; i < LENGTH(expected); i += 1) {
        CPreprocessorDirective directive;
        CPreprocessorDefine define;
        int32 hash;

        hash = token_range_next_text(&tokenization, all, cursor, STRLIT("#"));
        ASSERT_LT_VAR(hash, all.end);
        ASSERT(c_preprocessor_directive_at(&tokenization, hash, &directive));
        ASSERT(c_preprocessor_define_info(&tokenization, &directive, &define));
        ASSERT_EQ(define.function_like, expected[i]);
        cursor = directive.tokens.end - 1;
    }
    free_tokenization(&tokenization);
    return;
}

static void
test_preprocessor_empty_define_and_malformed(void) {
    char *text = "#define EMPTY\n"
                 "#define ZERO()\n"
                 "#define BROKEN(x\n";
    Tokenization tokenization;
    TokenRange all;
    int32 first_hash;
    int32 second_hash;
    int32 third_hash;
    CPreprocessorDirective directive;
    CPreprocessorDefine define;

    tokenization = tokenize(text, strlen32(text));
    all = (TokenRange){0, tokenization.token_count};
    first_hash = token_range_next_text(&tokenization, all, -1, STRLIT("#"));
    second_hash = token_range_next_text(&tokenization, all, first_hash,
                                        STRLIT("#"));
    third_hash = token_range_next_text(&tokenization, all, second_hash,
                                       STRLIT("#"));

    ASSERT(c_preprocessor_directive_at(&tokenization, first_hash, &directive));
    ASSERT(c_preprocessor_define_info(&tokenization, &directive, &define));
    ASSERT(!define.function_like);
    ASSERT(token_range_is_empty(&tokenization, define.replacement));

    ASSERT(c_preprocessor_directive_at(&tokenization, second_hash, &directive));
    ASSERT(c_preprocessor_define_info(&tokenization, &directive, &define));
    ASSERT(define.function_like);
    ASSERT_ZERO(c_preprocessor_define_parameter_count(&tokenization, &define));
    ASSERT(token_range_is_empty(&tokenization, define.replacement));

    ASSERT(c_preprocessor_directive_at(&tokenization, third_hash, &directive));
    ASSERT(!c_preprocessor_define_info(&tokenization, &directive, &define));
    free_tokenization(&tokenization);
    return;
}

static void
test_preprocessor_unknown_and_non_directive_hash(void) {
    char *text = "#warning hi\n"
                 "x # define NOPE 1\n"
                 "# 123 \"file.c\"\n";
    Tokenization tokenization;
    TokenRange all;
    CPreprocessorDirective directive;
    int32 first_hash;
    int32 second_hash;
    int32 third_hash;

    tokenization = tokenize(text, strlen32(text));
    all = (TokenRange){0, tokenization.token_count};
    first_hash = token_range_next_text(&tokenization, all, -1, STRLIT("#"));
    second_hash = token_range_next_text(&tokenization, all, first_hash,
                                        STRLIT("#"));
    third_hash = token_range_next_text(&tokenization, all, second_hash,
                                       STRLIT("#"));

    ASSERT(c_preprocessor_directive_at(&tokenization, first_hash, &directive));
    ASSERT(directive.kind == C_PREPROCESSOR_DIRECTIVE_UNKNOWN);
    ASSERT(!c_preprocessor_directive_at(&tokenization, second_hash,
                                        &directive));
    ASSERT(c_preprocessor_directive_at(&tokenization, third_hash, &directive));
    ASSERT(directive.kind == C_PREPROCESSOR_DIRECTIVE_UNKNOWN);
    free_tokenization(&tokenization);
    return;
}

static void
test_preprocessor_hash_digraph(void) {
    char *text = "%:define VALUE 3\r\n";
    Tokenization tokenization;
    CPreprocessorDirective directive;
    CPreprocessorDefine define;
    int32 hash;

    tokenization = tokenize(text, strlen32(text));
    hash = test_find_text(&tokenization, STRLIT("%:"));
    ASSERT(c_preprocessor_directive_at(&tokenization, hash, &directive));
    ASSERT(directive.kind == C_PREPROCESSOR_DIRECTIVE_DEFINE);
    ASSERT_EQ(directive.source.end, strlen32(text));
    ASSERT(c_preprocessor_define_info(&tokenization, &directive, &define));
    ASSERT(TOKEN_IS(&tokenization.tokens[define.name_token], "VALUE"));
    test_assert_range_text(&tokenization, define.replacement, STRLIT("3"));
    free_tokenization(&tokenization);
    return;
}

static void
test_preprocessor_skip_whitespace_tokens(void) {
    char *text = "  # define FN(a, \\\n"
                 "b) a + b\n"
                 "int x;\n";
    Tokenization tokenization;
    CPreprocessorDirective directive;
    CPreprocessorDefine define;
    TokenRange parameter;
    int32 hash;

    tokenization = tokenize_with_flags(text, strlen32(text),
                                       TOKENIZE_SKIP_WHITESPACE);
    hash = test_find_text(&tokenization, STRLIT("#"));
    ASSERT(c_preprocessor_directive_at(&tokenization, hash, &directive));
    ASSERT(directive.kind == C_PREPROCESSOR_DIRECTIVE_DEFINE);
    ASSERT_EQ(directive.source.start, 0);
    ASSERT_EQ(directive.source.end,
              strlen32("  # define FN(a, \\\n"
                       "b) a + b\n"));
    ASSERT(c_preprocessor_define_info(&tokenization, &directive, &define));
    ASSERT(define.function_like);
    ASSERT_EQ(c_preprocessor_define_parameter_count(&tokenization, &define), 2);
    ASSERT(c_preprocessor_define_parameter(&tokenization, &define, 1,
                                           &parameter));
    test_assert_range_text(&tokenization, parameter, STRLIT("b"));
    test_assert_range_text(&tokenization, define.replacement,
                           STRLIT("a + b"));
    free_tokenization(&tokenization);
    return;
}

int
main(void) {
    test_preprocessor_directive_kinds();
    test_preprocessor_logical_directive_range();
    test_preprocessor_define_info();
    test_preprocessor_object_and_function_spacing();
    test_preprocessor_empty_define_and_malformed();
    test_preprocessor_unknown_and_non_directive_hash();
    test_preprocessor_hash_digraph();
    test_preprocessor_skip_whitespace_tokens();
    return 0;
}

#endif /* TESTING_meta_preproc */

#endif /* META_PREPROC_C */
