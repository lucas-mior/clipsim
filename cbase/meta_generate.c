#if !defined(META_GENERATE_C)
#define META_GENERATE_C

#include <ctype.h>
#include <inttypes.h>

#include "meta.h"
#include "util.c"

static StrBuilder
c_string_literal(char *value, int32 value_len) {
    StrBuilder out = {0};

    SB_APPEND(&out, "\"");
    for (int32 i = 0; i < value_len; i += 1) {
        if ((value[i] == '\\') || (value[i] == '"')) {
            SB_APPEND(&out, "\\");
            SB_APPEND(&out, value + i, 1);
        } else {
            SB_APPEND(&out, value + i, 1);
        }
    }
    SB_APPEND(&out, "\"");

    if (out.cap != out.len + 1) {
        out.data
        = realloc2(out.data, out.cap, out.len + 1, SIZEOF(out.data[0]));
        out.cap = out.len + 1;
    }

    return out;
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

    if (!out.len || isdigit((uint8)out.data[0])) {
        StrBuilder pref = {0};
        SB_APPEND(&pref, "_");
        if (out.data) {
            SB_APPEND(&pref, out.data, out.len);
            free2(out.data, out.cap);
        }
        out = pref;
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
        int32 fb_len = SNPRINTF(fb, "%s%d", fallback_prefix, i);
        char *value;
        int32 value_len;
        if (values[i]) {
            value = values[i];
            value_len = value_lens[i];
        } else {
            value = fb;
            value_len = fb_len;
        }
        StrBuilder cs = c_string_literal(value, value_len);
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
        if (i) {
            SB_APPEND(out, ", ");
        }
        char fb[32];
        int32 fb_len = SNPRINTF(fb, "%s%d", fallback_prefix, i);
        int32 value_len;
        if (values[i]) {
            value_len = value_lens[i];
        } else {
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

static void
test_c_string_literal(void) {
    StrBuilder literal;

    literal = c_string_literal("a\\b\"c", strlen32("a\\b\"c"));
    ASSERT_EQUAL(literal.data, "\"a\\\\b\\\"c\"");
    ASSERT_EQUAL(literal.len, strlen32("\"a\\\\b\\\"c\""));
    ASSERT_EQUAL(literal.cap, literal.len + 1);
    free2(literal.data, literal.cap);
    return;
}

static void
test_c_identifier(void) {
    StrBuilder identifier;

    identifier = c_identifier("1 bad-name", strlen32("1 bad-name"));
    ASSERT_EQUAL(identifier.data, "_1_bad_name");
    ASSERT_EQUAL(identifier.len, strlen32("_1_bad_name"));
    ASSERT_EQUAL(identifier.cap, identifier.len + 1);
    free2(identifier.data, identifier.cap);

    identifier = c_identifier("already_ok_2", strlen32("already_ok_2"));
    ASSERT_EQUAL(identifier.data, "already_ok_2");
    free2(identifier.data, identifier.cap);
    return;
}

static void
test_emit_string_and_lens_initializers(void) {
    StrBuilder out = {0};
    char *values[3] = {"alpha", NULL, "quo\"te"};
    int32 lens[3] = {5, 0, 6};

    emit_string_array_initializer(&out, "names", values, lens, 3, "v");
    ASSERT_EQUAL(out.data,
                 "    .names = {\n"
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
    ASSERT_EQUAL(out.data,
                 "  return f(\n"
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
