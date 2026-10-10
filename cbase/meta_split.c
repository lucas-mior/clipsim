// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(META_SPLIT_C)
#define META_SPLIT_C

#if !defined(TESTING_meta_split)
#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_meta_split 1
#else
#define TESTING_meta_split 0
#endif
#endif

#include "cbase.h"

bool
token_range_split_init(TokenRangeSplit *split, Tokenization *tokenization,
                       TokenRange range, char *separator, int32 separator_len) {
    SourceRange source;
    if (split == NULL) {
        return false;
    }
    *split = (TokenRangeSplit){0};
    if (!token_range_is_valid(tokenization, range)
        || (separator == NULL) || (separator_len <= 0)
        || !token_range_is_balanced(tokenization, range)) {
        return false;
    }

    split->tokenization = tokenization;
    split->range = range;
    split->separator = separator;
    split->separator_len = separator_len;
    split->cursor = range.first;

    source = token_range_source_range(tokenization, range);

    split->source_cursor = source.start;
    split->source_end = source.end;
    split->finished = token_range_is_empty(tokenization, range);
    return true;
}

bool
token_range_split_next_spans(TokenRangeSplit *split, TokenRange *result,
                             SourceRange *source_result) {
    Tokenization *tokenization;
    TokenRange range;

    if ((split == NULL) || ((result == NULL) && (source_result == NULL))
        || split->finished || (split->tokenization == NULL)) {
        return false;
    }

    tokenization = split->tokenization;
    range = split->range;
    for (int32 i = split->cursor; i < range.end; i += 1) {
        Token *token = &tokenization->tokens[i];

        if (token_is_open_delimiter(token)) {
            int32 close;

            close = token_range_matching_delimiter_forward(tokenization,
                                                           range, i);
            if (close < 0) {
                split->finished = true;
                return false;
            }
            i = close;
            continue;
        }
        if (TOKEN_IS(token, split->separator, split->separator_len)) {
            if (result != NULL) {
                *result = (TokenRange){split->cursor, i};
            }
            if (source_result != NULL) {
                *source_result = (SourceRange){split->source_cursor,
                                               token->offset};
            }
            split->source_cursor = token->offset + token->len;
            split->cursor = i + 1;
            return true;
        }
    }
    if (result != NULL) {
        *result = (TokenRange){split->cursor, range.end};
    }
    if (source_result != NULL) {
        *source_result = (SourceRange){split->source_cursor,
                                       split->source_end};
    }
    split->finished = true;
    return true;
}

bool
token_range_split_next(TokenRangeSplit *split, TokenRange *result) {
    if (result == NULL) {
        return false;
    }
    return token_range_split_next_spans(split, result, NULL);
}

int32
token_range_split_collect(Tokenization *tokenization, TokenRange range,
                          char *separator, int32 separator_len,
                          TokenRange *items, int32 capacity) {
    TokenRangeSplit split;
    TokenRange item;
    int32 count = 0;

    bool valid = token_range_split_init(&split, tokenization, range,
                                        separator, separator_len);

    if ((capacity < 0) || ((capacity > 0) && (items == NULL)) || !valid) {
        return -1;
    }
    while (token_range_split_next(&split, &item)) {
        if (count < capacity) {
            items[count] = item;
        }
        count += 1;
    }
    return count;
}

#if 0 == TESTING_meta_split
static inline void
meta_split_sink(void) {
    (void)token_range_split_init;
    (void)token_range_split_next;
    (void)token_range_split_next_spans;
    (void)token_range_split_collect;
}
#endif

#if TESTING_meta_split
#define CBASE_IMPLEMENT
#include "cbase.h"

static void
test_split_assert_text(Tokenization *tokenization, TokenRange range,
                       char *expected, int32 expected_len) {
    SourceRange source = token_range_source_range(tokenization, range);

    ASSERT_EQ(source.end - source.start, expected_len);
    ASSERT_EQ(tokenization->text + source.start, expected_len, expected);
    return;
}

static void
test_split_nested_arguments(void) {
    char *text = " a, foo(b, c), (T){1, 2}, array[x, y] ";
    char *expected[] = {"a", "foo(b, c)", "(T){1, 2}", "array[x, y]"};
    Tokenization tokenization = tokenize(text, strlen32(text));
    TokenRange all = {0, tokenization.token_count};
    TokenRange items[4];
    TokenRangeSplit split;
    TokenRange item;
    int32 count;

    count = token_range_split_collect(&tokenization, all, STRLIT(","),
                                      items, LENGTH(items));
    ASSERT_EQ(count, 4);
    ASSERT(token_range_split_init(&split, &tokenization, all, STRLIT(",")));
    for (int32 i = 0; i < count; i += 1) {
        TokenRange trimmed;
        int32 len = strlen32(expected[i]);

        ASSERT(token_range_split_next(&split, &item));
        ASSERT_EQ(item.first, items[i].first);
        ASSERT_EQ(item.end, items[i].end);
        trimmed = token_range_trim_trivia(&tokenization, item);
        test_split_assert_text(&tokenization, trimmed, expected[i], len);
    }
    ASSERT(!token_range_split_next(&split, &item));
    ASSERT(!token_range_split_next(&split, &item));
    test_split_assert_text(&tokenization, items[0], " a", 2);
    test_split_assert_text(&tokenization, items[3], " array[x, y] ", 13);
    free_tokenization(&tokenization);
    return;
}

