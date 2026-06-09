#if !defined(META_PARSE_C)
#define META_PARSE_C

#include "meta_tokenize.c"

#define PARSE_INITIAL_LINE_CAPACITY 128

static void
free_line(Line *line) {
    free_line_tokens(line);
    if (line->text) {
        free2(line->text, line->len + 1);
    }
    line->text = NULL;
    line->len = 0;
    return;
}

static void
free_document(Document *doc) {
    for (int32 i = 0; i < doc->line_count; i += 1) {
        free_line(&doc->lines[i]);
    }
    free2(doc->lines, doc->capacity*SIZEOF(*doc->lines));
    free2(doc, SIZEOF(*doc));
    return;
}

static void
document_reserve_lines(Document *doc, int32 extra) {
    int32 need;
    int32 new_capacity;

    need = doc->line_count + extra;
    if (need <= doc->capacity) {
        return;
    }

    new_capacity = doc->capacity;
    while (new_capacity < need) {
        new_capacity *= 2;
    }
    doc->lines = realloc2(doc->lines, doc->capacity, new_capacity,
                          SIZEOF(*doc->lines));
    doc->capacity = new_capacity;
    return;
}

static void
document_add_line(Document *doc,
char *text,
int32 length,
bool *in_block_comment,
int32 tokenize_flags) {
    Line *line;

    document_reserve_lines(doc, 1);
    line = &doc->lines[doc->line_count];
    memset64(line, 0, SIZEOF(*line));
    line->len = length;
    line->text = malloc2(length + 1);
    memcpy64(line->text, text, length);
    line->text[length] = '\0';
    tokenize_line_with_flags(line, in_block_comment, tokenize_flags);
    doc->line_count += 1;
    return;
}

static Document *
parse_text_with_flags(char *text, int32 text_len, int32 tokenize_flags) {
    Document *doc;
    bool in_block_comment;
    int32 line_start;

    doc = malloc2(SIZEOF(*doc));
    if (!doc) {
        error("Memory allocation failed.\n");
        fatal(EXIT_FAILURE);
    }

    doc->capacity = PARSE_INITIAL_LINE_CAPACITY;
    doc->line_count = 0;
    doc->lines = malloc2(doc->capacity*SIZEOF(*doc->lines));
    if (!doc->lines) {
        error("Memory allocation failed.\n");
        fatal(EXIT_FAILURE);
    }

    in_block_comment = false;
    line_start = 0;
    for (int32 i = 0; i < text_len; i += 1) {
        if (text[i] == '\n') {
            document_add_line(doc, text + line_start, i - line_start + 1,
            &in_block_comment, tokenize_flags);
            line_start = i + 1;
        }
    }

    if (line_start < text_len) {
        document_add_line(doc, text + line_start, text_len - line_start,
        &in_block_comment, tokenize_flags);
    }

    return doc;
}

static Document *
parse_c_text(char *text, int32 text_len) {
    Document *result;

    result = parse_text_with_flags(text, text_len, TOKENIZE_DEFAULT);
    return result;
}

static Document *
parse_text(char *text, int32 text_len) {
    Document *result;

    result = parse_text_with_flags(text, text_len,
    TOKENIZE_PREPROCESSOR_LINES);
    return result;
}


#if TESTING_meta_parse

static void
test_parse_c_text_splits_lines_and_tokens(void) {
    char *text = "int x;\nfloat y;";
    Document *doc;

    doc = parse_c_text(text, strlen32(text));
    ASSERT_EQUAL(doc->line_count, 2);
    ASSERT_EQUAL(doc->lines[0].text, "int x;\n");
    ASSERT_EQUAL(doc->lines[0].len, strlen32("int x;\n"));
    ASSERT_EQUAL(doc->lines[0].token_count, 5);
    ASSERT(TOKEN_IS(&doc->lines[0].tokens[0], "int"));
    ASSERT(TOKEN_IS(&doc->lines[0].tokens[2], "x"));
    ASSERT(TOKEN_IS(&doc->lines[0].tokens[3], ";"));
    ASSERT_EQUAL(doc->lines[1].text, "float y;");
    ASSERT_EQUAL(doc->lines[1].len, strlen32("float y;"));
    free_document(doc);
    return;
}

static void
test_parse_text_marks_preprocessor_lines(void) {
    char *text = "#define VALUE 3\nint value;\n";
    Document *doc;

    doc = parse_text(text, strlen32(text));
    ASSERT_EQUAL(doc->line_count, 2);
    ASSERT_EQUAL(doc->lines[0].token_count, 1);
    ASSERT(doc->lines[0].tokens[0].kind == TOKEN_PREPROC);
    ASSERT_EQUAL(doc->lines[0].tokens[0].text, "#define VALUE 3\n");
    ASSERT(doc->lines[1].tokens[0].kind == TOKEN_IDENT);
    free_document(doc);
    return;
}

static void
test_parse_text_with_flags_skip_whitespace(void) {
    char *text = "a b\n";
    Document *doc;

    doc = parse_text_with_flags(text, strlen32(text), TOKENIZE_SKIP_WHITESPACE);
    ASSERT_EQUAL(doc->line_count, 1);
    ASSERT_EQUAL(doc->lines[0].token_count, 2);
    ASSERT(TOKEN_IS(&doc->lines[0].tokens[0], "a"));
    ASSERT(TOKEN_IS(&doc->lines[0].tokens[1], "b"));
    free_document(doc);
    return;
}

static void
test_document_add_line_grows_storage(void) {
    Document doc = {0};
    bool in_block_comment = false;
    char *line = "value\n";

    doc.capacity = 1;
    doc.lines = malloc2(doc.capacity*SIZEOF(*doc.lines));
    document_add_line(&doc, line, strlen32(line), &in_block_comment,
                      TOKENIZE_DEFAULT);
    document_add_line(&doc, line, strlen32(line), &in_block_comment,
                      TOKENIZE_DEFAULT);
    ASSERT_EQUAL(doc.line_count, 2);
    ASSERT(doc.capacity >= 2);
    ASSERT_EQUAL(doc.lines[0].text, "value\n");
    ASSERT_EQUAL(doc.lines[1].text, "value\n");
    free_line(&doc.lines[0]);
    ASSERT_NULL(doc.lines[0].text);
    ASSERT_EQUAL(doc.lines[0].len, 0);
    free_line(&doc.lines[1]);
    free2(doc.lines, doc.capacity*SIZEOF(*doc.lines));
    return;
}

int
main(void) {
    test_parse_c_text_splits_lines_and_tokens();
    test_parse_text_marks_preprocessor_lines();
    test_parse_text_with_flags_skip_whitespace();
    test_document_add_line_grows_storage();
    return 0;
}

#endif /* TESTING_meta_parse */

#endif /* META_PARSE_C */
