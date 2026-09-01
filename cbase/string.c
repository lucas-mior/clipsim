// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(STRING_C)
#define STRING_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_string 1
#elif !defined(TESTING_string)
#define TESTING_string 0
#endif

#include "cbase.h"

#define SFA_LINKAGE 
#define SFA_SNPRINTF_LINKAGE extern
#define SFA_TYPE char *
#define SFA_NAME strings
#define SFA_FORMAT "%s"
#include "sfa.h"

#define SFA_LINKAGE 
#define SFA_SNPRINTF_LINKAGE extern
#define SFA_TYPE double
#define SFA_NAME doubles
#define SFA_FORMAT "%f"
#include "sfa.h"

static void
striqual_validate_ascii_utf8(char *string, int32 string_len) {
    int32 bad_offset = 0;

    if (string_len < 0) {
        error("Error: Invalid string length = %d.\n", string_len);
        fatal(EXIT_FAILURE);
    }
    if ((string == NULL) && (string_len > 0)) {
        error("Error: NULL string with length = %d.\n", string_len);
        fatal(EXIT_FAILURE);
    }
    if (!utf8_valid(string, string_len, &bad_offset)) {
        error("Error: String is invalid UTF-8 at byte %d.\n", bad_offset);
        fatal(EXIT_FAILURE);
    }
    for (int32 i = 0; i < string_len; i += 1) {
        if ((uchar)string[i] > 0x7f) {
            error("Error: String contains non-ASCII UTF-8 at byte %d.\n", i);
            fatal(EXIT_FAILURE);
        }
    }

    return;
}

static char
striqual_ascii_lower(char c) {
    if ((c >= 'A') && (c <= 'Z')) {
        c = (char)(c - 'A' + 'a');
    }

    return c;
}

bool32
striqual(char *s1, char *s2) {
    return striqual2(s1, strlen32(s1), s2, strlen32(s2));
}

bool32
striqual2(char *a, int32 a_len, char *b, int32 b_len) {
    if (DEBUGGING) {
        striqual_validate_ascii_utf8(a, a_len);
        striqual_validate_ascii_utf8(b, b_len);
    }

    if (a_len != b_len) {
        return 0;
    }
    for (int32 i = 0; i < a_len; i += 1) {
        if (striqual_ascii_lower(a[i]) != striqual_ascii_lower(b[i])) {
            return 0;
        }
    }

    return 1;
}

bool
byte_matches_any(char byte, void *memory, int64 memory_len) {
    return memchr64(memory, byte, memory_len) != NULL;
}

int
strncmp32(char *left, char *right, int64 size) {
    int result;
    if (size == 0) {
        return 0;
    }
    if (DEBUGGING) {
        if ((ullong)size >= (ullong)SIZE_MAX) {
            error("Error: Size (%lld) is bigger than SIZEMAX\n", size);
            fatal(EXIT_FAILURE);
        }
    }
    result = strncmp(left, right, (size_t)size);
    return result;
}

char *
begins_with(char *string, int32 string_len, char *prefix, int32 prefix_len) {
    if (string_len < prefix_len) {
        return NULL;
    }
    if (!memcmp64(string, prefix, prefix_len)) {
        return string + prefix_len;
    } else {
        return NULL;
    }
}

char *
ends_with(char *string, int32 string_len, char *suffix, int32 suffix_len) {
    if (string_len < suffix_len) {
        return NULL;
    }
    string += (string_len - suffix_len);
    if (!memcmp64(string, suffix, suffix_len)) {
        return string;
    } else {
        return NULL;
    }
}

char *
remove_escape_sequences(char *data, int32 *data_len) {
    int32 old_len = *data_len;
    int32 read_index = 0;
    int32 write_index = 0;

    while (read_index < old_len) {
        if (data[read_index] != '\033') {
            data[write_index++] = data[read_index++];
            continue;
        }

        read_index += 1;

        if (read_index >= old_len) {
            break;
        }

        if (data[read_index] == '[') {
            read_index += 1;

            while (read_index < old_len) {
                uchar c = (uchar)data[read_index++];

                if ((c >= 0x40) && (c <= 0x7e)) {
                    break;
                }
            }
        } else {
            read_index += 1;
        }
    }

    data[write_index] = '\0';
    *data_len = write_index;
    data = realloc2(data, old_len + 1, *data_len + 1, SIZEOF(*data));

    return data;
}

int32
random_ascii_string(char *buffer, int32 capacity, int32 min_len) {
    int32 max_len = capacity - 1;
    int32 len = min_len;
    int32 range;

    if (capacity <= 0) {
        return 0;
    }

    if (len > max_len) {
        len = max_len;
    }

    range = max_len - len + 1;
    if (range > 1) {
        len = len + (rand_int() % range);
    }

    for (int32 i = 0; i < len; i += 1) {
        int32 ascii_val = 32 + (rand_int() % 95);
        buffer[i] = (char)ascii_val;
    }
    buffer[len] = '\0';

    return len;
}