static void
test_split_empty_elements(void) {
    char *text = " , /*comment*/, x, ";
    Tokenization tokenization = tokenize(text, strlen32(text));
    TokenRange all = {0, tokenization.token_count};
    TokenRange items[4];
    int32 count;

    count = token_range_split_collect(&tokenization, all, STRLIT(","),
                                      items, LENGTH(items));
    ASSERT_EQ(count, 4);
    for (int32 i = 0; i < count; i += 1) {
        TokenRange trimmed = token_range_trim_trivia(&tokenization, items[i]);

        if (i != 2) {
            ASSERT(token_range_is_empty(&tokenization, trimmed));
        } else {
            test_split_assert_text(&tokenization, trimmed, STRLIT("x"));
        }
    }
    free_tokenization(&tokenization);

    for (int32 flags = 0; flags <= TOKENIZE_SKIP_WHITESPACE;
         flags += TOKENIZE_SKIP_WHITESPACE) {
        char *only_trivia = " \n/* no arguments */ ";
        int32 text_len = strlen32(only_trivia);
        Tokenization empty = tokenize_with_flags(only_trivia, text_len, flags);
        TokenRange range = {0, empty.token_count};
        TokenRangeSplit split;
        TokenRange item;

        ASSERT(token_range_split_init(&split, &empty, range, STRLIT(",")));
        ASSERT(!token_range_split_next(&split, &item));
        ASSERT_EQ(token_range_split_collect(&empty, range, STRLIT(","),
                                            NULL, 0), 0);
        free_tokenization(&empty);
    }
    return;
}

static void
test_split_separators_and_collector(void) {
    char *text = ",,f(a, b),";
    Tokenization tokenization = tokenize(text, strlen32(text));
    TokenRange range = {0, tokenization.token_count};
    TokenRange partial[2] = {{-1, -1}, {-1, -1}};
    TokenRange full[5];
    int32 count;

    count = token_range_split_collect(&tokenization, range, STRLIT(","),
                                      NULL, 0);
    ASSERT_EQ(count, 4);
    ASSERT_EQ(token_range_split_collect(&tokenization, range, STRLIT(","),
                                        partial, LENGTH(partial)), 4);
    ASSERT_EQ(token_range_split_collect(&tokenization, range, STRLIT(","),
                                        full, LENGTH(full)), 4);
    for (int32 i = 0; i < 2; i += 1) {
        ASSERT_EQ(partial[i].first, full[i].first);
        ASSERT_EQ(partial[i].end, full[i].end);
    }
    ASSERT_EQ(full[0].first, full[0].end);
    ASSERT_EQ(full[1].first, full[1].end);
    test_split_assert_text(&tokenization, full[2], STRLIT("f(a, b)"));
    ASSERT_EQ(full[3].first, full[3].end);
    ASSERT_EQ(full[3].first, range.end);
    ASSERT_EQ(token_range_split_collect(&tokenization, range, STRLIT(";"),
                                        NULL, 0), 1);
    free_tokenization(&tokenization);
    return;
}

static void
test_split_literals_comments_and_flags(void) {
    char *text = "\"a,b\", /* , */ f(1, 2), (int[]){3, 4}";
    char *expected[] = {"\"a,b\"", "f(1, 2)", "(int[]){3, 4}"};
    int32 flags[] = {TOKENIZE_DEFAULT, TOKENIZE_SKIP_WHITESPACE};

    for (int32 i = 0; i < LENGTH(flags); i += 1) {
        Tokenization t = tokenize_with_flags(text, strlen32(text), flags[i]);
        TokenRange all = {0, t.token_count};
        TokenRangeSplit split;
        TokenRange item;
        int32 count = 0;

        ASSERT(token_range_split_init(&split, &t, all, STRLIT(",")));
        while (token_range_split_next(&split, &item)) {
            TokenRange trimmed = token_range_trim_trivia(&t, item);

            ASSERT_LT_VAR(count, LENGTH(expected));
            if (count == 1) {
                /* The comment is trivia in the untrimmed second item. */
                int32 first = token_range_first_significant(&t, item);

                ASSERT_EQ(t.tokens[first].kind, TOKEN_IDENT);
            }
            test_split_assert_text(&t, trimmed, expected[count],
                                   strlen32(expected[count]));
            count += 1;
        }
        ASSERT_EQ(count, 3);
        free_tokenization(&t);
    }
    return;
}

