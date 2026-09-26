// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(META_GENERATE_C)
#define META_GENERATE_C

#if !defined(TESTING_meta_generate)
#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_meta_generate 1
#else
#define TESTING_meta_generate 0
#endif
#endif

#include "cbase.h"

String
c_string_literal(char *value, int32 value_len) {
    String out = {0};

    STR_APPEND(&out, "\"");
    for (int32 i = 0; i < value_len; i += 1) {
        uint8 c = (uint8)value[i];

        switch (c) {
        case '\a':
            STR_APPEND(&out, "\\a");
            break;
        case '\b':
            STR_APPEND(&out, "\\b");
            break;
        case '\f':
            STR_APPEND(&out, "\\f");
            break;
        case '\n':
            STR_APPEND(&out, "\\n");
            break;
        case '\r':
            STR_APPEND(&out, "\\r");
            break;
        case '\t':
            STR_APPEND(&out, "\\t");
            break;
        case '\v':
            STR_APPEND(&out, "\\v");
            break;
        case '\\':
        case '"':
            str_append_byte(&out, '\\');
            str_append_byte(&out, (char)c);
            break;
        default:
            if ((c < 0x20) || (c >= 0x7f)) {
                str_printf(&out, "\\%03o", (uint32)c);
            } else {
                str_append_byte(&out, (char)c);
            }
            break;
        }
    }
    STR_APPEND(&out, "\"");

    if (out.cap != out.len + 1) {
        out.data = realloc2(out.data,
                            out.cap, out.len + 1, SIZEOF(out.data[0]));
        out.cap = out.len + 1;
    }

    return out;
}

bool
c_identifier_is_keyword(char *identifier) {
    enum CKeyword keyword;

    keyword = c_keyword_from_text(identifier, strlen32(identifier));
    if (keyword != C_KEYWORD_COUNT) {
        return true;
    }
    return false;
}

String
c_identifier(char *value, int32 value_len) {
    String out = {0};

    for (int32 i = 0; i < value_len; i += 1) {
        char c;
        if (isalnum((uint8)value[i]) || value[i] == '_') {
            c = value[i];
        } else {
            c = '_';
        }
        STR_APPEND(&out, &c, 1);
    }

    {
        bool needs_prefix = false;

        if (!out.len) {
            needs_prefix = true;
        } else if (isdigit((uint8)out.data[0])) {
            needs_prefix = true;
        } else if (out.data[0] == '_') {
            needs_prefix = true;
        } else if (c_identifier_is_keyword(out.data)) {
            needs_prefix = true;
        }

        if (needs_prefix) {
            String pref = {0};

            STR_APPEND(&pref, "c_");
            if (out.data) {
                STR_APPEND(&pref, out.data, out.len);
                free2(out.data, out.cap);
            }
            out = pref;
        }
    }

    if (out.cap != out.len + 1) {
        out.data
            = realloc2(out.data, out.cap, out.len + 1, SIZEOF(out.data[0]));
        out.cap = out.len + 1;
    }

    return out;
}

void
emit_string_array_init(String *out, char *field, char **values,
                              int32 *value_lens, int32 count,
                              char *fallback_prefix) {
    if (count <= 0) {
        return;
    }

    str_printf(out, "    .%s = {\n", field);

    for (int32 i = 0; i < count; i += 1) {
        char fb[32];
        char *value;
        int32 value_len;
        String cs;

        if (values[i]) {
            value = values[i];
            value_len = value_lens[i];
        } else {
            int32 fb_len = SNPRINTF(fb, "%s%d", fallback_prefix, i);
            value = fb;
            value_len = fb_len;
        }

        cs = c_string_literal(value, value_len);
        str_printf(out, "        %s,\n", cs.data);
        free2(cs.data, cs.cap);
    }

    STR_APPEND(out, "    },\n");
    return;
}

void
emit_lens_init(String *out, char *field, char **values,
                      int32 *value_lens, int32 count, char *fallback_prefix) {
    if (count <= 0) {
        return;
    }

    str_printf(out, "    .%s = { ", field);
    for (int32 i = 0; i < count; i += 1) {
        char fb[32];
        int32 value_len;

        if (i > 0) {
            STR_APPEND(out, ", ");
        }

        if (values[i]) {
            value_len = value_lens[i];
        } else {
            int32 fb_len = SNPRINTF(fb, "%s%d", fallback_prefix, i);
            value_len = fb_len;
        }

        str_itoa(out, value_len);
    }
    STR_APPEND(out, " },\n");

    return;
}

void
emit_int_array_init(String *out, char *field, int32 *values,
                           int32 count) {
    if (count <= 0) {
        return;
    }

    str_printf(out, "    .%s = { ", field);
    for (int32 i = 0; i < count; i += 1) {
        if (i) {
            STR_APPEND(out, ", ");
        }
        str_itoa(out, values[i]);
    }
    STR_APPEND(out, " },\n");

    return;
}

void
emit_u64_array_init(String *out, char *field, uint64 *values, int32 count) {
    if (count <= 0) {
        return;
    }

    str_printf(out, "    .%s = { ", field);
    for (int32 i = 0; i < count; i += 1) {
        if (i) {
            STR_APPEND(out, ", ");
        }
        str_printf(out, "UINT64_C(0x%w64x)", values[i]);
    }

    STR_APPEND(out, " },\n");
}