#define STR_BUILDER_INITIAL_CAPACITY 16

char *
sb_opt_cstr(StrBuilder *buffer) {
    if (buffer == NULL) {
        return "";
    }
    if (buffer->data == NULL) {
        return "";
    }

    return buffer->data;
}

void
sb_free(StrBuilder *str_builder) {
    free2(str_builder->data, str_builder->cap);
    *str_builder = (StrBuilder){0};
    return;
}

void
sb_clear(StrBuilder *str_builder) {
    str_builder->len = 0;
    if (str_builder->data) {
        str_builder->data[0] = '\0';
    }
    return;
}

int32
sb_copy(StrBuilder *dest, StrBuilder *source) {
    if (dest == NULL) {
        return -EINVAL;
    }
    if (dest == source) {
        return dest->len;
    }
    if (source == NULL) {
        sb_free(dest);
        return dest->len;
    }

    sb_clear(dest);
    sb_append(dest, source->data, source->len);
    return dest->len;
}

void
sb_move(StrBuilder *dest, StrBuilder *source) {
    if (dest == NULL) {
        return;
    }
    if (dest == source) {
        return;
    }

    sb_free(dest);
    if (source == NULL) {
        *dest = (StrBuilder){0};
        return;
    }

    *dest = *source;
    *source = (StrBuilder){0};
    return;
}

int32
sb_set(StrBuilder *str_builder, char *data, int32 data_len) {
    if (str_builder == NULL) {
        return -EINVAL;
    }
    if (data_len < 0) {
        return -EINVAL;
    }
    if ((data == NULL) && (data_len > 0)) {
        return -EINVAL;
    }
    if ((data == str_builder->data) && str_builder->data) {
        if (data_len > str_builder->len) {
            return -EINVAL;
        }
        str_builder->len = data_len;
        str_builder->data[data_len] = '\0';
        return str_builder->len;
    }

    sb_clear(str_builder);
    sb_append(str_builder, data, data_len);
    return str_builder->len;
}

void
sb_reserve(StrBuilder *str_builder, int64 extra) {
    int64 needed;
    int64 new_cap;
    int32 old_cap;

    if (extra <= 0) {
        return;
    }

    needed = str_builder->len + extra + 1;
    if (str_builder->data && (needed <= str_builder->cap)) {
        return;
    }
    if (needed >= MAXOF(str_builder->cap)) {
        error("StrBuilder only supports strings shorter than 2GB.\n");
        fatal(EXIT_FAILURE);
    }

    old_cap = str_builder->cap;
    if (str_builder->data == NULL) {
        old_cap = 0;
    }

    new_cap = str_builder->cap;
    if (new_cap <= 0) {
        new_cap = STR_BUILDER_INITIAL_CAPACITY;
    }
    while (new_cap < needed) {
        new_cap *= 2;
    }
    if (new_cap >= MAXOF(str_builder->cap)) {
        new_cap = needed;
    }

    str_builder->data = realloc2(str_builder->data, old_cap, new_cap,
                                 SIZEOF(*str_builder->data));
    str_builder->cap = (int32)new_cap;
    return;
}

void
sb_append(StrBuilder *str_builder, char *data, int64 data_len) {
    bool aliases = false;
    int32 data_offset = 0;

    if ((data_len <= 0) || (data == NULL)) {
        return;
    }

    if (data == str_builder->data) {
        aliases = true;
    } else if (str_builder->data) {
        uintptr data_address = (uintptr)data;
        uintptr start = (uintptr)str_builder->data;

        if (data_address >= start) {
            uintptr offset = data_address - start;

            if (offset < (uint32)str_builder->cap) {
                aliases = true;
                data_offset = (int32)offset;
            }
        }
    }

    sb_reserve(str_builder, data_len);
    if (aliases) {
        data = str_builder->data + data_offset;
        memmove64(str_builder->data + str_builder->len, data, data_len);
    } else {
        memcpy64(str_builder->data + str_builder->len, data, data_len);
    }
    str_builder->len += (int32)data_len;
    str_builder->data[str_builder->len] = '\0';

    return;
}

void
sb_append_byte(StrBuilder *str_builder, char byte) {
    if (byte == '\0') {
        return;
    }
    sb_reserve(str_builder, 1);
    str_builder->data[str_builder->len] = byte;
    str_builder->len += 1;
    str_builder->data[str_builder->len] = '\0';
    return;
}

void
sb_append_byte_if_not(StrBuilder *str_builder, char byte) {
    if ((str_builder->len > 0)
        && (str_builder->data[str_builder->len - 1] == byte)) {
        return;
    }
    sb_append_byte(str_builder, byte);
    return;
}