static void
test_split_source_spans_without_whitespace_tokens(void) {
    char *text = "f(1, 2),   /*hi*/ g(3, 4),  z";
    char *expected[] = {"f(1, 2)", "   /*hi*/ g(3, 4)", "  z"};
    Tokenization t = tokenize_with_flags(text, strlen32(text),
                                         TOKENIZE_SKIP_WHITESPACE);
    TokenRange all = {0, t.token_count};
    TokenRangeSplit split;
    TokenRange item;
    SourceRange source;
    int32 count = 0;

    ASSERT(token_range_split_init(&split, &t, all, STRLIT(",")));
    while (token_range_split_next_spans(&split, &item, &source)) {
        int32 len = strlen32(expected[count]);

        ASSERT_LT_VAR(count, LENGTH(expected));
        ASSERT_EQ(source.end - source.start, len);
        ASSERT_EQ(t.text + source.start, len, expected[count]);
        if (count == 1) {
            test_split_assert_text(&t, token_range_trim_trivia(&t, item),
                                   STRLIT("g(3, 4)"));
        }
        count += 1;
    }
    ASSERT_EQ(count, 3);
    ASSERT(!token_range_split_next_spans(&split, &item, &source));
    free_tokenization(&t);
    return;
}

static void
test_split_deep_nesting(void) {
    char text[256];
    int32 pos = 0;
    int32 depth = 70;
    TokenRange items[2];

    for (int32 i = 0; i < depth; i += 1) {
        text[pos] = '(';
        pos += 1;
    }
    text[pos] = 'x';
    pos += 1;
    for (int32 i = 0; i < depth; i += 1) {
        text[pos] = ')';
        pos += 1;
    }
    text[pos] = ',';
    pos += 1;
    text[pos] = 'y';
    pos += 1;
    text[pos] = '\0';

    {
        Tokenization t = tokenize(text, pos);
        TokenRange all = {0, t.token_count};

        ASSERT_EQ(token_range_split_collect(&t, all, STRLIT(","), items, 2), 2);
        ASSERT_EQ(items[1].end - items[1].first, 1);
        test_split_assert_text(&t, items[1], STRLIT("y"));
        free_tokenization(&t);
    }
    return;
}

static void
test_split_invalid_and_unbalanced(void) {
    char *bad[] = {"a, ([)] , z", "a, (b", "a, b]", "[ a }"};

    for (int32 i = 0; i < LENGTH(bad); i += 1) {
        Tokenization t = tokenize(bad[i], strlen32(bad[i]));
        TokenRange all = {0, t.token_count};
        TokenRangeSplit split;
        TokenRange result;

        ASSERT(!token_range_split_init(&split, &t, all, STRLIT(",")));
        ASSERT(!token_range_split_next(&split, &result));
        ASSERT_EQ(token_range_split_collect(&t, all, STRLIT(","), NULL, 0), -1);
        free_tokenization(&t);
    }

    {
        Tokenization t = tokenize("a,b", 3);
        TokenRange all = {0, t.token_count};
        TokenRangeSplit split;

        ASSERT(!token_range_split_init(NULL, &t, all, STRLIT(",")));
        ASSERT(!token_range_split_init(&split, NULL, all, STRLIT(",")));
        ASSERT(!token_range_split_init(&split, &t,
                                       (TokenRange){-1, all.end},
                                       STRLIT(",")));
        ASSERT(!token_range_split_init(&split, &t, all, NULL, 1));
        ASSERT(!token_range_split_init(&split, &t, all, "", 0));
        ASSERT_EQ(token_range_split_collect(&t, all, STRLIT(","),
                                            NULL, -1), -1);
        ASSERT_EQ(token_range_split_collect(&t, all, STRLIT(","),
                                            NULL, 2), -1);
        free_tokenization(&t);
    }
    return;
}

static void
test_split_raw_macro_parameters(void) {
    char *text = "#define F(x, y, ...) (x)\n";
    Tokenization t = tokenize(text, strlen32(text));
    TokenRange all = {0, t.token_count};
    CPreprocessorDirective directive;
    CPreprocessorDefine define;
    TokenRange items[3];
    int32 hash;

    hash = token_range_next_text(&t, all, -1, STRLIT("#"));
    ASSERT(c_preprocessor_directive_at(&t, hash, &directive));
    ASSERT(c_preprocessor_define_info(&t, &directive, &define));
    ASSERT_EQ(token_range_split_collect(&t, define.parameters, STRLIT(","),
                                        items, LENGTH(items)), 3);
    test_split_assert_text(&t, token_range_trim_trivia(&t, items[0]),
                           STRLIT("x"));
    test_split_assert_text(&t, token_range_trim_trivia(&t, items[1]),
                           STRLIT("y"));
    test_split_assert_text(&t, token_range_trim_trivia(&t, items[2]),
                           STRLIT("..."));
    free_tokenization(&t);
    return;
}

int32
main(void) {
    test_split_nested_arguments();
    test_split_empty_elements();
    test_split_separators_and_collector();
    test_split_literals_comments_and_flags();
    test_split_source_spans_without_whitespace_tokens();
    test_split_deep_nesting();
    test_split_invalid_and_unbalanced();
    test_split_raw_macro_parameters();
    exit(0);
}
#endif /* TESTING_meta_split */

#endif /* META_SPLIT_C */