void
c_emit_wrapped_expr(String *out, char *indent, char *prefix, char *expr,
                    char *suffix) {
    int32 prefix_len = strlen32(prefix);

    STR_APPEND(out, indent, strlen32(indent));
    STR_APPEND(out, prefix, strlen32(prefix));
    for (int32 i = 0; expr[i] != '\0'; i += 1) {
        STR_APPEND(out, expr + i, 1);
        if (expr[i] == '(' || expr[i] == ',') {
            STR_APPEND(out, "\n");
            STR_APPEND(out, indent, strlen32(indent));
            for (int32 j = 0; j < prefix_len; j += 1) {
                STR_APPEND(out, " ");
            }
        }
    }
    STR_APPEND(out, suffix, strlen32(suffix));
    STR_APPEND(out, "\n");
}

#if 0 == TESTING_meta_generate
static inline void
meta_generate_sink(void) {
    (void)c_emit_wrapped_expr;
    (void)c_identifier;
    (void)emit_int_array_init;
    (void)emit_lens_init;
    (void)emit_string_array_init;
    (void)emit_u64_array_init;
}
#endif

#if TESTING_meta_generate
#define CBASE_IMPLEMENT
#include "cbase.h"

static void
test_c_string_literal(void) {
    char control_bytes[] = {
        'a',
        '\n',
        '\0',
        '\x1f',
        '9',
        '\t',
        '\\',
        '"',
        '\x7f',
    };
    String literal;

    literal = c_string_literal("a\\b\"c", strlen32("a\\b\"c"));
    ASSERT_EQUAL(literal.data, "\"a\\\\b\\\"c\"");
    ASSERT_EQUAL_VAR(literal.len, strlen32("\"a\\\\b\\\"c\""));
    ASSERT_EQUAL_VAR(literal.cap, literal.len + 1);
    free2(literal.data, literal.cap);

    literal = c_string_literal(control_bytes, LENGTH(control_bytes));
    ASSERT_EQUAL(literal.data,
                 "\"a\\n\\000\\0379\\t\\\\\\\"\\177\"");
    ASSERT_EQUAL_VAR(literal.cap, literal.len + 1);
    free2(literal.data, literal.cap);
    return;
}

static void
test_c_identifier(void) {
    String identifier;

    identifier = c_identifier("1 bad-name", strlen32("1 bad-name"));
    ASSERT_EQUAL(identifier.data, "c_1_bad_name");
    ASSERT_EQUAL_VAR(identifier.len, strlen32("c_1_bad_name"));
    ASSERT_EQUAL_VAR(identifier.cap, identifier.len + 1);
    free2(identifier.data, identifier.cap);

    identifier = c_identifier("already_ok_2", strlen32("already_ok_2"));
    ASSERT_EQUAL(identifier.data, "already_ok_2");
    free2(identifier.data, identifier.cap);

    identifier = c_identifier("int", strlen32("int"));
    ASSERT_EQUAL(identifier.data, "c_int");
    free2(identifier.data, identifier.cap);

    identifier = c_identifier("_private", strlen32("_private"));
    ASSERT_EQUAL(identifier.data, "c__private");
    free2(identifier.data, identifier.cap);

    identifier = c_identifier("", 0);
    ASSERT_EQUAL(identifier.data, "c_");
    free2(identifier.data, identifier.cap);
    return;
}

static void
test_emit_string_and_lens_inits(void) {
    String out = {0};
    char *values[3] = {"alpha", NULL, "quo\"te"};
    int32 lens[3] = {5, 0, 6};

    emit_string_array_init(&out, "names", values, lens, 3, "v");
    ASSERT_EQUAL(out.data, "    .names = {\n"
                           "        \"alpha\",\n"
                           "        \"v1\",\n"
                           "        \"quo\\\"te\",\n"
                           "    },\n");

    str_free(&out);
    emit_lens_init(&out, "name_lens", values, lens, 3, "v");
    ASSERT_EQUAL(out.data, "    .name_lens = { 5, 2, 6 },\n");
    free2(out.data, out.cap);
    return;
}

static void
test_emit_number_inits(void) {
    String out = {0};
    int32 ints[3] = {-1, 0, 42};
    uint64 u64s[2] = {UINT64_C(0x1234), UINT64_C(0)};

    emit_int_array_init(&out, "ints", ints, 3);
    ASSERT_EQUAL(out.data, "    .ints = { -1, 0, 42 },\n");

    str_free(&out);
    emit_u64_array_init(&out, "bits", u64s, 2);
    ASSERT_EQUAL(out.data,
                 "    .bits = { UINT64_C(0x1234), UINT64_C(0x0) },\n");
    free2(out.data, out.cap);
    return;
}

static void
test_emit_wrapped_expr(void) {
    String out = {0};

    c_emit_wrapped_expr(&out, "  ", "return ", "f(a,b)", ";");
    ASSERT_EQUAL(out.data, "  return f(\n"
                           "         a,\n"
                           "         b);\n");
    free2(out.data, out.cap);
    return;
}

int
main(void) {
    test_c_string_literal();
    test_c_identifier();
    test_emit_string_and_lens_inits();
    test_emit_number_inits();
    test_emit_wrapped_expr();
    return 0;
}

#endif /* TESTING_meta_generate */

#endif /* META_GENERATE_C */
