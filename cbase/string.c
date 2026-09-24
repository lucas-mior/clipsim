// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(STRING_C)
#define STRING_C

#if !defined(TESTING_string)
#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_string 1
#else
#define TESTING_string 0
#endif
#endif

#include "cbase.h"

#define SFA_LINKAGE 
#define SFA_TYPE char *
#define SFA_NAME strings
#define SFA_FORMAT "%s"
#include "sfa.h"

#define SFA_LINKAGE 
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

void
strflex_list_push(StrFlexList *list, char *value, int32 value_len) {
    StrFlex *string;

    if (list->arena == NULL) {
        list->arena = arena_create(SIZEMB(2), "strflex_list");
    }

    string = xarena_push(list->arena, SIZEOF(*string) + value_len + 1);
    string->len = value_len;
    memcpy64(string->data, value, value_len);
    string->data[value_len] = '\0';
    ARRAY_PUSH(list->items, string);
    return;
}

void
strflex_list_destroy(StrFlexList *list) {
    if (list == NULL) {
        return;
    }

    ARRAY_FREE(list->items);
    if (list->arena) {
        arena_destroy(list->arena);
    }
    *list = (StrFlexList){0};

    return;
}

void
strflex_list_clear(StrFlexList *list) {
    if (list == NULL) {
        return;
    }

    ARRAY_CLEAR(list->items);
    arena_reset(list->arena);
    return;
}

int32
strflex_list_len(StrFlexList *list) {
    if (list == NULL) {
        return 0;
    }

    return ARRAY_LEN(list->items);
}

StrFlex *
strflex_list_at(StrFlexList *list, int32 idx) {
    if (list == NULL) {
        return NULL;
    }
    if ((idx < 0) || (idx >= ARRAY_LEN(list->items))) {
        return NULL;
    }

    return list->items[idx];
}

#define STRING_INITIAL_CAPACITY 16

char *
str_opt_cstr(String *buffer) {
    if (buffer == NULL) {
        return "";
    }
    if (buffer->data == NULL) {
        return "";
    }

    return buffer->data;
}

void
str_free(String *str) {
    free2(str->data, str->cap);
    *str = (String){0};
    return;
}

void
str_clear(String *str) {
    str->len = 0;
    if (str->data) {
        str->data[0] = '\0';
    }
    return;
}

int32
str_copy(String *dest, String *source) {
    if (dest == NULL) {
        return -EINVAL;
    }
    if (dest == source) {
        return dest->len;
    }
    if (source == NULL) {
        str_free(dest);
        return dest->len;
    }

    str_clear(dest);
    str_append(dest, source->data, source->len);
    return dest->len;
}

void
str_move(String *dest, String *source) {
    if (dest == NULL) {
        return;
    }
    if (dest == source) {
        return;
    }

    str_free(dest);
    if (source == NULL) {
        *dest = (String){0};
        return;
    }

    *dest = *source;
    *source = (String){0};
    return;
}

int32
str_set(String *str, char *data, int32 data_len) {
    if (str == NULL) {
        return -EINVAL;
    }
    if (data_len < 0) {
        return -EINVAL;
    }
    if ((data == NULL) && (data_len > 0)) {
        return -EINVAL;
    }
    if ((data == str->data) && str->data) {
        if (data_len > str->len) {
            return -EINVAL;
        }
        str->len = data_len;
        str->data[data_len] = '\0';
        return str->len;
    }

    str_clear(str);
    str_append(str, data, data_len);
    return str->len;
}