void
sb_itoa(StrBuilder *str_builder, llong num) {
    int32 len;

    sb_reserve(str_builder, 21);
    len = itoa2(str_builder->data + str_builder->len,
                str_builder->cap - str_builder->len, num);
    str_builder->len += len;
    return;
}

void
sb_printf(StrBuilder *str_builder, char *fmt, ...) {
    va_list ap;
    va_list ap2;
    int32 n;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0) {
        va_end(ap2);
        error("Error formatting \"%s\".", fmt);
        fatal(EXIT_FAILURE);
    }
    if (n == 0) {
        va_end(ap2);
        return;
    }

    sb_reserve(str_builder, n);
    vsnprintf(str_builder->data + str_builder->len, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    str_builder->len += n;
    return;
}

char *
sb_steal(StrBuilder *str_builder, int32 *len, int32 *cap) {
    char *data = str_builder->data;

    if (len) {
        *len = str_builder->len;
    }
    if (cap) {
        *cap = str_builder->cap;
    }

    *str_builder = (StrBuilder){0};
    return data;
}

char *
sb_steal_exact(StrBuilder *str_builder, int32 *len) {
    char *data;
    int32 data_len;
    int32 cap;

    data = sb_steal(str_builder, &data_len, &cap);
    if (cap != data_len + 1) {
        data = realloc2(data, cap, data_len + 1, SIZEOF(*data));
    }
    data[data_len] = '\0';

    if (len) {
        *len = data_len;
    }
    return data;
}

void
str_builder_array_init(StrBuilderArray *array) {
    array->items = NULL;
    array->len = 0;
    array->cap = 0;
    return;
}

void
str_builder_array_clear(StrBuilderArray *array) {
    if (array == NULL) {
        return;
    }

    for (int32 i = 0; i < array->len; i += 1) {
        sb_free(&array->items[i]);
    }
    array->len = 0;
    return;
}

void
str_builder_array_destroy(StrBuilderArray *array) {
    if (array == NULL) {
        return;
    }

    str_builder_array_clear(array);
    free2(array->items, array->cap*SIZEOF(*array->items));
    str_builder_array_init(array);
    return;
}

int32
str_builder_array_copy(StrBuilderArray *dest, StrBuilderArray *source) {
    StrBuilderArray replacement;
    int32 err;

    if (dest == NULL) {
        return -EINVAL;
    }
    if (dest == source) {
        return dest->len;
    }

    str_builder_array_init(&replacement);
    if (source) {
        if ((err = str_builder_array_reserve(&replacement, source->len)) < 0) {
            str_builder_array_destroy(&replacement);
            return err;
        }
        for (int32 i = 0; i < source->len; i += 1) {
            if ((err = str_builder_array_append_copy(
                     &replacement, &source->items[i])) < 0) {
                str_builder_array_destroy(&replacement);
                return err;
            }
        }
    }

    str_builder_array_destroy(dest);
    *dest = replacement;
    return dest->len;
}

void
str_builder_array_move(StrBuilderArray *dest, StrBuilderArray *source) {
    if (dest == NULL) {
        return;
    }
    if (dest == source) {
        return;
    }

    str_builder_array_destroy(dest);
    if (source == NULL) {
        str_builder_array_init(dest);
        return;
    }
    *dest = *source;
    str_builder_array_init(source);
    return;
}

void
str_builder_array_swap(StrBuilderArray *left, StrBuilderArray *right) {
    StrBuilderArray temp;

    if (left == NULL) {
        return;
    }
    if (right == NULL) {
        return;
    }

    temp = *left;
    *left = *right;
    *right = temp;
    return;
}

int32
str_builder_array_reserve(StrBuilderArray *array, int32 extra) {
    int64 needed;
    int32 old_cap;
    int32 new_cap;

    if (array == NULL) {
        return -EINVAL;
    }
    if (extra < 0) {
        return -EINVAL;
    }
    if (extra == 0) {
        return array->cap;
    }

    needed = (int64)array->len + extra;
    if (needed <= array->cap) {
        return array->cap;
    }
    if (needed >= MAXOF(array->cap)) {
        error("StrBuilderArray only supports fewer than 2GB items.\n");
        fatal(EXIT_FAILURE);
    }

    old_cap = array->cap;
    new_cap = array->cap;
    if (new_cap <= 0) {
        new_cap = 8;
    }

    if (needed >= (MAXOF(new_cap)/2)) {
        new_cap = (int32)needed;
    } else {
        while (new_cap < needed) {
            new_cap *= 2;
        }
    }

    array->items = realloc2(array->items,
                            old_cap, new_cap, SIZEOF(*array->items));
    array->cap = new_cap;
    return array->cap;
}

