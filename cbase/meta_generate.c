// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(META_GENERATE_C)
#define META_GENERATE_C

#include <ctype.h>
#include <inttypes.h>

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_meta_generate 1
#elif !defined(TESTING_meta_generate)
#define TESTING_meta_generate 0
#endif

#include "cbase.h"

static StrBuilder
c_string_literal(char *value, int32 value_len) {
    StrBuilder out = {0};

    SB_APPEND(&out, "\"");
    for (int32 i = 0; i < value_len; i += 1) {
        uint8 c = (uint8)value[i];

        switch (c) {
        case '\a':
            SB_APPEND(&out, "\\a");
            break;
        case '\b':
            SB_APPEND(&out, "\\b");
            break;
        case '\f':
            SB_APPEND(&out, "\\f");
            break;
        case '\n':
            SB_APPEND(&out, "\\n");
            break;
        case '\r':
            SB_APPEND(&out, "\\r");
            break;
        case '\t':
            SB_APPEND(&out, "\\t");
            break;
        case '\v':
            SB_APPEND(&out, "\\v");
            break;
        case '\\':
        case '"':
            sb_append_byte(&out, '\\');
            sb_append_byte(&out, (char)c);
            break;
        default:
            if ((c < 0x20) || (c >= 0x7f)) {
                sb_printf(&out, "\\%03o", (uint32)c);
            } else {
                sb_append_byte(&out, (char)c);
            }
            break;
        }
    }
    SB_APPEND(&out, "\"");

    if (out.cap != out.len + 1) {
        out.data = realloc2(out.data,
                            out.cap, out.len + 1, SIZEOF(out.data[0]));
        out.cap = out.len + 1;
    }

    return out;
}

static bool
c_identifier_is_keyword(char *identifier) {
    static char *keywords[] = {
        "_Alignas",
        "_Alignof",
        "_Atomic",
        "_Bool",
        "_Complex",
        "_Generic",
        "_Imaginary",
        "_Noreturn",
        "_Static_assert",
        "_Thread_local",
        "auto",
        "break",
        "case",
        "char",
        "const",
        "continue",
        "default",
        "do",
        "double",
        "else",
        "enum",
        "extern",
        "float",
        "for",
        "goto",
        "if",
        "inline",
        "int",
        "long",
        "register",
        "restrict",
        "return",
        "short",
        "signed",
        "sizeof",
        "static",
        "struct",
        "switch",
        "typedef",
        "union",
        "unsigned",
        "void",
        "volatile",
        "while",
    };

    for (int32 i = 0; i < LENGTH(keywords); i += 1) {
        if (strequal(identifier, keywords[i])) {
            return true;
        }
    }
    return false;
}