void
str_reserve(String *str, int64 extra) {
    int64 needed;
    int64 new_cap;
    int32 old_cap;

    if (extra <= 0) {
        return;
    }

    if (UNLIKELY(extra >= MAXOF(str->cap))) {
        error("String only supports strings shorter than 2GB.\n");
        fatal(EXIT_FAILURE);
    }

    needed = str->len + extra + 1;
    if (str->data && (needed <= str->cap)) {
        return;
    }
    if (UNLIKELY(needed >= MAXOF(str->cap))) {
        error("String only supports strings shorter than 2GB.\n");
        fatal(EXIT_FAILURE);
    }

    old_cap = str->cap;
    if (str->data == NULL) {
        old_cap = 0;
    }

    new_cap = str->cap;
    if (new_cap <= 0) {
        new_cap = STRING_INITIAL_CAPACITY;
    }
    while (new_cap < needed) {
        new_cap *= 2;
    }
    if (new_cap >= MAXOF(str->cap)) {
        new_cap = needed;
    }

    str->data = realloc2(str->data, old_cap, new_cap, SIZEOF(*str->data));
    str->cap = (int32)new_cap;
    return;
}

void
str_append(String *str, char *data, int64 data_len) {
    bool aliases = false;
    int32 data_offset = 0;

    if ((data_len <= 0) || (data == NULL)) {
        return;
    }

    if (UNLIKELY(data == str->data)) {
        aliases = true;
    } else if (LIKELY(str->data != NULL)) {
        uintptr data_address = (uintptr)data;
        uintptr start = (uintptr)str->data;

        if (data_address >= start) {
            uintptr offset = data_address - start;

            if (UNLIKELY(offset < (uint32)str->cap)) {
                aliases = true;
                data_offset = (int32)offset;
            }
        }
    }

    str_reserve(str, data_len);
    if (UNLIKELY(aliases)) {
        data = str->data + data_offset;
        memmove64(str->data + str->len, data, data_len);
    } else {
        memcpy64(str->data + str->len, data, data_len);
    }
    str->len += (int32)data_len;
    str->data[str->len] = '\0';

    return;
}

void
str_append_byte(String *str, char byte) {
    if (byte == '\0') {
        return;
    }
    str_reserve(str, 1);
    str->data[str->len] = byte;
    str->len += 1;
    str->data[str->len] = '\0';
    return;
}

void
str_append_byte_if_not(String *str, char byte) {
    if ((str->len > 0)
        && (str->data[str->len - 1] == byte)) {
        return;
    }
    str_append_byte(str, byte);
    return;
}

void
str_itoa(String *str, llong num) {
    int32 len;

    str_reserve(str, 21);
    len = itoa2(str->data + str->len, str->cap - str->len, num);
    str->len += len;

    return;
}

void
str_bytes_pretty(String *str, llong size) {
    int32 len;

    str_reserve(str, 16);
    len = bytes_pretty(str->data + str->len, size);
    str->len += len;

    return;
}

void
str_printf(String *str, char *fmt, ...) {
    va_list ap;
    va_list ap2;
    int32 estimate;
    int32 len;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    estimate = fmt_vsnprintf_estimate(fmt, ap);
    va_end(ap);

    if (estimate < 0) {
        va_end(ap2);
        error("Error formatting \"%s\".", fmt);
        fatal(EXIT_FAILURE);
    }

    str_reserve(str, estimate);

    len = fmt_vsnprintf(str->data + str->len, estimate + 1, fmt, ap2);
    va_end(ap2);

    if (len < 0) {
        error("Error formatting \"%s\".", fmt);
        fatal(EXIT_FAILURE);
    }
    if (len > estimate) {
        error("Error: Format estimate was too small for \"%s\".", fmt);
        fatal(EXIT_FAILURE);
    }

    str->len += len;
    return;
}

char *
str_steal(String *str, int32 *len, int32 *cap) {
    char *data = str->data;

    if (len) {
        *len = str->len;
    }
    if (cap) {
        *cap = str->cap;
    }

    *str = (String){0};
    return data;
}