StrBuilder *
str_builder_array_append(StrBuilderArray *array) {
    StrBuilder *item;

    if (str_builder_array_reserve(array, 1) < 0) {
        return NULL;
    }

    item = &array->items[array->len];
    array->len += 1;
    *item = (StrBuilder){0};
    return item;
}

int32
str_builder_array_append_copy(StrBuilderArray *array, StrBuilder *item) {
    StrBuilder *dest;
    int32 err;
    int32 index;

    if ((array == NULL) || (item == NULL)) {
        return -EINVAL;
    }

    if ((err = str_builder_array_reserve(array, 1)) < 0) {
        return err;
    }

    index = array->len;
    dest = &array->items[index];
    array->len += 1;
    *dest = (StrBuilder){0};
    if ((err = sb_copy(dest, item)) < 0) {
        array->len -= 1;
        sb_free(dest);
        return err;
    }
    return index;
}

#if 0 == TESTING_string
static inline void
string_functions_sink(void) {
    (void)string_functions_sink;
    (void)optional_strequal;
    (void)strequal;
    (void)striqual;
    (void)striqual2;
    (void)strncmp32;
    (void)begins_with;
    (void)ends_with;
    (void)byte_matches_any;
    (void)remove_escape_sequences;
    (void)random_ascii_string;
    (void)string_from_doubles;
    (void)string_from_strings;
    (void)sb_append_byte_if_not;
    (void)sb_itoa;
    (void)sb_move;
    (void)sb_opt_cstr;
    (void)sb_printf;
    (void)str_builder_array_copy;
    (void)str_builder_array_move;
    (void)str_builder_array_swap;
    return;
}
#endif

#if TESTING_string
#define CBASE_IMPLEMENT
#include "cbase.h"

int
main(void) {
    char *s1 = "aaaabbbb";

    ASSERT(BEGINS_WITH(s1, strlen32(s1), "aaaa"));
    ASSERT(BEGINS_WITH(s1, strlen32(s1), "aaaabbbb"));
    ASSERT(!BEGINS_WITH(s1, strlen32(s1), "bbbb"));
    ASSERT(!BEGINS_WITH(s1, strlen32(s1), "aaaabbbbb"));

    ASSERT(ENDS_WITH(s1, strlen32(s1), "bbbb"));
    ASSERT(ENDS_WITH(s1, strlen32(s1), "aaaabbbb"));
    ASSERT(!ENDS_WITH(s1, strlen32(s1), "aaaa"));
    ASSERT(!ENDS_WITH(s1, strlen32(s1), "aaaaabbbbb"));

    ASSERT(BYTE_MATCHES_ANY('a', "abc"));
    ASSERT(!BYTE_MATCHES_ANY('d', "abc", 3));

    ASSERT(striqual("abc", "ABC"));
    ASSERT(striqual("ASCII 123 _-", "ascii 123 _-"));
    ASSERT(!striqual("abc", "abd"));
    ASSERT(!striqual("abc", "abcd"));

    ASSERT(STRIQUAL(s1, strlen32(s1), "AAAABBBB"));
    ASSERT(STRIQUAL(s1 + 4, 4, "BBBB"));
    ASSERT(STRIQUAL("MiXeD", 5, "mixed", 5));
    ASSERT(!STRIQUAL("MiXeD", 4, "mixed", 5));
    ASSERT(!STRIQUAL("MiXeD", 5, "match", 5));

    {
        StrBuilder builder = {0};
        int32 old_cap;

        SB_APPEND(&builder, "0123456789abcde");
        old_cap = builder.cap;
        sb_append(&builder, builder.data + 1, builder.len - 1);
        ASSERT_MORE(builder.cap, old_cap);
        ASSERT_EQUAL(builder.data,
                     "0123456789abcde123456789abcde");
        sb_free(&builder);
    }

    {
        StrBuilder builder = {0};

        SB_APPEND(&builder, "x");
        sb_itoa(&builder, 0);
        SB_APPEND(&builder, " ");
        sb_itoa(&builder, -9223372036854775807LL - 1);
        SB_APPEND(&builder, " ");
        sb_itoa(&builder, 9223372036854775807LL);
        ASSERT_EQUAL(builder.data,
                     "x0 -9223372036854775808 9223372036854775807");
        sb_free(&builder);
    }

    {
        char b[64];
        char *strs[] = {"one", "two", "three"};
        double dbls[] = {1.1, 2.2};
        string_from_strings(b, sizeof(b), "|", strs, 3);
        ASSERT_EQUAL(b, "one|two|three");
        string_from_doubles(b, sizeof(b), ",", dbls, 2);
        ASSERT_POSITIVE(strlen32(b));
    }

    exit(EXIT_SUCCESS);
}

#endif

#endif /* STRING_C */