static StrBuilder
c_identifier(char *value, int32 value_len) {
    StrBuilder out = {0};

    for (int32 i = 0; i < value_len; i += 1) {
        char c;
        if (isalnum((uint8)value[i]) || value[i] == '_') {
            c = value[i];
        } else {
            c = '_';
        }
        SB_APPEND(&out, &c, 1);
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
            StrBuilder pref = {0};

            SB_APPEND(&pref, "c_");
            if (out.data) {
                SB_APPEND(&pref, out.data, out.len);
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

static void
emit_string_array_initializer(StrBuilder *out, char *field, char **values,
                              int32 *value_lens, int32 count,
                              char *fallback_prefix) {
    if (count <= 0) {
        return;
    }

    sb_printf(out, "    .%s = {\n", field);

    for (int32 i = 0; i < count; i += 1) {
        char fb[32];
        char *value;
        int32 value_len;
        StrBuilder cs;

        if (values[i]) {
            value = values[i];
            value_len = value_lens[i];
        } else {
            int32 fb_len = SNPRINTF(fb, "%s%d", fallback_prefix, i);
            value = fb;
            value_len = fb_len;
        }

        cs = c_string_literal(value, value_len);
        sb_printf(out, "        %s,\n", cs.data);
        free2(cs.data, cs.cap);
    }

    SB_APPEND(out, "    },\n");
    return;
}

static void
emit_lens_initializer(StrBuilder *out, char *field, char **values,
                      int32 *value_lens, int32 count, char *fallback_prefix) {
    if (count <= 0) {
        return;
    }

    sb_printf(out, "    .%s = { ", field);
    for (int32 i = 0; i < count; i += 1) {
        char fb[32];
        int32 value_len;

        if (i > 0) {
            SB_APPEND(out, ", ");
        }

        if (values[i]) {
            value_len = value_lens[i];
        } else {
            int32 fb_len = SNPRINTF(fb, "%s%d", fallback_prefix, i);
            value_len = fb_len;
        }

        sb_printf(out, "%d", value_len);
    }
    SB_APPEND(out, " },\n");

    return;
}

static void
emit_int_array_initializer(StrBuilder *out, char *field, int32 *values,
                           int32 count) {
    if (count <= 0) {
        return;
    }

    sb_printf(out, "    .%s = { ", field);
    for (int32 i = 0; i < count; i += 1) {
        if (i) {
            SB_APPEND(out, ", ");
        }
        sb_printf(out, "%d", values[i]);
    }
    SB_APPEND(out, " },\n");

    return;
}

static void
emit_u64_array_initializer(StrBuilder *out, char *field, uint64 *values,
                           int32 count) {
    if (count <= 0) {
        return;
    }

    sb_printf(out, "    .%s = { ", field);
    for (int32 i = 0; i < count; i += 1) {
        if (i) {
            SB_APPEND(out, ", ");
        }
        sb_printf(out, "UINT64_C(0x%" PRIx64 ")", values[i]);
    }

    SB_APPEND(out, " },\n");
}

static void
c_emit_wrapped_expr(StrBuilder *out, char *indent, char *prefix, char *expr,
                    char *suffix) {
    int32 prefix_len = strlen32(prefix);

    SB_APPEND(out, indent);
    SB_APPEND(out, prefix);
    for (int32 i = 0; expr[i] != '\0'; i += 1) {
        SB_APPEND(out, expr + i, 1);
        if (expr[i] == '(' || expr[i] == ',') {
            SB_APPEND(out, "\n");
            SB_APPEND(out, indent);
            for (int32 j = 0; j < prefix_len; j += 1) {
                SB_APPEND(out, " ");
            }
        }
    }
    SB_APPEND(out, suffix);
    SB_APPEND(out, "\n");
}

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
    StrBuilder literal;

    literal = c_string_literal("a\\b\"c", strlen32("a\\b\"c"));
    ASSERT_EQUAL(literal.data, "\"a\\\\b\\\"c\"");
    ASSERT_EQUAL(literal.len, strlen32("\"a\\\\b\\\"c\""));
    ASSERT_EQUAL(literal.cap, literal.len + 1);
    free2(literal.data, literal.cap);

    literal = c_string_literal(control_bytes, LENGTH(control_bytes));
    ASSERT_EQUAL(literal.data,
                 "\"a\\n\\000\\0379\\t\\\\\\\"\\177\"");
    ASSERT_EQUAL(literal.cap, literal.len + 1);
    free2(literal.data, literal.cap);
    return;
}

static void
test_c_identifier(void) {
    StrBuilder identifier;

    identifier = c_identifier("1 bad-name", strlen32("1 bad-name"));
    ASSERT_EQUAL(identifier.data, "c_1_bad_name");
    ASSERT_EQUAL(identifier.len, strlen32("c_1_bad_name"));
    ASSERT_EQUAL(identifier.cap, identifier.len + 1);
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
test_emit_string_and_lens_initializers(void) {
    StrBuilder out = {0};
    char *values[3] = {"alpha", NULL, "quo\"te"};
    int32 lens[3] = {5, 0, 6};

    emit_string_array_initializer(&out, "names", values, lens, 3, "v");
    ASSERT_EQUAL(out.data, "    .names = {\n"
                           "        \"alpha\",\n"
                           "        \"v1\",\n"
                           "        \"quo\\\"te\",\n"
                           "    },\n");

    sb_free(&out);
    emit_lens_initializer(&out, "name_lens", values, lens, 3, "v");
    ASSERT_EQUAL(out.data, "    .name_lens = { 5, 2, 6 },\n");
    free2(out.data, out.cap);
    return;
}

static void
test_emit_number_initializers(void) {
    StrBuilder out = {0};
    int32 ints[3] = {-1, 0, 42};
    uint64 u64s[2] = {UINT64_C(0x1234), UINT64_C(0)};

    emit_int_array_initializer(&out, "ints", ints, 3);
    ASSERT_EQUAL(out.data, "    .ints = { -1, 0, 42 },\n");

    sb_free(&out);
    emit_u64_array_initializer(&out, "bits", u64s, 2);
    ASSERT_EQUAL(out.data,
                 "    .bits = { UINT64_C(0x1234), UINT64_C(0x0) },\n");
    free2(out.data, out.cap);
    return;
}

static void
test_emit_wrapped_expr(void) {
    StrBuilder out = {0};

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
    test_emit_string_and_lens_initializers();
    test_emit_number_initializers();
    test_emit_wrapped_expr();
    return 0;
}

#endif /* TESTING_meta_generate */

#endif /* META_GENERATE_C */