char *
str_steal_exact(String *str, int32 *len) {
    char *data;
    int32 data_len;
    int32 cap;

    data = str_steal(str, &data_len, &cap);
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
string_array_clear(StringArray *array) {
    if (array == NULL) {
        return;
    }

    for (int32 i = 0; i < array->len; i += 1) {
        str_free(&array->items[i]);
    }
    array->len = 0;
    return;
}

void
string_array_destroy(StringArray *array) {
    if (array == NULL) {
        return;
    }

    string_array_clear(array);
    free2(array->items, array->cap*SIZEOF(*array->items));
    *array = (StringArray){0};
    return;
}

int32
string_array_copy(StringArray *dest, StringArray *source) {
    StringArray replacement = {0};
    int32 err;

    if (dest == NULL) {
        return -EINVAL;
    }
    if (dest == source) {
        return dest->len;
    }

    if (source) {
        if ((err = string_array_reserve(&replacement, source->len)) < 0) {
            string_array_destroy(&replacement);
            return err;
        }
        for (int32 i = 0; i < source->len; i += 1) {
            if ((err = string_array_append_copy(
                     &replacement, &source->items[i])) < 0) {
                string_array_destroy(&replacement);
                return err;
            }
        }
    }

    string_array_destroy(dest);
    *dest = replacement;
    return dest->len;
}

void
string_array_move(StringArray *dest, StringArray *source) {
    if (dest == NULL) {
        return;
    }
    if (dest == source) {
        return;
    }

    string_array_destroy(dest);
    if (source == NULL) {
        *dest = (StringArray){0};
        return;
    }
    *dest = *source;
    *source = (StringArray){0};
    return;
}

void
string_array_swap(StringArray *left, StringArray *right) {
    StringArray temp;

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
string_array_reserve(StringArray *array, int32 extra) {
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
        error("StringArray only supports fewer than 2GB items.\n");
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

String *
string_array_append(StringArray *array) {
    String *item;

    if (string_array_reserve(array, 1) < 0) {
        return NULL;
    }

    item = &array->items[array->len];
    array->len += 1;
    *item = (String){0};
    return item;
}

int32
string_array_append_copy(StringArray *array, String *item) {
    String *dest;
    int32 err;
    int32 index;

    if ((array == NULL) || (item == NULL)) {
        return -EINVAL;
    }

    if ((err = string_array_reserve(array, 1)) < 0) {
        return err;
    }

    index = array->len;
    dest = &array->items[index];
    array->len += 1;
    *dest = (String){0};
    if ((err = str_copy(dest, item)) < 0) {
        array->len -= 1;
        str_free(dest);
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
    (void)str_append_byte_if_not;
    (void)str_itoa;
    (void)str_move;
    (void)str_opt_cstr;
    (void)str_printf;
    (void)string_array_copy;
    (void)string_array_move;
    (void)string_array_swap;
    (void)strflex_list_at;
    (void)strflex_list_clear;
    (void)strflex_list_destroy;
    (void)strflex_list_len;
    (void)strflex_list_push;
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
        String builder = {0};
        int32 old_cap;

        STR_APPEND(&builder, "0123456789abcde");
        old_cap = builder.cap;
        str_append(&builder, builder.data + 1, builder.len - 1);
        ASSERT_MORE(builder.cap, old_cap);
        ASSERT_EQUAL(builder.data,
                     "0123456789abcde123456789abcde");
        str_free(&builder);
    }

    {
        String builder = {0};

        STR_APPEND(&builder, "x");
        str_itoa(&builder, 0);
        STR_APPEND(&builder, " ");
        str_itoa(&builder, -9223372036854775807LL - 1);
        STR_APPEND(&builder, " ");
        str_itoa(&builder, 9223372036854775807LL);
        ASSERT_EQUAL(builder.data,
                     "x0 -9223372036854775808 9223372036854775807");
        str_free(&builder);
    }
    {
        String builder = {0};
        int32 count = 0;

        str_printf(&builder, "%s %.10s %d%n", "x", "abc", 7, &count);
        ASSERT_EQUAL(builder.data, "x abc 7");
        ASSERT_EQUAL(builder.len, 7);
        ASSERT_EQUAL(count, builder.len);
        str_free(&builder);
    }

    {
        String builder = {0};
        STR_APPEND(&builder, "x");
        str_bytes_pretty(&builder, UINT32_MAX);
        ASSERT_EQUAL(builder.data, "x4.0000GB");
        str_free(&builder);
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
