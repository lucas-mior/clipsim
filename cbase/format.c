// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(FORMAT_C)
#define FORMAT_C

#if !defined(TESTING_format)
#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_format 1
#else
#define TESTING_format 0
#endif
#endif

#include "cbase.h"

#if !defined(EOVERFLOW)
#define EOVERFLOW ERANGE
#endif

enum {
    FORMAT_FLOAT_RYU_BUFFER_SIZE = 2000,
    FORMAT_FLOAT_PRINTF_STACK_BUFFER_SIZE = 4096,
    FORMAT_FLOAT_PRINTF_MAX_TEMP_SIZE = 1024*1024,
    FORMAT_FLOAT_MAX_PRECISION = 1024,
    FORMAT_FLOAT_MAX_FIXED_PREFIX = 312,
    FORMAT_FLOAT_MAX_EXP_PREFIX = 8,
    FORMAT_LONG_DOUBLE_MAX_FIXED_PREFIX = LDBL_MAX_10_EXP + 8,
    FORMAT_DOUBLE_HEX_DIGITS = 13,
    FORMAT_DOUBLE_FRACTION_BITS = 52,
    FORMAT_DOUBLE_EXPONENT_BIAS = 1023,
    FORMAT_DOUBLE_SUBNORMAL_EXPONENT = -1022,
    FORMAT_BIG_UINT_WORD_BITS = 32,
    FORMAT_BIG_UINT_MAX_WORDS = 640,
    FORMAT_LONG_DOUBLE_DOUBLE_FRACTION_BITS = 52,
    FORMAT_LONG_DOUBLE_DOUBLE_EXPONENT_BIAS = 1023,
    FORMAT_LONG_DOUBLE_X87_FRACTION_BITS = 63,
    FORMAT_LONG_DOUBLE_X87_EXPONENT_BIAS = 16383,
    FORMAT_LONG_DOUBLE_X87_EXPONENT_MASK = 0x7fff,
    FORMAT_LONG_DOUBLE_BINARY128_FRACTION_BITS = 112,
    FORMAT_LONG_DOUBLE_BINARY128_EXPONENT_BIAS = 16383,
};

#if FLT_RADIX == 2 \
    && ((LDBL_MANT_DIG == DBL_MANT_DIG && LDBL_MAX_EXP == DBL_MAX_EXP) \
        || (LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384) \
        || (LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384))
#define FORMAT_LONG_DOUBLE_SUPPORTED 1
#else
#define FORMAT_LONG_DOUBLE_SUPPORTED 0
#endif

_Static_assert(FORMAT_FLOAT_MAX_FIXED_PREFIX
               + FORMAT_FLOAT_MAX_PRECISION < FORMAT_FLOAT_RYU_BUFFER_SIZE,
               "format fixed temporary buffer is too small");
_Static_assert(FORMAT_FLOAT_MAX_EXP_PREFIX
               + FORMAT_FLOAT_MAX_PRECISION < FORMAT_FLOAT_RYU_BUFFER_SIZE,
               "format scientific temporary buffer is too small");

enum {
    FORMAT_FLAG_LEFT = 1 << 0,
    FORMAT_FLAG_SIGN = 1 << 1,
    FORMAT_FLAG_SPACE = 1 << 2,
    FORMAT_FLAG_ALTERNATE = 1 << 3,
    FORMAT_FLAG_ZERO = 1 << 4,
};

enum FormatWidthKind {
    FORMAT_WIDTH_NONE,
    FORMAT_WIDTH_LITERAL,
    FORMAT_WIDTH_ARG,
};

enum FormatPrecisionKind {
    FORMAT_PRECISION_NONE,
    FORMAT_PRECISION_LITERAL,
    FORMAT_PRECISION_ARG,
};

enum FormatLength {
    FORMAT_LENGTH_NONE,
    FORMAT_LENGTH_HH,
    FORMAT_LENGTH_H,
    FORMAT_LENGTH_LL,
    FORMAT_LENGTH_BIG_L,
    FORMAT_LENGTH_W8,
    FORMAT_LENGTH_W16,
    FORMAT_LENGTH_W32,
    FORMAT_LENGTH_W64,
};

typedef struct FormatSpec {
    int32 flags;
    int64 width;
    int64 precision;
    enum FormatWidthKind width_kind;
    enum FormatPrecisionKind precision_kind;
    enum FormatLength length;
    char conversion;
} FormatSpec;

static bool
format_is_digit(char byte) {
    return byte >= '0' && byte <= '9';
}

static int32
format_digit_value(char byte) {
    ASSERT(format_is_digit(byte));
    return byte - '0';
}

static int32
format_parse_uint(char **cursor, int64 *value) {
    char *scan;
    int64 result;
    bool found_digit;

    ASSERT(cursor != NULL);
    ASSERT(*cursor != NULL);
    ASSERT(value != NULL);

    scan = *cursor;
    result = 0;
    found_digit = false;
    while (format_is_digit(*scan)) {
        int32 digit;

        digit = format_digit_value(*scan);
        if (result > (INT64_MAX - digit)/10) {
            return -EOVERFLOW;
        }

        result = result*10 + digit;
        scan += 1;
        found_digit = true;
    }

    if (!found_digit) {
        return -EINVAL;
    }

    *cursor = scan;
    *value = result;
    return 0;
}

static int32
format_reject_positional_prefix(char *cursor) {
    char *scan;

    ASSERT(cursor != NULL);

    if (!format_is_digit(*cursor)) {
        return 0;
    }

    scan = cursor;
    while (format_is_digit(*scan)) {
        scan += 1;
    }
    if (*scan == '$') {
        return -EINVAL;
    }

    return 0;
}

static int32
format_reject_star_positional(char *cursor) {
    char *scan;

    ASSERT(cursor != NULL);

    if (!format_is_digit(*cursor)) {
        return 0;
    }

    scan = cursor;
    while (format_is_digit(*scan)) {
        scan += 1;
    }
    if (*scan == '$') {
        return -EINVAL;
    }

    return 0;
}

static void
format_parse_flags(char **cursor, FormatSpec *spec) {
    ASSERT(cursor != NULL);
    ASSERT(*cursor != NULL);
    ASSERT(spec != NULL);

    for (;;) {
        if (**cursor == '-') {
            spec->flags |= FORMAT_FLAG_LEFT;
        } else if (**cursor == '+') {
            spec->flags |= FORMAT_FLAG_SIGN;
        } else if (**cursor == ' ') {
            spec->flags |= FORMAT_FLAG_SPACE;
        } else if (**cursor == '#') {
            spec->flags |= FORMAT_FLAG_ALTERNATE;
        } else if (**cursor == '0') {
            spec->flags |= FORMAT_FLAG_ZERO;
        } else {
            return;
        }

        *cursor += 1;
    }
}

static int32
format_parse_width(char **cursor, FormatSpec *spec) {
    int32 status;

    ASSERT(cursor != NULL);
    ASSERT(*cursor != NULL);
    ASSERT(spec != NULL);

    if (**cursor == '*') {
        *cursor += 1;
        if ((status = format_reject_star_positional(*cursor)) < 0) {
            return status;
        }
        spec->width_kind = FORMAT_WIDTH_ARG;
        return 0;
    }

    if (format_is_digit(**cursor)) {
        spec->width_kind = FORMAT_WIDTH_LITERAL;
        if ((status = format_parse_uint(cursor, &spec->width)) < 0) {
            return status;
        }
    }

    return 0;
}

static int32
format_parse_precision(char **cursor, FormatSpec *spec) {
    int32 status;

    ASSERT(cursor != NULL);
    ASSERT(*cursor != NULL);
    ASSERT(spec != NULL);

    if (**cursor != '.') {
        return 0;
    }

    *cursor += 1;
    if (**cursor == '*') {
        *cursor += 1;
        if ((status = format_reject_star_positional(*cursor)) < 0) {
            return status;
        }
        spec->precision_kind = FORMAT_PRECISION_ARG;
        return 0;
    }

    spec->precision_kind = FORMAT_PRECISION_LITERAL;
    if (format_is_digit(**cursor)) {
        char *scan;

        scan = *cursor;
        while (format_is_digit(*scan)) {
            scan += 1;
        }
        if (*scan == '$') {
            return -EINVAL;
        }

        if ((status = format_parse_uint(cursor, &spec->precision)) < 0) {
            return status;
        }
    }

    return 0;
}

static int32
format_parse_w_length(char **cursor, FormatSpec *spec) {
    int64 width;
    int32 status;

    ASSERT(cursor != NULL);
    ASSERT(*cursor != NULL);
    ASSERT(spec != NULL);
    ASSERT(**cursor == 'w');

    *cursor += 1;
    if (**cursor == 'f') {
        return -EINVAL;
    }
    if (!format_is_digit(**cursor)) {
        return -EINVAL;
    }

    if ((status = format_parse_uint(cursor, &width)) < 0) {
        return status;
    }

    if (width == 8) {
        spec->length = FORMAT_LENGTH_W8;
    } else if (width == 16) {
        spec->length = FORMAT_LENGTH_W16;
    } else if (width == 32) {
        spec->length = FORMAT_LENGTH_W32;
    } else if (width == 64) {
        spec->length = FORMAT_LENGTH_W64;
    } else {
        return -EINVAL;
    }

    return 0;
}

static int32
format_parse_length(char **cursor, FormatSpec *spec) {
    ASSERT(cursor != NULL);
    ASSERT(*cursor != NULL);
    ASSERT(spec != NULL);

    if ((*cursor)[0] == 'h' && (*cursor)[1] == 'h') {
        spec->length = FORMAT_LENGTH_HH;
        *cursor += 2;
    } else if (**cursor == 'h') {
        spec->length = FORMAT_LENGTH_H;
        *cursor += 1;
    } else if ((*cursor)[0] == 'l' && (*cursor)[1] == 'l') {
        spec->length = FORMAT_LENGTH_LL;
        *cursor += 2;
    } else if (**cursor == 'l') {
        return -EINVAL;
    } else if (**cursor == 'L') {
        spec->length = FORMAT_LENGTH_BIG_L;
        *cursor += 1;
    } else if (**cursor == 'w') {
        return format_parse_w_length(cursor, spec);
    } else if (**cursor == 'j' || **cursor == 'z' || **cursor == 't') {
        return -EINVAL;
    }

    return 0;
}

static bool
format_is_integer_conversion(char conversion) {
    return conversion == 'd'
           || conversion == 'i'
           || conversion == 'u'
           || conversion == 'o'
           || conversion == 'x'
           || conversion == 'X'
           || conversion == 'b'
           || conversion == 'B';
}

static bool
format_is_float_conversion(char conversion) {
    return conversion == 'f'
           || conversion == 'F'
           || conversion == 'e'
           || conversion == 'E'
           || conversion == 'g'
           || conversion == 'G'
           || conversion == 'a'
           || conversion == 'A';
}

static bool
format_length_is_integer(enum FormatLength length) {
    return length == FORMAT_LENGTH_NONE
           || length == FORMAT_LENGTH_HH
           || length == FORMAT_LENGTH_H
           || length == FORMAT_LENGTH_LL
           || length == FORMAT_LENGTH_W8
           || length == FORMAT_LENGTH_W16
           || length == FORMAT_LENGTH_W32
           || length == FORMAT_LENGTH_W64;
}

static bool
format_length_is_char_string(enum FormatLength length) {
    return length == FORMAT_LENGTH_NONE;
}

static bool
format_length_is_float(enum FormatLength length) {
    return length == FORMAT_LENGTH_NONE || length == FORMAT_LENGTH_BIG_L;
}

static bool
format_has_width(FormatSpec *spec) {
    ASSERT(spec != NULL);
    return spec->width_kind != FORMAT_WIDTH_NONE;
}

static bool
format_has_precision(FormatSpec *spec) {
    ASSERT(spec != NULL);
    return spec->precision_kind != FORMAT_PRECISION_NONE;
}

static int32
format_validate_flags(FormatSpec *spec, int32 allowed_flags) {
    ASSERT(spec != NULL);

    if ((spec->flags & ~allowed_flags) != 0) {
        return -EINVAL;
    }

    return 0;
}

static int32
format_validate_spec(FormatSpec *spec) {
    ASSERT(spec != NULL);

    if (format_is_integer_conversion(spec->conversion)) {
        if (!format_length_is_integer(spec->length)) {
            return -EINVAL;
        }
        return 0;
    }

    if (format_is_float_conversion(spec->conversion)) {
        if (!format_length_is_float(spec->length)) {
            return -EINVAL;
        }
        return 0;
    }

    if (spec->conversion == 'c') {
        if (!format_length_is_char_string(spec->length)) {
            return -EINVAL;
        }
        if (format_has_precision(spec)) {
            return -EINVAL;
        }
        return format_validate_flags(spec, FORMAT_FLAG_LEFT);
    }

    if (spec->conversion == 's') {
        if (!format_length_is_char_string(spec->length)) {
            return -EINVAL;
        }
        return format_validate_flags(spec, FORMAT_FLAG_LEFT);
    }

    if (spec->conversion == 'p') {
        if (spec->length != FORMAT_LENGTH_NONE) {
            return -EINVAL;
        }
        if (format_has_precision(spec)) {
            return -EINVAL;
        }
        return format_validate_flags(spec, FORMAT_FLAG_LEFT);
    }

    if (spec->conversion == 'n') {
        if (!format_length_is_integer(spec->length)) {
            return -EINVAL;
        }
        if (spec->flags != 0 || format_has_width(spec)
            || format_has_precision(spec)) {
            return -EINVAL;
        }
        return 0;
    }

    if (spec->conversion == '%') {
        if (spec->length != FORMAT_LENGTH_NONE) {
            return -EINVAL;
        }
        if (spec->flags != 0 || format_has_width(spec)
            || format_has_precision(spec)) {
            return -EINVAL;
        }
        return 0;
    }

    return -EINVAL;
}

static int32
format_parse_spec(char *cursor, char **next, FormatSpec *spec) {
    int32 status;

    ASSERT(cursor != NULL);
    ASSERT(next != NULL);
    ASSERT(spec != NULL);

    memset(spec, 0, SIZEOF(*spec));

    if (*cursor == '\0') {
        return -EINVAL;
    }
    if ((status = format_reject_positional_prefix(cursor)) < 0) {
        return status;
    }

    format_parse_flags(&cursor, spec);
    if ((status = format_parse_width(&cursor, spec)) < 0) {
        return status;
    }
    if ((status = format_parse_precision(&cursor, spec)) < 0) {
        return status;
    }
    if ((status = format_parse_length(&cursor, spec)) < 0) {
        return status;
    }

    if (*cursor == '\0') {
        return -EINVAL;
    }

    spec->conversion = *cursor;
    cursor += 1;
    if ((status = format_validate_spec(spec)) < 0) {
        return status;
    }

    *next = cursor;
    return 0;
}

typedef struct FormatSink {
    char *buffer;
    int64 capacity;
    int64 written;
    int64 total;
    int32 status;
} FormatSink;

static int32
format_sink_init(FormatSink *sink, char *buffer, int64 capacity) {
    ASSERT(sink != NULL);

    if (capacity < 0) {
        return -EINVAL;
    }
    if (capacity > 0 && buffer == NULL) {
        return -EINVAL;
    }

    sink->buffer = buffer;
    sink->capacity = capacity;
    sink->written = 0;
    sink->total = 0;
    sink->status = 0;

    if (capacity > 0) {
        buffer[0] = '\0';
    }

    return 0;
}

static void
format_sink_add_total(FormatSink *sink, int64 len) {
    ASSERT(sink != NULL);
    ASSERT_NON_NEGATIVE(len);

    if (sink->status < 0) {
        return;
    }
    if (len > INT64_MAX - sink->total) {
        sink->status = -EOVERFLOW;
        return;
    }

    sink->total += len;
    return;
}

static void
format_sink_write(FormatSink *sink, char *data, int64 len) {
    int64 available;
    int64 copy_len;

    ASSERT(sink != NULL);
    ASSERT(data != NULL);
    ASSERT_NON_NEGATIVE(len);

    if (sink->status < 0) {
        return;
    }

    format_sink_add_total(sink, len);
    if (sink->status < 0) {
        return;
    }
    if (sink->capacity <= 0) {
        return;
    }

    ASSERT(sink->buffer != NULL);
    ASSERT_LESS(sink->written, sink->capacity);

    available = sink->capacity - 1 - sink->written;
    if (available <= 0) {
        return;
    }

    copy_len = MIN(len, available);
    memcpy(sink->buffer + sink->written, data, (size_t)copy_len);
    sink->written += copy_len;
    sink->buffer[sink->written] = '\0';
    return;
}

static void
format_sink_write_byte(FormatSink *sink, char byte) {
    format_sink_write(sink, &byte, 1);
    return;
}

static int32
format_sink_finish(FormatSink *sink) {
    ASSERT(sink != NULL);

    if (sink->status < 0) {
        return sink->status;
    }
    if (sink->total > INT32_MAX) {
        return -EOVERFLOW;
    }

    return (int32)sink->total;
}

static void
format_sink_write_repeat(FormatSink *sink, char byte, int64 len) {
    int64 available;
    int64 copy_len;

    ASSERT(sink != NULL);
    ASSERT_NON_NEGATIVE(len);

    if (sink->status < 0) {
        return;
    }

    format_sink_add_total(sink, len);
    if (sink->status < 0) {
        return;
    }
    if (sink->capacity <= 0) {
        return;
    }

    ASSERT(sink->buffer != NULL);
    ASSERT_LESS(sink->written, sink->capacity);

    available = sink->capacity - 1 - sink->written;
    if (available <= 0) {
        return;
    }

    copy_len = MIN(len, available);
    memset(sink->buffer + sink->written, byte, (size_t)copy_len);
    sink->written += copy_len;
    sink->buffer[sink->written] = '\0';
    return;
}

static bool
format_is_signed_integer_conversion(char conversion) {
    return conversion == 'd' || conversion == 'i';
}

typedef struct FormatArgs {
    va_list args;
} FormatArgs;

static int32
format_load_dynamic_width(FormatSpec *spec, FormatArgs *args) {
    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if (spec->width_kind == FORMAT_WIDTH_ARG) {
        int32 width = va_arg(args->args, int32);

        if (width < 0) {
            spec->flags |= FORMAT_FLAG_LEFT;
            spec->width = -(int64)width;
        } else {
            spec->width = width;
        }
        spec->width_kind = FORMAT_WIDTH_LITERAL;
    }

    return 0;
}

static int32
format_load_dynamic_width_precision(FormatSpec *spec, FormatArgs *args) {
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if ((status = format_load_dynamic_width(spec, args)) < 0) {
        return status;
    }

    if (spec->precision_kind == FORMAT_PRECISION_ARG) {
        int32 precision = va_arg(args->args, int32);

        if (precision < 0) {
            spec->precision = 0;
            spec->precision_kind = FORMAT_PRECISION_NONE;
        } else {
            spec->precision = precision;
            spec->precision_kind = FORMAT_PRECISION_LITERAL;
        }
    }

    return 0;
}

typedef struct FormatIntegerValue {
    uint64 magnitude;
    bool negative;
} FormatIntegerValue;

static uint64
format_signed_magnitude(int64 value, bool *negative) {
    ASSERT(negative != NULL);

    if (value < 0) {
        *negative = true;
        return (uint64)(-(value + 1)) + 1;
    }

    *negative = false;
    return (uint64)value;
}

static FormatIntegerValue
format_read_signed_integer(FormatSpec *spec, FormatArgs *args) {
    FormatIntegerValue value;
    int64 signed_value;

    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if (spec->length == FORMAT_LENGTH_HH) {
        signed_value = (int8)va_arg(args->args, int32);
    } else if (spec->length == FORMAT_LENGTH_H) {
        signed_value = (int16)va_arg(args->args, int32);
    } else if (spec->length == FORMAT_LENGTH_LL) {
        signed_value = va_arg(args->args, int64);
    } else if (spec->length == FORMAT_LENGTH_W8) {
        signed_value = (int8)va_arg(args->args, int32);
    } else if (spec->length == FORMAT_LENGTH_W16) {
        signed_value = (int16)va_arg(args->args, int32);
    } else if (spec->length == FORMAT_LENGTH_W32) {
        signed_value = va_arg(args->args, int32);
    } else if (spec->length == FORMAT_LENGTH_W64) {
        signed_value = va_arg(args->args, int64);
    } else {
        ASSERT(spec->length == FORMAT_LENGTH_NONE);
        signed_value = va_arg(args->args, int32);
    }

    value.magnitude = format_signed_magnitude(signed_value, &value.negative);
    return value;
}

static FormatIntegerValue
format_read_unsigned_integer(FormatSpec *spec, FormatArgs *args) {
    FormatIntegerValue value;

    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    value.negative = false;
    if (spec->length == FORMAT_LENGTH_HH) {
        value.magnitude = (uint8)va_arg(args->args, int32);
    } else if (spec->length == FORMAT_LENGTH_H) {
        value.magnitude = (uint16)va_arg(args->args, int32);
    } else if (spec->length == FORMAT_LENGTH_LL) {
        value.magnitude = va_arg(args->args, uint64);
    } else if (spec->length == FORMAT_LENGTH_W8) {
        value.magnitude = (uint8)va_arg(args->args, int32);
    } else if (spec->length == FORMAT_LENGTH_W16) {
        value.magnitude = (uint16)va_arg(args->args, int32);
    } else if (spec->length == FORMAT_LENGTH_W32) {
        value.magnitude = va_arg(args->args, uint32);
    } else if (spec->length == FORMAT_LENGTH_W64) {
        value.magnitude = va_arg(args->args, uint64);
    } else {
        ASSERT(spec->length == FORMAT_LENGTH_NONE);
        value.magnitude = va_arg(args->args, uint32);
    }

    return value;
}

static int32
format_integer_base(char conversion) {
    if (conversion == 'b' || conversion == 'B') {
        return 2;
    }
    if (conversion == 'o') {
        return 8;
    }
    if (conversion == 'x' || conversion == 'X') {
        return 16;
    }

    return 10;
}

static int32
format_integer_digits(char *digits, uint64 value, int32 base, bool upper) {
    char lower_digits[] = "0123456789abcdef";
    char upper_digits[] = "0123456789ABCDEF";
    char reversed[64];
    char *digit_table;
    int32 len;

    ASSERT(digits != NULL);
    ASSERT(base == 2 || base == 8 || base == 10 || base == 16);

    if (upper) {
        digit_table = upper_digits;
    } else {
        digit_table = lower_digits;
    }

    if (value == 0) {
        digits[0] = '0';
        return 1;
    }

    len = 0;
    while (value > 0) {
        uint64 digit = value%(uint64)base;

        reversed[len] = digit_table[digit];
        value /= (uint64)base;
        len += 1;
    }

    for (int32 i = 0; i < len; i += 1) {
        digits[i] = reversed[len - 1 - i];
    }

    return len;
}

static int64
format_pad_len(int64 width, int64 used) {
    if (width > used) {
        return width - used;
    }

    return 0;
}

static int32
format_integer_prefix(FormatSpec *spec, FormatIntegerValue value,
                      char *prefix, int32 digit_len, int64 precision_zeros) {
    ASSERT(spec != NULL);
    ASSERT(prefix != NULL);
    ASSERT_NON_NEGATIVE(digit_len);
    ASSERT_NON_NEGATIVE(precision_zeros);

    if (format_is_signed_integer_conversion(spec->conversion)) {
        if (value.negative) {
            prefix[0] = '-';
            return 1;
        }
        if ((spec->flags & FORMAT_FLAG_SIGN) != 0) {
            prefix[0] = '+';
            return 1;
        }
        if ((spec->flags & FORMAT_FLAG_SPACE) != 0) {
            prefix[0] = ' ';
            return 1;
        }
        return 0;
    }

    if ((spec->flags & FORMAT_FLAG_ALTERNATE) == 0) {
        return 0;
    }
    if (spec->conversion == 'o') {
        if (value.magnitude == 0 && digit_len == 0) {
            prefix[0] = '0';
            return 1;
        }
        if (value.magnitude != 0 && precision_zeros == 0) {
            prefix[0] = '0';
            return 1;
        }
        return 0;
    }
    if (value.magnitude == 0) {
        return 0;
    }
    if (spec->conversion == 'x') {
        prefix[0] = '0';
        prefix[1] = 'x';
        return 2;
    }
    if (spec->conversion == 'X') {
        prefix[0] = '0';
        prefix[1] = 'X';
        return 2;
    }
    if (spec->conversion == 'b') {
        prefix[0] = '0';
        prefix[1] = 'b';
        return 2;
    }
    if (spec->conversion == 'B') {
        prefix[0] = '0';
        prefix[1] = 'B';
        return 2;
    }

    return 0;
}

static void
format_write_integer(FormatSink *sink, FormatSpec *spec,
                     FormatIntegerValue value) {
    char digits[64];
    char prefix[2];
    int32 base;
    int32 digit_len;
    int32 prefix_len;
    int64 precision_zeros;
    int64 zero_pad;
    int64 inner_len;
    int64 spaces;
    bool upper;

    ASSERT(sink != NULL);
    ASSERT(spec != NULL);

    base = format_integer_base(spec->conversion);
    upper = spec->conversion == 'X' || spec->conversion == 'B';
    if (value.magnitude == 0 && format_has_precision(spec)
        && spec->precision == 0) {
        digit_len = 0;
    } else {
        digit_len = format_integer_digits(digits, value.magnitude, base, upper);
    }

    precision_zeros = 0;
    if (format_has_precision(spec) && spec->precision > digit_len) {
        precision_zeros = spec->precision - digit_len;
    }

    prefix_len = format_integer_prefix(spec, value, prefix, digit_len,
                                       precision_zeros);
    inner_len = prefix_len + precision_zeros + digit_len;

    zero_pad = 0;
    if ((spec->flags & FORMAT_FLAG_ZERO) != 0
        && (spec->flags & FORMAT_FLAG_LEFT) == 0
        && !format_has_precision(spec)) {
        zero_pad = format_pad_len(spec->width, inner_len);
    }

    spaces = format_pad_len(spec->width, inner_len + zero_pad);
    if ((spec->flags & FORMAT_FLAG_LEFT) == 0) {
        format_sink_write_repeat(sink, ' ', spaces);
    }
    if (prefix_len > 0) {
        format_sink_write(sink, prefix, prefix_len);
    }
    format_sink_write_repeat(sink, '0', zero_pad);
    format_sink_write_repeat(sink, '0', precision_zeros);
    if (digit_len > 0) {
        format_sink_write(sink, digits, digit_len);
    }
    if ((spec->flags & FORMAT_FLAG_LEFT) != 0) {
        format_sink_write_repeat(sink, ' ', spaces);
    }
    return;
}

static int32
format_handle_integer(FormatSink *sink, FormatSpec *spec, FormatArgs *args) {
    FormatIntegerValue value;
    int32 status;

    ASSERT(sink != NULL);
    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if ((status = format_load_dynamic_width_precision(spec, args)) < 0) {
        return status;
    }

    if (format_is_signed_integer_conversion(spec->conversion)) {
        value = format_read_signed_integer(spec, args);
    } else {
        value = format_read_unsigned_integer(spec, args);
    }

    format_write_integer(sink, spec, value);
    return sink->status;
}

static int64
format_string_len_limited(char *string, int64 limit) {
    int64 len;

    ASSERT(string != NULL);
    ASSERT_NON_NEGATIVE(limit);

    len = 0;
    while (len < limit && string[len] != '\0') {
        len += 1;
    }

    return len;
}

static void
format_write_padded_bytes(FormatSink *sink, FormatSpec *spec,
                          char *data, int64 len) {
    int64 spaces;

    ASSERT(sink != NULL);
    ASSERT(spec != NULL);
    ASSERT_NON_NEGATIVE(len);
    ASSERT(data != NULL || len == 0);

    spaces = format_pad_len(spec->width, len);
    if ((spec->flags & FORMAT_FLAG_LEFT) == 0) {
        format_sink_write_repeat(sink, ' ', spaces);
    }
    if (len > 0) {
        format_sink_write(sink, data, len);
    }
    if ((spec->flags & FORMAT_FLAG_LEFT) != 0) {
        format_sink_write_repeat(sink, ' ', spaces);
    }
    return;
}

static int32
format_handle_char(FormatSink *sink, FormatSpec *spec, FormatArgs *args) {
    int32 status;
    int32 value;
    char byte;

    ASSERT(sink != NULL);
    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if ((status = format_load_dynamic_width(spec, args)) < 0) {
        return status;
    }

    value = va_arg(args->args, int32);
    byte = (char)(uint8)value;
    format_write_padded_bytes(sink, spec, &byte, 1);
    return sink->status;
}

static int32
format_load_string_precision(FormatSpec *spec, FormatArgs *args,
                             bool *exact_span) {
    ASSERT(spec != NULL);
    ASSERT(args != NULL);
    ASSERT(exact_span != NULL);

    *exact_span = false;
    if (spec->precision_kind == FORMAT_PRECISION_ARG) {
        int32 precision = va_arg(args->args, int32);

        if (precision < 0) {
            return -EINVAL;
        }

        *exact_span = true;
        spec->precision = precision;
        spec->precision_kind = FORMAT_PRECISION_LITERAL;
    }

    return 0;
}

static int32
format_handle_string(FormatSink *sink, FormatSpec *spec, FormatArgs *args) {
    int32 status;
    int64 len;
    char *string;
    bool exact_span;

    ASSERT(sink != NULL);
    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if ((status = format_load_dynamic_width(spec, args)) < 0) {
        return status;
    }
    if ((status = format_load_string_precision(spec, args, &exact_span)) < 0) {
        return status;
    }

    string = va_arg(args->args, char *);
    if (exact_span) {
        len = spec->precision;
        if (string == NULL && len > 0) {
            return -EINVAL;
        }
    } else {
        if (string == NULL) {
            string = "(null)";
        }
        if (format_has_precision(spec)) {
            len = format_string_len_limited(string, spec->precision);
        } else {
            len = format_string_len_limited(string, INT64_MAX);
        }
    }

    format_write_padded_bytes(sink, spec, string, len);
    return sink->status;
}


static int32
format_handle_char_string(FormatSink *sink, FormatSpec *spec,
                          FormatArgs *args) {
    ASSERT(sink != NULL);
    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if (spec->conversion == 'c') {
        return format_handle_char(sink, spec, args);
    }

    ASSERT(spec->conversion == 's');
    return format_handle_string(sink, spec, args);
}


static void
format_write_pointer(FormatSink *sink, FormatSpec *spec, void *pointer) {
    char digits[64];
    char prefix[] = {'0', 'x'};
    uintptr value;
    int32 digit_len;
    int64 inner_len;
    int64 spaces;

    ASSERT(sink != NULL);
    ASSERT(spec != NULL);

    value = (uintptr)pointer;
    digit_len = format_integer_digits(digits, (uint64)value, 16, false);
    inner_len = 2 + digit_len;
    spaces = format_pad_len(spec->width, inner_len);

    if ((spec->flags & FORMAT_FLAG_LEFT) == 0) {
        format_sink_write_repeat(sink, ' ', spaces);
    }
    format_sink_write(sink, prefix, 2);
    format_sink_write(sink, digits, digit_len);
    if ((spec->flags & FORMAT_FLAG_LEFT) != 0) {
        format_sink_write_repeat(sink, ' ', spaces);
    }
    return;
}

static int32
format_handle_pointer(FormatSink *sink, FormatSpec *spec, FormatArgs *args) {
    int32 status;
    void *pointer;

    ASSERT(sink != NULL);
    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if ((status = format_load_dynamic_width(spec, args)) < 0) {
        return status;
    }

    pointer = va_arg(args->args, void *);
    format_write_pointer(sink, spec, pointer);
    return sink->status;
}

static bool
format_count_fits(FormatSpec *spec, int64 count) {
    ASSERT(spec != NULL);
    ASSERT_NON_NEGATIVE(count);

    if (spec->length == FORMAT_LENGTH_HH
        || spec->length == FORMAT_LENGTH_W8) {
        return count <= INT8_MAX;
    }
    if (spec->length == FORMAT_LENGTH_H
        || spec->length == FORMAT_LENGTH_W16) {
        return count <= INT16_MAX;
    }
    if (spec->length == FORMAT_LENGTH_LL
        || spec->length == FORMAT_LENGTH_W64) {
        return count <= INT64_MAX;
    }

    ASSERT(spec->length == FORMAT_LENGTH_NONE
           || spec->length == FORMAT_LENGTH_W32);
    return count <= INT32_MAX;
}

static int32
format_store_count(FormatSpec *spec, FormatArgs *args, int64 count) {
    ASSERT(spec != NULL);
    ASSERT(args != NULL);
    ASSERT_NON_NEGATIVE(count);

    if (!format_count_fits(spec, count)) {
        return -EOVERFLOW;
    }

    if (spec->length == FORMAT_LENGTH_HH) {
        int8 *pointer = va_arg(args->args, int8 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        *pointer = (int8)count;
        return 0;
    }
    if (spec->length == FORMAT_LENGTH_H) {
        int16 *pointer = va_arg(args->args, int16 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        *pointer = (int16)count;
        return 0;
    }
    if (spec->length == FORMAT_LENGTH_LL) {
        int64 *pointer = va_arg(args->args, int64 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        *pointer = count;
        return 0;
    }
    if (spec->length == FORMAT_LENGTH_W8) {
        int8 *pointer = va_arg(args->args, int8 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        *pointer = (int8)count;
        return 0;
    }
    if (spec->length == FORMAT_LENGTH_W16) {
        int16 *pointer = va_arg(args->args, int16 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        *pointer = (int16)count;
        return 0;
    }
    if (spec->length == FORMAT_LENGTH_W32) {
        int32 *pointer = va_arg(args->args, int32 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        *pointer = (int32)count;
        return 0;
    }
    if (spec->length == FORMAT_LENGTH_W64) {
        int64 *pointer = va_arg(args->args, int64 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        *pointer = count;
        return 0;
    }

    ASSERT(spec->length == FORMAT_LENGTH_NONE);
    {
        int32 *pointer = va_arg(args->args, int32 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        *pointer = (int32)count;
        return 0;
    }
}

static int32
format_handle_count(FormatSink *sink, FormatSpec *spec, FormatArgs *args) {
    ASSERT(sink != NULL);
    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if (sink->status < 0) {
        return sink->status;
    }
    return format_store_count(spec, args, sink->total);
}


typedef struct FormatBigUInt {
    uint32 words[FORMAT_BIG_UINT_MAX_WORDS];
    int32 len;
} FormatBigUInt;

enum FormatRemainderHalf {
    FORMAT_REMAINDER_ZERO,
    FORMAT_REMAINDER_LESS_HALF,
    FORMAT_REMAINDER_HALF,
    FORMAT_REMAINDER_MORE_HALF,
};

typedef struct FormatBinaryFloat {
    FormatBigUInt significand;
    int32 binary_exponent;
    int32 precision_bits;
    bool negative;
    bool zero;
} FormatBinaryFloat;

static void
format_big_uint_zero(FormatBigUInt *value) {
    ASSERT(value != NULL);

    memset(value->words, 0, (size_t)SIZEOF(value->words));
    value->len = 0;
    return;
}

static void
format_big_uint_normalize(FormatBigUInt *value) {
    ASSERT(value != NULL);
    ASSERT_MORE_EQUAL(value->len, 0);
    ASSERT_LESS_EQUAL(value->len, FORMAT_BIG_UINT_MAX_WORDS);

    while (value->len > 0 && value->words[value->len - 1] == 0) {
        value->len -= 1;
    }
    return;
}

static bool
format_big_uint_is_zero(FormatBigUInt *value) {
    ASSERT(value != NULL);

    return value->len == 0;
}

static int32
format_big_uint_ensure_word(FormatBigUInt *value, int32 index) {
    ASSERT(value != NULL);
    ASSERT_NON_NEGATIVE(index);

    if (index >= FORMAT_BIG_UINT_MAX_WORDS) {
        return -EOVERFLOW;
    }
    while (value->len <= index) {
        value->words[value->len] = 0;
        value->len += 1;
    }
    return 0;
}

static int32 UNUSED
format_big_uint_set_bit(FormatBigUInt *value, int32 bit_index) {
    int32 word_index;
    int32 bit_offset;
    int32 status;

    ASSERT(value != NULL);
    ASSERT_NON_NEGATIVE(bit_index);

    word_index = bit_index/FORMAT_BIG_UINT_WORD_BITS;
    bit_offset = bit_index%FORMAT_BIG_UINT_WORD_BITS;
    if ((status = format_big_uint_ensure_word(value, word_index)) < 0) {
        return status;
    }

    value->words[word_index] |= UINT32_C(1) << bit_offset;
    return 0;
}

static int32 UNUSED
format_big_uint_from_uint64(FormatBigUInt *value, uint64 source) {
    ASSERT(value != NULL);

    format_big_uint_zero(value);
    if (source == 0) {
        return 0;
    }

    value->words[0] = (uint32)source;
    value->words[1] = (uint32)(source >> 32);
    value->len = 2;
    format_big_uint_normalize(value);
    return 0;
}

static int32 UNUSED
format_big_uint_from_uint128_parts(FormatBigUInt *value, uint64 low,
                                   uint64 high) {
    ASSERT(value != NULL);

    format_big_uint_zero(value);
    value->words[0] = (uint32)low;
    value->words[1] = (uint32)(low >> 32);
    value->words[2] = (uint32)high;
    value->words[3] = (uint32)(high >> 32);
    value->len = 4;
    format_big_uint_normalize(value);
    return 0;
}

static int32 UNUSED
format_big_uint_add_one(FormatBigUInt *value) {
    uint64 carry;

    ASSERT(value != NULL);

    carry = 1;
    for (int32 i = 0; i < value->len; i += 1) {
        uint64 sum;

        sum = (uint64)value->words[i] + carry;
        value->words[i] = (uint32)sum;
        carry = sum >> 32;
        if (carry == 0) {
            return 0;
        }
    }

    if (carry != 0) {
        int32 status;

        status = format_big_uint_ensure_word(value, value->len);
        if (status < 0) {
            return status;
        }
        value->words[value->len - 1] = (uint32)carry;
    }
    return 0;
}

static uint64 UNUSED
format_read_le_uint64(uchar *bytes) {
    uint64 value;

    ASSERT(bytes != NULL);

    value = 0;
    for (int32 i = 7; i >= 0; i -= 1) {
        value <<= 8;
        value |= bytes[i];
    }
    return value;
}

static uint64 UNUSED
format_read_be_uint64(uchar *bytes) {
    uint64 value;

    ASSERT(bytes != NULL);

    value = 0;
    for (int32 i = 0; i < 8; i += 1) {
        value <<= 8;
        value |= bytes[i];
    }
    return value;
}

static bool UNUSED
format_host_is_little_endian(void) {
    uint32 one;
    uchar bytes[SIZEOF(one)];

    one = 1;
    memcpy(bytes, &one, (size_t)SIZEOF(one));
    return bytes[0] == 1;
}

static int32 UNUSED
format_big_uint_bit_len(FormatBigUInt *value) {
    uint32 top;
    int32 bits;

    ASSERT(value != NULL);

    if (value->len == 0) {
        return 0;
    }

    top = value->words[value->len - 1];
    bits = 0;
    while (top > 0) {
        bits += 1;
        top >>= 1;
    }
    return (value->len - 1)*FORMAT_BIG_UINT_WORD_BITS + bits;
}

static bool
format_big_uint_test_bit(FormatBigUInt *value, int32 bit_index) {
    int32 word_index;
    int32 bit_offset;

    ASSERT(value != NULL);
    ASSERT_NON_NEGATIVE(bit_index);

    word_index = bit_index/FORMAT_BIG_UINT_WORD_BITS;
    bit_offset = bit_index%FORMAT_BIG_UINT_WORD_BITS;
    if (word_index >= value->len) {
        return false;
    }
    return (value->words[word_index] & (UINT32_C(1) << bit_offset)) != 0;
}

static int32
format_big_uint_shift_left(FormatBigUInt *value, int32 shift) {
    uint32 original[FORMAT_BIG_UINT_MAX_WORDS];
    int32 original_len;
    int32 word_shift;
    int32 bit_shift;
    int32 new_len;

    ASSERT(value != NULL);
    ASSERT_NON_NEGATIVE(shift);

    if (value->len == 0 || shift == 0) {
        return 0;
    }

    original_len = value->len;
    word_shift = shift/FORMAT_BIG_UINT_WORD_BITS;
    bit_shift = shift%FORMAT_BIG_UINT_WORD_BITS;
    new_len = original_len + word_shift;
    if (bit_shift != 0) {
        new_len += 1;
    }
    if (new_len > FORMAT_BIG_UINT_MAX_WORDS) {
        return -EOVERFLOW;
    }

    memcpy(original, value->words, (size_t)(original_len*SIZEOF(original[0])));
    memset(value->words, 0, (size_t)SIZEOF(value->words));
    value->len = new_len;
    for (int32 i = 0; i < original_len; i += 1) {
        uint64 shifted;
        int32 index;

        shifted = (uint64)original[i] << bit_shift;
        index = i + word_shift;
        value->words[index] |= (uint32)shifted;
        if (bit_shift != 0) {
            value->words[index + 1] |= (uint32)(shifted >> 32);
        }
    }

    format_big_uint_normalize(value);
    return 0;
}

static void
format_big_uint_shift_right(FormatBigUInt *value, int32 shift) {
    int32 word_shift;
    int32 bit_shift;

    ASSERT(value != NULL);
    ASSERT_NON_NEGATIVE(shift);

    if (value->len == 0 || shift == 0) {
        return;
    }

    word_shift = shift/FORMAT_BIG_UINT_WORD_BITS;
    bit_shift = shift%FORMAT_BIG_UINT_WORD_BITS;
    if (word_shift >= value->len) {
        format_big_uint_zero(value);
        return;
    }

    for (int32 i = 0; i + word_shift < value->len; i += 1) {
        uint32 low;
        uint32 high;

        low = value->words[i + word_shift];
        high = 0;
        if (bit_shift != 0 && i + word_shift + 1 < value->len) {
            high = value->words[i + word_shift + 1];
        }
        if (bit_shift == 0) {
            value->words[i] = low;
        } else {
            value->words[i] = (low >> bit_shift)
                              |(high << (FORMAT_BIG_UINT_WORD_BITS
                                         - bit_shift));
        }
    }
    value->len -= word_shift;
    format_big_uint_normalize(value);
    return;
}

static bool
format_big_uint_has_low_bits(FormatBigUInt *value, int32 bits) {
    int32 full_words;
    int32 partial_bits;

    ASSERT(value != NULL);
    ASSERT_NON_NEGATIVE(bits);

    if (bits == 0 || value->len == 0) {
        return false;
    }

    full_words = bits/FORMAT_BIG_UINT_WORD_BITS;
    partial_bits = bits%FORMAT_BIG_UINT_WORD_BITS;
    for (int32 i = 0; i < full_words && i < value->len; i += 1) {
        if (value->words[i] != 0) {
            return true;
        }
    }
    if (partial_bits != 0 && full_words < value->len) {
        uint32 mask;

        mask = (UINT32_C(1) << partial_bits) - 1;
        if ((value->words[full_words] & mask) != 0) {
            return true;
        }
    }
    return false;
}

static enum FormatRemainderHalf
format_big_uint_remainder_half(FormatBigUInt *value, int32 bits) {
    int32 half_bit;

    ASSERT(value != NULL);
    ASSERT_POSITIVE(bits);

    if (!format_big_uint_has_low_bits(value, bits)) {
        return FORMAT_REMAINDER_ZERO;
    }

    half_bit = bits - 1;
    if (!format_big_uint_test_bit(value, half_bit)) {
        return FORMAT_REMAINDER_LESS_HALF;
    }
    if (format_big_uint_has_low_bits(value, half_bit)) {
        return FORMAT_REMAINDER_MORE_HALF;
    }
    return FORMAT_REMAINDER_HALF;
}

static int32
format_big_uint_mul_small(FormatBigUInt *value, uint32 factor) {
    uint64 carry;

    ASSERT(value != NULL);

    if (value->len == 0 || factor == 1) {
        return 0;
    }
    if (factor == 0) {
        format_big_uint_zero(value);
        return 0;
    }

    carry = 0;
    for (int32 i = 0; i < value->len; i += 1) {
        uint64 product;

        product = (uint64)value->words[i]*factor + carry;
        value->words[i] = (uint32)product;
        carry = product >> 32;
    }
    if (carry != 0) {
        int32 status;

        status = format_big_uint_ensure_word(value, value->len);
        if (status < 0) {
            return status;
        }
        value->words[value->len - 1] = (uint32)carry;
    }
    return 0;
}

static uint32
format_big_uint_div_small(FormatBigUInt *value, uint32 divisor) {
    uint64 remainder;

    ASSERT(value != NULL);
    ASSERT_POSITIVE(divisor);

    remainder = 0;
    for (int32 i = value->len - 1; i >= 0; i -= 1) {
        uint64 current;
        uint64 quotient;

        current = (remainder << 32) | value->words[i];
        quotient = current/divisor;
        remainder = current%divisor;
        value->words[i] = (uint32)quotient;
    }
    format_big_uint_normalize(value);
    return (uint32)remainder;
}

static int32 UNUSED
format_big_uint_to_decimal(FormatBigUInt *value, char *buffer,
                           int32 capacity) {
    enum { GROUP_BASE = 1000000000, GROUP_DIGITS = 9 };
    uint32 groups[FORMAT_BIG_UINT_MAX_WORDS*2];
    FormatBigUInt work;
    int32 group_count;
    int32 len;

    ASSERT(value != NULL);
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);

    if (value->len == 0) {
        if (capacity < 2) {
            return -EOVERFLOW;
        }
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }

    work = *value;
    group_count = 0;
    while (!format_big_uint_is_zero(&work)) {
        groups[group_count] = format_big_uint_div_small(&work, GROUP_BASE);
        group_count += 1;
    }

    len = format_integer_digits(buffer, groups[group_count - 1], 10, false);
    if (len >= capacity) {
        return -EOVERFLOW;
    }

    for (int32 i = group_count - 2; i >= 0; i -= 1) {
        char digits[GROUP_DIGITS];
        int32 digit_len;
        int32 zeros;

        digit_len = format_integer_digits(digits, groups[i], 10, false);
        zeros = GROUP_DIGITS - digit_len;
        if (len + zeros + digit_len >= capacity) {
            return -EOVERFLOW;
        }
        for (int32 j = 0; j < zeros; j += 1) {
            buffer[len] = '0';
            len += 1;
        }
        memcpy(buffer + len, digits, (size_t)digit_len);
        len += digit_len;
    }

    buffer[len] = '\0';
    return len;
}

static int32 UNUSED
format_binary_float_to_exact_integer(FormatBinaryFloat *parts,
                                     FormatBigUInt *integer) {
    int32 shift;

    ASSERT(parts != NULL);
    ASSERT(integer != NULL);

    *integer = parts->significand;
    if (parts->zero) {
        return 0;
    }

    if (parts->binary_exponent >= 0) {
        return format_big_uint_shift_left(integer, parts->binary_exponent);
    }

    shift = -parts->binary_exponent;
    if (format_big_uint_has_low_bits(integer, shift)) {
        return -ERANGE;
    }
    format_big_uint_shift_right(integer, shift);
    return 0;
}

static int32 UNUSED
format_binary_float_scaled_decimal(FormatBinaryFloat *parts,
                                   int32 decimal_places,
                                   FormatBigUInt *integer,
                                   enum FormatRemainderHalf *remainder) {
    int32 binary_shift;
    int32 status;

    ASSERT(parts != NULL);
    ASSERT_NON_NEGATIVE(decimal_places);
    ASSERT(integer != NULL);
    ASSERT(remainder != NULL);

    *integer = parts->significand;
    *remainder = FORMAT_REMAINDER_ZERO;
    if (parts->zero) {
        return 0;
    }

    for (int32 i = 0; i < decimal_places; i += 1) {
        if ((status = format_big_uint_mul_small(integer, 5)) < 0) {
            return status;
        }
    }

    binary_shift = parts->binary_exponent + decimal_places;
    if (binary_shift >= 0) {
        return format_big_uint_shift_left(integer, binary_shift);
    }

    binary_shift = -binary_shift;
    *remainder = format_big_uint_remainder_half(integer, binary_shift);
    format_big_uint_shift_right(integer, binary_shift);
    return 0;
}

static int32 UNUSED
format_binary_float_set_zero(FormatBinaryFloat *parts, bool negative,
                             int32 precision_bits) {
    ASSERT(parts != NULL);
    ASSERT_POSITIVE(precision_bits);

    format_big_uint_zero(&parts->significand);
    parts->binary_exponent = 0;
    parts->precision_bits = precision_bits;
    parts->negative = negative;
    parts->zero = true;
    return 0;
}

#if FORMAT_LONG_DOUBLE_SUPPORTED

static int32
format_decode_binary64_long_double(ldouble value,
                                   FormatBinaryFloat *parts) {
    uint64 fraction_mask;
    uint64 exponent_bits;
    uint64 fraction;
    uint64 bits;
    bool negative;

    ASSERT(parts != NULL);
    ASSERT(SIZEOF(ldouble) == SIZEOF(double));

    memcpy(&bits, &value, (size_t)SIZEOF(bits));
    negative = (bits >> 63) != 0;
    fraction_mask = (UINT64_C(1)
                     << FORMAT_LONG_DOUBLE_DOUBLE_FRACTION_BITS) - 1;
    exponent_bits = (bits >> FORMAT_LONG_DOUBLE_DOUBLE_FRACTION_BITS)
                    & 0x7ff;
    fraction = bits & fraction_mask;

    if (exponent_bits == 0 && fraction == 0) {
        return format_binary_float_set_zero(parts, negative, DBL_MANT_DIG);
    }
    if (exponent_bits == 0x7ff) {
        return -EINVAL;
    }

    format_big_uint_zero(&parts->significand);
    if (exponent_bits == 0) {
        format_big_uint_from_uint64(&parts->significand, fraction);
        parts->binary_exponent = 1 - FORMAT_LONG_DOUBLE_DOUBLE_EXPONENT_BIAS
                                 - FORMAT_LONG_DOUBLE_DOUBLE_FRACTION_BITS;
    } else {
        format_big_uint_from_uint64(&parts->significand,
                                    (UINT64_C(1)
                                     << FORMAT_LONG_DOUBLE_DOUBLE_FRACTION_BITS)
                                    |fraction);
        parts->binary_exponent = (int32)exponent_bits
                                 - FORMAT_LONG_DOUBLE_DOUBLE_EXPONENT_BIAS
                                 - FORMAT_LONG_DOUBLE_DOUBLE_FRACTION_BITS;
    }
    parts->precision_bits = DBL_MANT_DIG;
    parts->negative = negative;
    parts->zero = false;
    return 0;
}

static int32
format_decode_x87_long_double(ldouble value, FormatBinaryFloat *parts) {
    uchar bytes[SIZEOF(ldouble)];
    uint64 significand;
    uint32 sign_exp;
    uint32 exponent_bits;
    bool negative;

    ASSERT(parts != NULL);

    if (!format_host_is_little_endian() || SIZEOF(ldouble) < 10) {
        return -ENOSYS;
    }

    memcpy(bytes, &value, (size_t)SIZEOF(bytes));
    significand = format_read_le_uint64(bytes);
    sign_exp = (uint32)bytes[8] | ((uint32)bytes[9] << 8);
    negative = (sign_exp & UINT32_C(0x8000)) != 0;
    exponent_bits = sign_exp & FORMAT_LONG_DOUBLE_X87_EXPONENT_MASK;

    if (exponent_bits == 0 && significand == 0) {
        return format_binary_float_set_zero(parts, negative, LDBL_MANT_DIG);
    }
    if (exponent_bits == FORMAT_LONG_DOUBLE_X87_EXPONENT_MASK) {
        return -EINVAL;
    }
    if (exponent_bits != 0
        && (significand & (UINT64_C(1)
                           << FORMAT_LONG_DOUBLE_X87_FRACTION_BITS)) == 0) {
        return -EINVAL;
    }

    format_big_uint_from_uint64(&parts->significand, significand);
    if (exponent_bits == 0) {
        parts->binary_exponent = 1 - FORMAT_LONG_DOUBLE_X87_EXPONENT_BIAS
                                 - FORMAT_LONG_DOUBLE_X87_FRACTION_BITS;
    } else {
        parts->binary_exponent = (int32)exponent_bits
                                 - FORMAT_LONG_DOUBLE_X87_EXPONENT_BIAS
                                 - FORMAT_LONG_DOUBLE_X87_FRACTION_BITS;
    }
    parts->precision_bits = LDBL_MANT_DIG;
    parts->negative = negative;
    parts->zero = false;
    return 0;
}

static int32
format_decode_binary128_long_double(ldouble value,
                                    FormatBinaryFloat *parts) {
    uchar bytes[SIZEOF(ldouble)];
    uint64 fraction_high;
    uint64 exponent_bits;
    uint64 high;
    uint64 low;
    bool negative;

    ASSERT(parts != NULL);

    if (SIZEOF(ldouble) != 16) {
        return -ENOSYS;
    }

    memcpy(bytes, &value, (size_t)SIZEOF(bytes));
    if (format_host_is_little_endian()) {
        low = format_read_le_uint64(bytes);
        high = format_read_le_uint64(bytes + 8);
    } else {
        high = format_read_be_uint64(bytes);
        low = format_read_be_uint64(bytes + 8);
    }

    negative = (high >> 63) != 0;
    exponent_bits = (high >> 48) & 0x7fff;
    fraction_high = high & UINT64_C(0x0000ffffffffffff);

    if (exponent_bits == 0 && fraction_high == 0 && low == 0) {
        return format_binary_float_set_zero(parts, negative, LDBL_MANT_DIG);
    }
    if (exponent_bits == 0x7fff) {
        return -EINVAL;
    }

    format_big_uint_from_uint128_parts(&parts->significand, low,
                                       fraction_high);
    if (exponent_bits != 0) {
        int32 status;

        status = format_big_uint_set_bit(
            &parts->significand,
            FORMAT_LONG_DOUBLE_BINARY128_FRACTION_BITS);
        if (status < 0) {
            return status;
        }
        parts->binary_exponent = (int32)exponent_bits
            - FORMAT_LONG_DOUBLE_BINARY128_EXPONENT_BIAS
            - FORMAT_LONG_DOUBLE_BINARY128_FRACTION_BITS;
    } else {
        parts->binary_exponent = 1
            - FORMAT_LONG_DOUBLE_BINARY128_EXPONENT_BIAS
            - FORMAT_LONG_DOUBLE_BINARY128_FRACTION_BITS;
    }
    parts->precision_bits = LDBL_MANT_DIG;
    parts->negative = negative;
    parts->zero = false;
    return 0;
}

static int32 UNUSED
format_decompose_long_double(ldouble value, FormatBinaryFloat *parts) {
    ASSERT(parts != NULL);

    if (!isfinite(value)) {
        return -EINVAL;
    }
    format_big_uint_zero(&parts->significand);
    parts->binary_exponent = 0;
    parts->precision_bits = 0;
    parts->negative = false;
    parts->zero = false;

#if FLT_RADIX != 2
    return -ENOSYS;
#elif LDBL_MANT_DIG == DBL_MANT_DIG && LDBL_MAX_EXP == DBL_MAX_EXP
    {
        if (SIZEOF(ldouble) != SIZEOF(double)) {
            return -ENOSYS;
        }
        return format_decode_binary64_long_double(value, parts);
    }
#elif LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384
    return format_decode_x87_long_double(value, parts);
#elif LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384
    return format_decode_binary128_long_double(value, parts);
#else
    return -ENOSYS;
#endif
}

#endif

static bool
format_float_is_upper(char conversion) {
    return conversion == 'F' || conversion == 'E' || conversion == 'G'
           || conversion == 'A';
}

static bool
format_float_is_fixed(char conversion) {
    return conversion == 'f' || conversion == 'F';
}

static bool
format_float_is_scientific(char conversion) {
    return conversion == 'e' || conversion == 'E';
}

static bool
format_float_is_general(char conversion) {
    return conversion == 'g' || conversion == 'G';
}

static bool
format_float_is_hex(char conversion) {
    return conversion == 'a' || conversion == 'A';
}

static int32
format_load_float_width_precision(FormatSpec *spec, FormatArgs *args) {
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if ((status = format_load_dynamic_width_precision(spec, args)) < 0) {
        return status;
    }
    if (!format_has_precision(spec)) {
        if (format_float_is_hex(spec->conversion)) {
            spec->precision = -1;
        } else {
            spec->precision = 6;
            spec->precision_kind = FORMAT_PRECISION_LITERAL;
        }
    }
    if (spec->precision > INT32_MAX) {
        return -EOVERFLOW;
    }

    return 0;
}


static int32
format_float_temp_capacity(FormatSpec *spec, int64 *capacity) {
    int64 prefix;

    ASSERT(spec != NULL);
    ASSERT(capacity != NULL);

    if (format_float_is_fixed(spec->conversion)) {
        if (spec->length == FORMAT_LENGTH_BIG_L) {
            prefix = FORMAT_LONG_DOUBLE_MAX_FIXED_PREFIX;
        } else {
            prefix = FORMAT_FLOAT_MAX_FIXED_PREFIX;
        }
    } else if (format_float_is_general(spec->conversion)) {
        prefix = 32;
    } else if (format_float_is_hex(spec->conversion)) {
        prefix = 32;
    } else {
        prefix = FORMAT_FLOAT_MAX_EXP_PREFIX;
    }

    if (spec->precision < 0) {
        *capacity = prefix + FORMAT_DOUBLE_HEX_DIGITS + 8;
        return 0;
    }

    if (spec->precision > INT64_MAX - prefix - 8) {
        return -EOVERFLOW;
    }

    *capacity = prefix + spec->precision + 8;
    if (*capacity > FORMAT_FLOAT_PRINTF_MAX_TEMP_SIZE) {
        return -EOVERFLOW;
    }

    return 0;
}

static char
format_float_sign(double value, FormatSpec *spec) {
    ASSERT(spec != NULL);

    if (signbit(value)) {
        return '-';
    }
    if ((spec->flags & FORMAT_FLAG_SIGN) != 0) {
        return '+';
    }
    if ((spec->flags & FORMAT_FLAG_SPACE) != 0) {
        return ' ';
    }

    return '\0';
}

static int32
format_float_special_body(double value, FormatSpec *spec,
                          char *body, int32 *body_len) {
    ASSERT(spec != NULL);
    ASSERT(body != NULL);
    ASSERT(body_len != NULL);

    if (isnan(value)) {
        if (format_float_is_upper(spec->conversion)) {
            memcpy(body, "NAN", 3);
        } else {
            memcpy(body, "nan", 3);
        }
        *body_len = 3;
        return 0;
    }
    if (isinf(value)) {
        if (format_float_is_upper(spec->conversion)) {
            memcpy(body, "INF", 3);
        } else {
            memcpy(body, "inf", 3);
        }
        *body_len = 3;
        return 0;
    }

    return -EINVAL;
}

static void
format_float_uppercase_body(char *body, int32 body_len) {
    ASSERT(body != NULL);
    ASSERT_NON_NEGATIVE(body_len);

    for (int32 i = 0; i < body_len; i += 1) {
        if (body[i] == 'e') {
            body[i] = 'E';
        }
    }
    return;
}

static int32
format_float_force_decimal_point(char *body, int32 body_len,
                                 int32 capacity) {
    int32 exponent_index;
    bool found_point;

    ASSERT(body != NULL);
    ASSERT_NON_NEGATIVE(body_len);
    ASSERT(body_len < capacity);

    exponent_index = body_len;
    found_point = false;
    for (int32 i = 0; i < body_len; i += 1) {
        if (body[i] == '.') {
            found_point = true;
        } else if (body[i] == 'e' || body[i] == 'E') {
            exponent_index = i;
            break;
        }
    }
    if (found_point) {
        return body_len;
    }
    if (body_len + 1 >= capacity) {
        return -EOVERFLOW;
    }

    memmove(body + exponent_index + 1, body + exponent_index,
            (size_t)(body_len - exponent_index));
    body[exponent_index] = '.';
    return body_len + 1;
}


static int32
format_hex_digit_value(char digit) {
    if (digit >= '0' && digit <= '9') {
        return digit - '0';
    }
    if (digit >= 'a' && digit <= 'f') {
        return digit - 'a' + 10;
    }
    ASSERT(digit >= 'A' && digit <= 'F');
    return digit - 'A' + 10;
}

static char
format_hex_digit_char(int32 digit, bool upper) {
    char lower_digits[] = "0123456789abcdef";
    char upper_digits[] = "0123456789ABCDEF";

    ASSERT_MORE_EQUAL(digit, 0);
    ASSERT_LESS_EQUAL(digit, 15);

    if (upper) {
        return upper_digits[digit];
    }
    return lower_digits[digit];
}

static void
format_float_hex_fraction_digits(uint64 fraction, char *digits, bool upper) {
    ASSERT(digits != NULL);

    for (int32 i = 0; i < FORMAT_DOUBLE_HEX_DIGITS; i += 1) {
        int32 shift;
        int32 digit;

        shift = FORMAT_DOUBLE_FRACTION_BITS - 4*(i + 1);
        digit = (int32)((fraction >> shift) & 0xf);
        digits[i] = format_hex_digit_char(digit, upper);
    }
    return;
}

static bool
format_float_hex_has_nonzero_tail(char *digits, int32 start) {
    ASSERT(digits != NULL);
    ASSERT_MORE_EQUAL(start, 0);
    ASSERT_LESS_EQUAL(start, FORMAT_DOUBLE_HEX_DIGITS);

    for (int32 i = start; i < FORMAT_DOUBLE_HEX_DIGITS; i += 1) {
        if (format_hex_digit_value(digits[i]) != 0) {
            return true;
        }
    }
    return false;
}

static bool
format_float_hex_should_round(char first_digit, char *digits,
                              int32 precision) {
    int32 round_digit;
    int32 even_digit;

    ASSERT(digits != NULL);
    ASSERT_MORE_EQUAL(precision, 0);
    ASSERT_LESS(precision, FORMAT_DOUBLE_HEX_DIGITS);

    round_digit = format_hex_digit_value(digits[precision]);
    if (round_digit > 8) {
        return true;
    }
    if (round_digit < 8) {
        return false;
    }
    if (format_float_hex_has_nonzero_tail(digits, precision + 1)) {
        return true;
    }

    if (precision > 0) {
        even_digit = format_hex_digit_value(digits[precision - 1]);
    } else {
        even_digit = format_hex_digit_value(first_digit);
    }
    return (even_digit & 1) != 0;
}

static void
format_float_hex_round(char *first_digit, char *digits, int32 precision,
                       bool upper) {
    ASSERT(first_digit != NULL);
    ASSERT(digits != NULL);
    ASSERT_MORE_EQUAL(precision, 0);
    ASSERT_LESS(precision, FORMAT_DOUBLE_HEX_DIGITS);

    if (!format_float_hex_should_round(*first_digit, digits, precision)) {
        return;
    }

    for (int32 i = precision - 1; i >= 0; i -= 1) {
        int32 digit;

        digit = format_hex_digit_value(digits[i]);
        if (digit < 15) {
            digits[i] = format_hex_digit_char(digit + 1, upper);
            return;
        }
        digits[i] = '0';
    }

    *first_digit = format_hex_digit_char(
        format_hex_digit_value(*first_digit) + 1, upper);
    return;
}

static int32
format_float_hex_trim_digits(char *digits) {
    int32 len;

    ASSERT(digits != NULL);

    len = FORMAT_DOUBLE_HEX_DIGITS;
    while (len > 0 && digits[len - 1] == '0') {
        len -= 1;
    }
    return len;
}

static int32
format_buffer_put(char *buffer, int32 capacity, int32 len, char byte) {
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT_NON_NEGATIVE(len);

    if (len >= capacity) {
        return -EOVERFLOW;
    }
    buffer[len] = byte;
    return len + 1;
}

static int32
format_buffer_write(char *buffer, int32 capacity, int32 len,
                    char *source, int32 source_len) {
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT_NON_NEGATIVE(len);
    ASSERT(source != NULL);
    ASSERT_NON_NEGATIVE(source_len);

    if (source_len > capacity - len) {
        return -EOVERFLOW;
    }
    memcpy(buffer + len, source, (size_t)source_len);
    return len + source_len;
}

static int32
format_float_hex_append_exponent(char *buffer, int32 capacity, int32 len,
                                 int32 exponent, bool upper) {
    char digits[16];
    uint64 magnitude;
    int32 digit_len;
    int32 status;

    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT_NON_NEGATIVE(len);

    if (upper) {
        if ((status = format_buffer_put(buffer, capacity, len, 'P')) < 0) {
            return status;
        }
    } else {
        if ((status = format_buffer_put(buffer, capacity, len, 'p')) < 0) {
            return status;
        }
    }
    len = status;

    if (exponent < 0) {
        magnitude = (uint64)(-(int64)exponent);
        if ((status = format_buffer_put(buffer, capacity, len, '-')) < 0) {
            return status;
        }
    } else {
        magnitude = (uint64)exponent;
        if ((status = format_buffer_put(buffer, capacity, len, '+')) < 0) {
            return status;
        }
    }
    len = status;

    digit_len = format_integer_digits(digits, magnitude, 10, false);
    return format_buffer_write(buffer, capacity, len, digits, digit_len);
}

static int32 UNUSED
format_float_decimal_append_exponent(char *buffer, int32 capacity,
                                     int32 len, int32 exponent,
                                     bool upper) {
    char digits[16];
    uint64 magnitude;
    int32 digit_len;
    int32 status;

    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT_NON_NEGATIVE(len);

    if (upper) {
        if ((status = format_buffer_put(buffer, capacity, len, 'E')) < 0) {
            return status;
        }
    } else {
        if ((status = format_buffer_put(buffer, capacity, len, 'e')) < 0) {
            return status;
        }
    }
    len = status;

    if (exponent < 0) {
        magnitude = (uint64)(-(int64)exponent);
        if ((status = format_buffer_put(buffer, capacity, len, '-')) < 0) {
            return status;
        }
    } else {
        magnitude = (uint64)exponent;
        if ((status = format_buffer_put(buffer, capacity, len, '+')) < 0) {
            return status;
        }
    }
    len = status;

    digit_len = format_integer_digits(digits, magnitude, 10, false);
    if (digit_len < 2) {
        if ((status = format_buffer_put(buffer, capacity, len, '0')) < 0) {
            return status;
        }
        len = status;
    }

    return format_buffer_write(buffer, capacity, len, digits, digit_len);
}

static bool UNUSED
format_remainder_should_round(enum FormatRemainderHalf remainder,
                              bool odd) {
    if (remainder == FORMAT_REMAINDER_MORE_HALF) {
        return true;
    }
    if (remainder == FORMAT_REMAINDER_HALF && odd) {
        return true;
    }
    return false;
}

static bool UNUSED
format_big_uint_is_odd(FormatBigUInt *value) {
    ASSERT(value != NULL);

    return value->len > 0 && (value->words[0] & 1) != 0;
}

#if FORMAT_LONG_DOUBLE_SUPPORTED

static int32
format_big_uint_round_half_even(FormatBigUInt *value,
                                enum FormatRemainderHalf remainder) {
    ASSERT(value != NULL);

    if (format_remainder_should_round(remainder,
                                      format_big_uint_is_odd(value))) {
        return format_big_uint_add_one(value);
    }
    return 0;
}

static int32
format_long_double_special_body(ldouble value, FormatSpec *spec,
                                char *body, int32 *body_len) {
    ASSERT(spec != NULL);
    ASSERT(body != NULL);
    ASSERT(body_len != NULL);

    if (isnan(value)) {
        if (format_float_is_upper(spec->conversion)) {
            memcpy(body, "NAN", 3);
        } else {
            memcpy(body, "nan", 3);
        }
        *body_len = 3;
        return 0;
    }
    if (isinf(value)) {
        if (format_float_is_upper(spec->conversion)) {
            memcpy(body, "INF", 3);
        } else {
            memcpy(body, "inf", 3);
        }
        *body_len = 3;
        return 0;
    }

    return -EINVAL;
}

static char
format_long_double_sign(ldouble value, FormatSpec *spec) {
    ASSERT(spec != NULL);

    if (signbit(value)) {
        return '-';
    }
    if ((spec->flags & FORMAT_FLAG_SIGN) != 0) {
        return '+';
    }
    if ((spec->flags & FORMAT_FLAG_SPACE) != 0) {
        return ' ';
    }

    return '\0';
}

static int32
format_long_double_write_fixed_digits(char *buffer, int32 capacity,
                                      char *digits, int32 digit_len,
                                      int32 precision, bool alternate) {
    int32 integer_len;
    int32 len;
    int32 zeros;
    int32 status;

    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT(digits != NULL);
    ASSERT_POSITIVE(digit_len);
    ASSERT_NON_NEGATIVE(precision);

    len = 0;
    if (precision == 0) {
        if ((status = format_buffer_write(buffer, capacity, len,
                                          digits, digit_len)) < 0) {
            return status;
        }
        len = status;
        if (alternate) {
            return format_buffer_put(buffer, capacity, len, '.');
        }
        return len;
    }

    integer_len = digit_len - precision;
    if (integer_len > 0) {
        if ((status = format_buffer_write(buffer, capacity, len,
                                          digits, integer_len)) < 0) {
            return status;
        }
        len = status;
    } else {
        if ((status = format_buffer_put(buffer, capacity, len, '0')) < 0) {
            return status;
        }
        len = status;
    }

    if ((status = format_buffer_put(buffer, capacity, len, '.')) < 0) {
        return status;
    }
    len = status;

    zeros = 0;
    if (integer_len < 0) {
        zeros = -integer_len;
    }
    for (int32 i = 0; i < zeros; i += 1) {
        if ((status = format_buffer_put(buffer, capacity, len, '0')) < 0) {
            return status;
        }
        len = status;
    }

    if (integer_len < 0) {
        return format_buffer_write(buffer, capacity, len, digits, digit_len);
    }
    return format_buffer_write(buffer, capacity, len,
                               digits + integer_len,
                               digit_len - integer_len);
}

static int32
format_long_double_generate_fixed_body(FormatSpec *spec, ldouble value,
                                       char *buffer, int32 capacity) {
    enum FormatRemainderHalf remainder;
    FormatBinaryFloat parts;
    FormatBigUInt integer;
    char *digits;
    char stack_digits[FORMAT_FLOAT_PRINTF_STACK_BUFFER_SIZE];
    int32 digit_capacity;
    int32 digit_len;
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT(format_float_is_fixed(spec->conversion));
    ASSERT(spec->precision >= 0);

    if ((status = format_decompose_long_double(value, &parts)) < 0) {
        return status;
    }
    if ((status = format_binary_float_scaled_decimal(&parts,
                                                     (int32)spec->precision,
                                                     &integer,
                                                     &remainder)) < 0) {
        return status;
    }
    if ((status = format_big_uint_round_half_even(&integer, remainder)) < 0) {
        return status;
    }

    digit_capacity = capacity;
    if (digit_capacity <= SIZEOF(stack_digits)) {
        digits = stack_digits;
    } else {
        digits = malloc2(digit_capacity);
    }

    digit_len = format_big_uint_to_decimal(&integer, digits, digit_capacity);
    if (digit_len >= 0) {
        status = format_long_double_write_fixed_digits(
            buffer, capacity, digits, digit_len, (int32)spec->precision,
            (spec->flags & FORMAT_FLAG_ALTERNATE) != 0);
    } else {
        status = digit_len;
    }

    if (digits != stack_digits) {
        free2(digits, digit_capacity);
    }
    return status;
}

static bool
format_decimal_digits_have_nonzero_tail(char *digits, int32 start,
                                        int32 len) {
    ASSERT(digits != NULL);
    ASSERT_MORE_EQUAL(start, 0);
    ASSERT_LESS_EQUAL(start, len);

    for (int32 i = start; i < len; i += 1) {
        if (digits[i] != '0') {
            return true;
        }
    }
    return false;
}

static bool
format_decimal_fraction_nonzero(enum FormatRemainderHalf remainder) {
    return remainder != FORMAT_REMAINDER_ZERO;
}

static int32
format_decimal_round_digits(char *digits, int32 *len, int32 keep,
                            bool round_up) {
    ASSERT(digits != NULL);
    ASSERT(len != NULL);
    ASSERT_POSITIVE(*len);
    ASSERT_POSITIVE(keep);
    ASSERT_LESS_EQUAL(keep, *len);

    *len = keep;
    if (!round_up) {
        return 0;
    }

    for (int32 i = keep - 1; i >= 0; i -= 1) {
        if (digits[i] < '9') {
            digits[i] += 1;
            return 0;
        }
        digits[i] = '0';
    }

    if (keep >= INT32_MAX) {
        return -EOVERFLOW;
    }
    memmove(digits + 1, digits, (size_t)keep);
    digits[0] = '1';
    *len = keep + 1;
    return 0;
}

static int32
format_long_double_scientific_exponent_small(FormatBinaryFloat *parts,
                                             ldouble value,
                                             int32 *exponent) {
    FormatBigUInt integer;
    enum FormatRemainderHalf remainder;
    ldouble estimate_float;
    int32 estimate;
    int32 digit_len;
    int32 status;
    char digits[16];

    ASSERT(parts != NULL);
    ASSERT(exponent != NULL);
    ASSERT(value > 0.0L && value < 1.0L);

    estimate_float = floorl(log10l(value));
    if (estimate_float < INT32_MIN || estimate_float > INT32_MAX) {
        return -EOVERFLOW;
    }
    estimate = (int32)estimate_float;
    for (int32 i = 0; i < 16; i += 1) {
        if (estimate >= 0) {
            return -EINVAL;
        }
        if ((status = format_binary_float_scaled_decimal(parts, -estimate,
                                                         &integer,
                                                         &remainder)) < 0) {
            return status;
        }
        digit_len = format_big_uint_to_decimal(&integer, digits,
                                               SIZEOF(digits));
        if (digit_len < 0) {
            return digit_len;
        }
        if (digit_len == 1 && digits[0] == '0') {
            estimate -= 1;
        } else if (digit_len > 1) {
            estimate += 1;
        } else {
            *exponent = estimate;
            return 0;
        }
    }

    return -ERANGE;
}

static int32
format_long_double_scientific_exponent(FormatBinaryFloat *parts,
                                       ldouble value,
                                       int32 *exponent) {
    FormatBigUInt integer;
    enum FormatRemainderHalf remainder;
    char digits[FORMAT_LONG_DOUBLE_MAX_FIXED_PREFIX];
    int32 digit_len;
    int32 status;

    ASSERT(parts != NULL);
    ASSERT(exponent != NULL);
    ASSERT(value >= 0.0L);

    if (parts->zero) {
        *exponent = 0;
        return 0;
    }
    if (value < 1.0L) {
        return format_long_double_scientific_exponent_small(parts, value,
                                                            exponent);
    }

    if ((status = format_binary_float_scaled_decimal(parts, 0, &integer,
                                                     &remainder)) < 0) {
        return status;
    }
    digit_len = format_big_uint_to_decimal(&integer, digits, SIZEOF(digits));
    if (digit_len < 0) {
        return digit_len;
    }
    *exponent = digit_len - 1;
    return 0;
}

static int32
format_long_double_scientific_digits(FormatBinaryFloat *parts, ldouble value,
                                     int32 exponent, int32 significant_len,
                                     char *digits, int32 capacity,
                                     int32 *digits_len,
                                     int32 *decimal_exponent) {
    FormatBigUInt integer;
    enum FormatRemainderHalf remainder;
    int32 decimal_places;
    int32 integer_digit_len;
    int32 status;

    ASSERT(parts != NULL);
    ASSERT_NON_NEGATIVE(significant_len);
    ASSERT(digits != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT(digits_len != NULL);
    ASSERT(decimal_exponent != NULL);

    *decimal_exponent = exponent;
    if (parts->zero) {
        digits[0] = '0';
        *digits_len = 1;
        return 0;
    }

    if (value >= 1.0L) {
        if ((status = format_binary_float_scaled_decimal(parts, 0,
                                                         &integer,
                                                         &remainder)) < 0) {
            return status;
        }
        integer_digit_len = format_big_uint_to_decimal(&integer, digits,
                                                       capacity);
        if (integer_digit_len < 0) {
            return integer_digit_len;
        }
        if (integer_digit_len > significant_len) {
            int32 round_digit;
            bool tail_nonzero;
            bool round_up;

            round_digit = digits[significant_len] - '0';
            tail_nonzero = format_decimal_digits_have_nonzero_tail(
                digits, significant_len + 1, integer_digit_len);
            if (format_decimal_fraction_nonzero(remainder)) {
                tail_nonzero = true;
            }
            if (round_digit > 5) {
                round_up = true;
            } else if (round_digit < 5) {
                round_up = false;
            } else if (tail_nonzero) {
                round_up = true;
            } else {
                round_up = ((digits[significant_len - 1] - '0') & 1) != 0;
            }
            status = format_decimal_round_digits(digits, &integer_digit_len,
                                                 significant_len, round_up);
            if (status < 0) {
                return status;
            }
            if (integer_digit_len > significant_len) {
                *decimal_exponent += 1;
                integer_digit_len = significant_len;
            }
            *digits_len = integer_digit_len;
            return 0;
        }
        if (integer_digit_len == significant_len) {
            bool round_up;

            round_up = format_remainder_should_round(
                remainder, ((digits[integer_digit_len - 1] - '0') & 1) != 0);
            status = format_decimal_round_digits(digits, &integer_digit_len,
                                                 significant_len, round_up);
            if (status < 0) {
                return status;
            }
            if (integer_digit_len > significant_len) {
                *decimal_exponent += 1;
                integer_digit_len = significant_len;
            }
            *digits_len = integer_digit_len;
            return 0;
        }

        decimal_places = significant_len - integer_digit_len;
    } else {
        decimal_places = significant_len - 1 - exponent;
    }

    if (decimal_places < 0) {
        return -EINVAL;
    }
    if ((status = format_binary_float_scaled_decimal(parts, decimal_places,
                                                     &integer,
                                                     &remainder)) < 0) {
        return status;
    }
    if ((status = format_big_uint_round_half_even(&integer, remainder)) < 0) {
        return status;
    }

    integer_digit_len = format_big_uint_to_decimal(&integer, digits, capacity);
    if (integer_digit_len < 0) {
        return integer_digit_len;
    }
    while (integer_digit_len < significant_len) {
        memmove(digits + 1, digits, (size_t)integer_digit_len);
        digits[0] = '0';
        integer_digit_len += 1;
    }
    if (integer_digit_len > significant_len) {
        *decimal_exponent += 1;
        integer_digit_len = significant_len;
    }
    *digits_len = integer_digit_len;
    return 0;
}

static int32
format_long_double_write_scientific_digits(char *buffer, int32 capacity,
                                           char *digits, int32 digit_len,
                                           int32 precision, int32 exponent,
                                           bool alternate, bool upper) {
    int32 len;
    int32 status;

    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT(digits != NULL);
    ASSERT_POSITIVE(digit_len);
    ASSERT_NON_NEGATIVE(precision);

    len = 0;
    if ((status = format_buffer_put(buffer, capacity, len, digits[0])) < 0) {
        return status;
    }
    len = status;

    if (precision > 0 || alternate) {
        if ((status = format_buffer_put(buffer, capacity, len, '.')) < 0) {
            return status;
        }
        len = status;
    }
    for (int32 i = 0; i < precision; i += 1) {
        char digit;

        if (i + 1 < digit_len) {
            digit = digits[i + 1];
        } else {
            digit = '0';
        }
        if ((status = format_buffer_put(buffer, capacity, len, digit)) < 0) {
            return status;
        }
        len = status;
    }

    return format_float_decimal_append_exponent(buffer, capacity, len,
                                                exponent, upper);
}

static int32
format_long_double_generate_scientific_body(FormatSpec *spec, ldouble value,
                                            char *buffer, int32 capacity) {
    FormatBinaryFloat parts;
    char *digits;
    char stack_digits[FORMAT_FLOAT_PRINTF_STACK_BUFFER_SIZE];
    int32 significant_len;
    int32 decimal_exponent;
    int32 digit_capacity;
    int32 exponent;
    int32 digit_len;
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT(format_float_is_scientific(spec->conversion));
    ASSERT(spec->precision >= 0);

    if ((status = format_decompose_long_double(value, &parts)) < 0) {
        return status;
    }
    if ((status = format_long_double_scientific_exponent(&parts,
                                                         fabsl(value),
                                                         &exponent)) < 0) {
        return status;
    }

    significant_len = (int32)spec->precision + 1;
    digit_capacity = FORMAT_LONG_DOUBLE_MAX_FIXED_PREFIX + significant_len;
    if (digit_capacity < SIZEOF(stack_digits)) {
        digit_capacity = SIZEOF(stack_digits);
    }
    if (digit_capacity > FORMAT_FLOAT_PRINTF_MAX_TEMP_SIZE) {
        return -EOVERFLOW;
    }
    if (digit_capacity <= SIZEOF(stack_digits)) {
        digits = stack_digits;
    } else {
        digits = malloc2(digit_capacity);
    }

    status = format_long_double_scientific_digits(&parts, fabsl(value),
                                                  exponent, significant_len,
                                                  digits, digit_capacity,
                                                  &digit_len,
                                                  &decimal_exponent);
    if (status == 0) {
        status = format_long_double_write_scientific_digits(
            buffer, capacity, digits, digit_len, (int32)spec->precision,
            decimal_exponent, (spec->flags & FORMAT_FLAG_ALTERNATE) != 0,
            format_float_is_upper(spec->conversion));
    }

    if (digits != stack_digits) {
        free2(digits, digit_capacity);
    }
    return status;
}


static int32
format_long_double_generate_body(FormatSpec *spec, ldouble value,
                                 char *buffer, int32 capacity);

static int32
format_long_double_parse_decimal_exponent(char *body, int32 body_len,
                                          int32 *exponent) {
    int32 index;
    int64 value;
    int32 sign;

    ASSERT(body != NULL);
    ASSERT_NON_NEGATIVE(body_len);
    ASSERT(exponent != NULL);

    index = 0;
    while (index < body_len && body[index] != 'e'
           && body[index] != 'E') {
        index += 1;
    }
    if (index >= body_len) {
        return -EINVAL;
    }

    index += 1;
    if (index >= body_len) {
        return -EINVAL;
    }

    sign = 1;
    if (body[index] == '-') {
        sign = -1;
        index += 1;
    } else if (body[index] == '+') {
        index += 1;
    }
    if (index >= body_len || !format_is_digit(body[index])) {
        return -EINVAL;
    }

    value = 0;
    while (index < body_len) {
        int32 digit;

        if (!format_is_digit(body[index])) {
            return -EINVAL;
        }
        digit = format_digit_value(body[index]);
        if (value > (INT32_MAX - digit)/10) {
            return -EOVERFLOW;
        }
        value = value*10 + digit;
        index += 1;
    }

    if (sign < 0) {
        value = -value;
    }
    *exponent = (int32)value;
    return 0;
}

static int32
format_long_double_strip_trailing_zeros(char *body, int32 body_len) {
    int32 exponent_index;
    int32 point_index;
    int32 end;

    ASSERT(body != NULL);
    ASSERT_NON_NEGATIVE(body_len);

    exponent_index = body_len;
    point_index = -1;
    for (int32 i = 0; i < body_len; i += 1) {
        if (body[i] == '.') {
            point_index = i;
        } else if (body[i] == 'e' || body[i] == 'E') {
            exponent_index = i;
            break;
        }
    }
    if (point_index < 0) {
        return body_len;
    }

    end = exponent_index;
    while (end > point_index + 1 && body[end - 1] == '0') {
        end -= 1;
    }
    if (end == point_index + 1) {
        end = point_index;
    }

    if (exponent_index < body_len) {
        memmove(body + end, body + exponent_index,
                (size_t)(body_len - exponent_index));
        end += body_len - exponent_index;
    }

    return end;
}

static int32
format_long_double_generate_general_body(FormatSpec *spec, ldouble value,
                                         char *buffer, int32 capacity) {
    FormatSpec work_spec;
    int32 exponent;
    int32 body_len;
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT(format_float_is_general(spec->conversion));
    ASSERT(spec->precision >= 1);

    work_spec = *spec;
    if (format_float_is_upper(spec->conversion)) {
        work_spec.conversion = 'E';
    } else {
        work_spec.conversion = 'e';
    }
    work_spec.precision = spec->precision - 1;
    body_len = format_long_double_generate_body(&work_spec, value, buffer,
                                                capacity);
    if (body_len < 0) {
        return body_len;
    }

    if ((status = format_long_double_parse_decimal_exponent(buffer,
                                                            body_len,
                                                            &exponent)) < 0) {
        return status;
    }

    if (exponent >= -4 && exponent < spec->precision) {
        work_spec = *spec;
        if (format_float_is_upper(spec->conversion)) {
            work_spec.conversion = 'F';
        } else {
            work_spec.conversion = 'f';
        }
        work_spec.precision = spec->precision - (exponent + 1);
        body_len = format_long_double_generate_body(&work_spec, value,
                                                    buffer, capacity);
        if (body_len < 0) {
            return body_len;
        }
    }

    if ((spec->flags & FORMAT_FLAG_ALTERNATE) == 0) {
        body_len = format_long_double_strip_trailing_zeros(buffer,
                                                          body_len);
    }

    return body_len;
}

static int32
format_long_double_hex_digit_count(FormatBinaryFloat *parts) {
    int32 fraction_bits;

    ASSERT(parts != NULL);
    ASSERT_POSITIVE(parts->precision_bits);

    fraction_bits = parts->precision_bits - 1;
    return (fraction_bits + 3)/4;
}

static void
format_long_double_hex_fraction_digits(FormatBinaryFloat *parts,
                                       char *digits, int32 digit_len,
                                       bool upper) {
    int32 fraction_bits;

    ASSERT(parts != NULL);
    ASSERT(digits != NULL);
    ASSERT_NON_NEGATIVE(digit_len);

    fraction_bits = parts->precision_bits - 1;
    for (int32 i = 0; i < digit_len; i += 1) {
        int32 digit;

        digit = 0;
        for (int32 j = 0; j < 4; j += 1) {
            int32 bit_index;

            bit_index = fraction_bits - 1 - i*4 - j;
            if (bit_index >= 0
                && format_big_uint_test_bit(&parts->significand,
                                            bit_index)) {
                digit |= 1 << (3 - j);
            }
        }
        digits[i] = format_hex_digit_char(digit, upper);
    }
    return;
}

static bool
format_long_double_hex_has_nonzero_tail(char *digits, int32 start,
                                        int32 digit_len) {
    ASSERT(digits != NULL);
    ASSERT_MORE_EQUAL(start, 0);
    ASSERT_LESS_EQUAL(start, digit_len);

    for (int32 i = start; i < digit_len; i += 1) {
        if (format_hex_digit_value(digits[i]) != 0) {
            return true;
        }
    }
    return false;
}

static bool
format_long_double_hex_should_round(char first_digit, char *digits,
                                    int32 digit_len, int32 precision) {
    int32 round_digit;
    int32 even_digit;

    ASSERT(digits != NULL);
    ASSERT_NON_NEGATIVE(digit_len);
    ASSERT_MORE_EQUAL(precision, 0);
    ASSERT_LESS(precision, digit_len);

    round_digit = format_hex_digit_value(digits[precision]);
    if (round_digit > 8) {
        return true;
    }
    if (round_digit < 8) {
        return false;
    }
    if (format_long_double_hex_has_nonzero_tail(digits,
                                                precision + 1,
                                                digit_len)) {
        return true;
    }

    if (precision > 0) {
        even_digit = format_hex_digit_value(digits[precision - 1]);
    } else {
        even_digit = format_hex_digit_value(first_digit);
    }
    return (even_digit & 1) != 0;
}

static void
format_long_double_hex_round(char *first_digit, char *digits,
                             int32 digit_len, int32 precision,
                             bool upper) {
    ASSERT(first_digit != NULL);
    ASSERT(digits != NULL);
    ASSERT_NON_NEGATIVE(digit_len);
    ASSERT_MORE_EQUAL(precision, 0);
    ASSERT_LESS(precision, digit_len);

    if (!format_long_double_hex_should_round(*first_digit, digits,
                                             digit_len, precision)) {
        return;
    }

    for (int32 i = precision - 1; i >= 0; i -= 1) {
        int32 digit;

        digit = format_hex_digit_value(digits[i]);
        if (digit < 15) {
            digits[i] = format_hex_digit_char(digit + 1, upper);
            return;
        }
        digits[i] = '0';
    }

    *first_digit = format_hex_digit_char(
        format_hex_digit_value(*first_digit) + 1, upper);
    return;
}

static int32
format_long_double_hex_trim_digits(char *digits, int32 digit_len) {
    ASSERT(digits != NULL);
    ASSERT_NON_NEGATIVE(digit_len);

    while (digit_len > 0 && digits[digit_len - 1] == '0') {
        digit_len -= 1;
    }
    return digit_len;
}

static int32
format_long_double_write_hex_body(FormatSpec *spec, char *buffer,
                                  int32 capacity, char first_digit,
                                  char *digits, int32 digit_len,
                                  int32 exponent) {
    bool upper;
    int32 len;
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT(digits != NULL);
    ASSERT_NON_NEGATIVE(digit_len);

    upper = format_float_is_upper(spec->conversion);
    len = 0;
    if (upper) {
        if ((status = format_buffer_write(buffer, capacity, len,
                                          "0X", 2)) < 0) {
            return status;
        }
    } else {
        if ((status = format_buffer_write(buffer, capacity, len,
                                          "0x", 2)) < 0) {
            return status;
        }
    }
    len = status;

    if ((status = format_buffer_put(buffer, capacity, len,
                                    first_digit)) < 0) {
        return status;
    }
    len = status;

    if (digit_len > 0 || (spec->flags & FORMAT_FLAG_ALTERNATE) != 0) {
        if ((status = format_buffer_put(buffer, capacity, len, '.')) < 0) {
            return status;
        }
        len = status;
    }

    for (int32 i = 0; i < digit_len; i += 1) {
        if ((status = format_buffer_put(buffer, capacity, len,
                                        digits[i])) < 0) {
            return status;
        }
        len = status;
    }

    return format_float_hex_append_exponent(buffer, capacity, len,
                                            exponent, upper);
}

static int32
format_long_double_generate_hex_body(FormatSpec *spec, ldouble value,
                                     char *buffer, int32 capacity) {
    FormatBinaryFloat parts;
    char stack_digits[64];
    char *digits;
    char first_digit;
    int32 available_digits;
    int32 digit_capacity;
    int32 digit_len;
    int32 exponent;
    int32 status;
    bool upper;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT(format_float_is_hex(spec->conversion));

    upper = format_float_is_upper(spec->conversion);
    if ((status = format_decompose_long_double(value, &parts)) < 0) {
        return status;
    }

    available_digits = format_long_double_hex_digit_count(&parts);
    digit_capacity = available_digits;
    if (spec->precision > digit_capacity) {
        digit_capacity = (int32)spec->precision;
    }
    if (digit_capacity < 1) {
        digit_capacity = 1;
    }
    if (digit_capacity > FORMAT_FLOAT_PRINTF_MAX_TEMP_SIZE) {
        return -EOVERFLOW;
    }

    if (digit_capacity <= SIZEOF(stack_digits)) {
        digits = stack_digits;
    } else {
        digits = malloc2(digit_capacity);
    }
    memset(digits, '0', (size_t)digit_capacity);

    if (parts.zero) {
        first_digit = '0';
        exponent = 0;
    } else {
        exponent = parts.binary_exponent + parts.precision_bits - 1;
        if (format_big_uint_test_bit(&parts.significand,
                                     parts.precision_bits - 1)) {
            first_digit = '1';
        } else {
            first_digit = '0';
        }
        format_long_double_hex_fraction_digits(&parts, digits,
                                               available_digits, upper);
    }

    if (spec->precision < 0) {
        digit_len = format_long_double_hex_trim_digits(digits,
                                                       available_digits);
    } else {
        if (spec->precision < available_digits) {
            format_long_double_hex_round(&first_digit, digits,
                                         available_digits,
                                         (int32)spec->precision, upper);
        }
        for (int32 i = available_digits; i < spec->precision; i += 1) {
            digits[i] = '0';
        }
        digit_len = (int32)spec->precision;
    }

    status = format_long_double_write_hex_body(spec, buffer, capacity,
                                               first_digit, digits,
                                               digit_len, exponent);
    if (digits != stack_digits) {
        free2(digits, digit_capacity);
    }
    return status;
}

static int32
format_long_double_generate_body(FormatSpec *spec, ldouble value,
                                 char *buffer, int32 capacity) {
    int32 body_len;
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);

    if (isnan(value) || isinf(value)) {
        status = format_long_double_special_body(value, spec, buffer,
                                                 &body_len);
        if (status < 0) {
            return status;
        }
        return body_len;
    }

    if (format_float_is_general(spec->conversion)) {
        return format_long_double_generate_general_body(spec, value, buffer,
                                                        capacity);
    }
    if (format_float_is_hex(spec->conversion)) {
        return format_long_double_generate_hex_body(spec, value, buffer,
                                                    capacity);
    }
    if (format_float_is_fixed(spec->conversion)) {
        return format_long_double_generate_fixed_body(spec, value, buffer,
                                                      capacity);
    }
    if (format_float_is_scientific(spec->conversion)) {
        return format_long_double_generate_scientific_body(spec, value,
                                                           buffer,
                                                           capacity);
    }

    return -ENOSYS;
}

#else

static int32 UNUSED
format_decompose_long_double(ldouble value, FormatBinaryFloat *parts) {
    ASSERT(parts != NULL);

    (void)value;
    return -ENOSYS;
}

static char
format_long_double_sign(ldouble value, FormatSpec *spec) {
    ASSERT(spec != NULL);

    (void)value;
    if ((spec->flags & FORMAT_FLAG_SIGN) != 0) {
        return '+';
    }
    if ((spec->flags & FORMAT_FLAG_SPACE) != 0) {
        return ' ';
    }
    return '\0';
}

static int32
format_long_double_generate_body(FormatSpec *spec, ldouble value,
                                 char *buffer, int32 capacity) {
    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);

    (void)value;
    return -ENOSYS;
}

#endif

static void
format_write_float_sign(FormatSink *sink, FormatSpec *spec, char sign,
                        char *body, int32 body_len) {
    int64 zero_pad;
    int64 inner_len;
    int64 spaces;
    int32 prefix_len;

    ASSERT(sink != NULL);
    ASSERT(spec != NULL);
    ASSERT(body != NULL);
    ASSERT_NON_NEGATIVE(body_len);

    inner_len = body_len;
    if (sign != '\0') {
        inner_len += 1;
    }

    zero_pad = 0;
    if ((spec->flags & FORMAT_FLAG_ZERO) != 0
        && (spec->flags & FORMAT_FLAG_LEFT) == 0) {
        zero_pad = format_pad_len(spec->width, inner_len);
    }

    prefix_len = 0;
    if (format_float_is_hex(spec->conversion) && body_len >= 2
        && body[0] == '0' && (body[1] == 'x' || body[1] == 'X')) {
        prefix_len = 2;
    }

    spaces = format_pad_len(spec->width, inner_len + zero_pad);
    if ((spec->flags & FORMAT_FLAG_LEFT) == 0) {
        format_sink_write_repeat(sink, ' ', spaces);
    }
    if (sign != '\0') {
        format_sink_write_byte(sink, sign);
    }
    if (prefix_len > 0) {
        format_sink_write(sink, body, prefix_len);
    }
    format_sink_write_repeat(sink, '0', zero_pad);
    format_sink_write(sink, body + prefix_len, body_len - prefix_len);
    if ((spec->flags & FORMAT_FLAG_LEFT) != 0) {
        format_sink_write_repeat(sink, ' ', spaces);
    }
    return;
}

static int32
format_float_generate_hex_body(FormatSpec *spec, double value,
                               char *buffer, int32 capacity) {
    char digits[FORMAT_DOUBLE_HEX_DIGITS];
    uint64 fraction_mask;
    uint64 exponent_bits;
    uint64 fraction;
    uint64 bits;
    char first_digit;
    int32 exponent;
    int32 digit_len;
    int32 len;
    bool upper;
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT(format_float_is_hex(spec->conversion));

    upper = format_float_is_upper(spec->conversion);
    memcpy(&bits, &value, (size_t)SIZEOF(bits));
    fraction_mask = (UINT64_C(1) << FORMAT_DOUBLE_FRACTION_BITS) - 1;
    exponent_bits = (bits >> FORMAT_DOUBLE_FRACTION_BITS) & 0x7ff;
    fraction = bits & fraction_mask;

    if (exponent_bits == 0 && fraction == 0) {
        first_digit = '0';
        exponent = 0;
    } else if (exponent_bits == 0) {
        first_digit = '0';
        exponent = FORMAT_DOUBLE_SUBNORMAL_EXPONENT;
    } else {
        first_digit = '1';
        exponent = (int32)exponent_bits - FORMAT_DOUBLE_EXPONENT_BIAS;
    }

    format_float_hex_fraction_digits(fraction, digits, upper);
    if (spec->precision < 0) {
        digit_len = format_float_hex_trim_digits(digits);
    } else {
        ASSERT(spec->precision <= INT32_MAX);
        if (spec->precision < FORMAT_DOUBLE_HEX_DIGITS) {
            format_float_hex_round(&first_digit, digits,
                                   (int32)spec->precision, upper);
        }
        digit_len = (int32)spec->precision;
    }

    len = 0;
    if (upper) {
        if ((status = format_buffer_write(buffer, capacity, len,
                                          "0X", 2)) < 0) {
            return status;
        }
    } else {
        if ((status = format_buffer_write(buffer, capacity, len,
                                          "0x", 2)) < 0) {
            return status;
        }
    }
    len = status;

    if ((status = format_buffer_put(buffer, capacity, len,
                                    first_digit)) < 0) {
        return status;
    }
    len = status;

    if (digit_len > 0 || (spec->flags & FORMAT_FLAG_ALTERNATE) != 0) {
        if ((status = format_buffer_put(buffer, capacity, len, '.')) < 0) {
            return status;
        }
        len = status;
    }

    for (int32 i = 0; i < digit_len; i += 1) {
        char digit;

        if (i < FORMAT_DOUBLE_HEX_DIGITS) {
            digit = digits[i];
        } else {
            digit = '0';
        }
        if ((status = format_buffer_put(buffer, capacity, len,
                                        digit)) < 0) {
            return status;
        }
        len = status;
    }

    return format_float_hex_append_exponent(buffer, capacity, len,
                                            exponent, upper);
}

static int32
format_float_generate_body(FormatSpec *spec, double value,
                           char *buffer, int32 capacity);

static int32
format_float_parse_exponent(char *body, int32 body_len,
                            int32 *exponent) {
    int32 index;
    int64 value;
    int32 sign;

    ASSERT(body != NULL);
    ASSERT_NON_NEGATIVE(body_len);
    ASSERT(exponent != NULL);

    index = 0;
    while (index < body_len && body[index] != 'e'
           && body[index] != 'E') {
        index += 1;
    }
    if (index >= body_len) {
        return -EINVAL;
    }

    index += 1;
    if (index >= body_len) {
        return -EINVAL;
    }

    sign = 1;
    if (body[index] == '-') {
        sign = -1;
        index += 1;
    } else if (body[index] == '+') {
        index += 1;
    }
    if (index >= body_len || !format_is_digit(body[index])) {
        return -EINVAL;
    }

    value = 0;
    while (index < body_len) {
        int32 digit;

        if (!format_is_digit(body[index])) {
            return -EINVAL;
        }
        digit = format_digit_value(body[index]);
        if (value > (INT32_MAX - digit)/10) {
            return -EOVERFLOW;
        }
        value = value*10 + digit;
        index += 1;
    }

    if (sign < 0) {
        value = -value;
    }
    *exponent = (int32)value;
    return 0;
}

static int32
format_float_strip_trailing_zeros(char *body, int32 body_len) {
    int32 exponent_index;
    int32 point_index;
    int32 end;

    ASSERT(body != NULL);
    ASSERT_NON_NEGATIVE(body_len);

    exponent_index = body_len;
    point_index = -1;
    for (int32 i = 0; i < body_len; i += 1) {
        if (body[i] == '.') {
            point_index = i;
        } else if (body[i] == 'e' || body[i] == 'E') {
            exponent_index = i;
            break;
        }
    }
    if (point_index < 0) {
        return body_len;
    }

    end = exponent_index;
    while (end > point_index + 1 && body[end - 1] == '0') {
        end -= 1;
    }
    if (end == point_index + 1) {
        end = point_index;
    }

    if (exponent_index < body_len) {
        memmove(body + end, body + exponent_index,
                (size_t)(body_len - exponent_index));
        end += body_len - exponent_index;
    }

    return end;
}

static int32
format_float_generate_general_body(FormatSpec *spec, double value,
                                   char *buffer, int32 capacity) {
    FormatSpec work_spec;
    int32 exponent;
    int32 body_len;
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT(format_float_is_general(spec->conversion));
    ASSERT(spec->precision >= 1);

    work_spec = *spec;
    if (format_float_is_upper(spec->conversion)) {
        work_spec.conversion = 'E';
    } else {
        work_spec.conversion = 'e';
    }
    work_spec.precision = spec->precision - 1;
    body_len = format_float_generate_body(&work_spec, value, buffer,
                                          capacity);
    if (body_len < 0) {
        return body_len;
    }

    if ((status = format_float_parse_exponent(buffer, body_len,
                                              &exponent)) < 0) {
        return status;
    }

    if (exponent >= -4 && exponent < spec->precision) {
        work_spec = *spec;
        if (format_float_is_upper(spec->conversion)) {
            work_spec.conversion = 'F';
        } else {
            work_spec.conversion = 'f';
        }
        work_spec.precision = spec->precision - (exponent + 1);
        body_len = format_float_generate_body(&work_spec, value, buffer,
                                              capacity);
        if (body_len < 0) {
            return body_len;
        }
    }

    if ((spec->flags & FORMAT_FLAG_ALTERNATE) == 0) {
        body_len = format_float_strip_trailing_zeros(buffer, body_len);
    }

    return body_len;
}

static int32
format_float_generate_body(FormatSpec *spec, double value,
                           char *buffer, int32 capacity) {
    int32 body_len;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);

    if (isnan(value) || isinf(value)) {
        int32 status;

        status = format_float_special_body(value, spec, buffer, &body_len);
        if (status < 0) {
            return status;
        }
        return body_len;
    }

    if (format_float_is_general(spec->conversion)) {
        return format_float_generate_general_body(spec, value, buffer,
                                                  capacity);
    }
    if (format_float_is_hex(spec->conversion)) {
        return format_float_generate_hex_body(spec, value, buffer, capacity);
    }

    if (format_float_is_fixed(spec->conversion)) {
        body_len = d2fixed_buffered_n(value, (uint32_t)spec->precision,
                                      buffer);
    } else {
        body_len = d2exp_buffered_n(value, (uint32_t)spec->precision,
                                    buffer);
    }
    if (body_len <= 0 || body_len >= capacity) {
        return -EOVERFLOW;
    }

    if (buffer[0] == '-') {
        memmove(buffer, buffer + 1, (size_t)(body_len - 1));
        body_len -= 1;
    }
    if ((spec->flags & FORMAT_FLAG_ALTERNATE) != 0) {
        body_len = format_float_force_decimal_point(buffer, body_len,
                                                    capacity);
        if (body_len < 0) {
            return body_len;
        }
    }
    if (format_float_is_upper(spec->conversion)) {
        format_float_uppercase_body(buffer, body_len);
    }

    return body_len;
}

static void
format_write_float(FormatSink *sink, FormatSpec *spec,
                   double value, char *body, int32 body_len) {
    ASSERT(sink != NULL);
    ASSERT(spec != NULL);
    ASSERT(body != NULL);
    ASSERT_NON_NEGATIVE(body_len);

    format_write_float_sign(sink, spec, format_float_sign(value, spec),
                            body, body_len);
    return;
}


static int32
format_handle_float(FormatSink *sink, FormatSpec *spec, FormatArgs *args) {
    char stack_buffer[FORMAT_FLOAT_PRINTF_STACK_BUFFER_SIZE];
    char *body;
    double value;
    int64 capacity64;
    int32 body_len;
    int32 capacity;
    int32 status;

    ASSERT(sink != NULL);
    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if (!format_float_is_fixed(spec->conversion)
        && !format_float_is_scientific(spec->conversion)
        && !format_float_is_general(spec->conversion)
        && !format_float_is_hex(spec->conversion)) {
        return -ENOSYS;
    }

    if ((status = format_load_float_width_precision(spec, args)) < 0) {
        return status;
    }
    if (format_float_is_general(spec->conversion) && spec->precision == 0) {
        spec->precision = 1;
    }
    if ((status = format_float_temp_capacity(spec, &capacity64)) < 0) {
        return status;
    }
    ASSERT(capacity64 <= INT32_MAX);
    capacity = (int32)capacity64;

    if (capacity <= SIZEOF(stack_buffer)) {
        body = stack_buffer;
    } else {
        body = malloc2(capacity);
    }

    if (spec->length == FORMAT_LENGTH_BIG_L) {
        ldouble long_value;

        long_value = va_arg(args->args, ldouble);
        body_len = format_long_double_generate_body(spec, long_value, body,
                                                    capacity);
        if (body_len >= 0) {
            format_write_float_sign(
                sink, spec, format_long_double_sign(long_value, spec),
                body, body_len);
            status = sink->status;
        } else {
            status = body_len;
        }
    } else {
        value = va_arg(args->args, double);
        body_len = format_float_generate_body(spec, value, body, capacity);
        if (body_len >= 0) {
            format_write_float(sink, spec, value, body, body_len);
            status = sink->status;
        } else {
            status = body_len;
        }
    }

    if (body != stack_buffer) {
        free2(body, capacity);
    }
    return status;
}

static int32
format_vsnprintf_impl(char *buffer, int64 capacity, char *format,
                      va_list args) {
    FormatArgs format_args;
    FormatSink sink;
    char *literal;
    char *cursor;
    int32 result;
    int32 status;

    if (format == NULL) {
        return -EINVAL;
    }
    if ((status = format_sink_init(&sink, buffer, capacity)) < 0) {
        return status;
    }

    va_copy(format_args.args, args);
    literal = format;
    cursor = format;
    while (*cursor != '\0') {
        FormatSpec spec;

        if (*cursor != '%') {
            cursor += 1;
            continue;
        }

        format_sink_write(&sink, literal, cursor - literal);
        if (sink.status < 0) {
            result = format_sink_finish(&sink);
            goto done;
        }

        cursor += 1;
        if ((status = format_parse_spec(cursor, &cursor, &spec)) < 0) {
            result = status;
            goto done;
        }
        if (format_is_integer_conversion(spec.conversion)) {
            status = format_handle_integer(&sink, &spec, &format_args);
            if (status < 0) {
                result = status;
                goto done;
            }
        } else if (spec.conversion == 'c' || spec.conversion == 's') {
            status = format_handle_char_string(&sink, &spec, &format_args);
            if (status < 0) {
                result = status;
                goto done;
            }
        } else if (spec.conversion == 'p') {
            status = format_handle_pointer(&sink, &spec, &format_args);
            if (status < 0) {
                result = status;
                goto done;
            }
        } else if (spec.conversion == 'n') {
            status = format_handle_count(&sink, &spec, &format_args);
            if (status < 0) {
                result = status;
                goto done;
            }
        } else if (format_is_float_conversion(spec.conversion)) {
            status = format_handle_float(&sink, &spec, &format_args);
            if (status < 0) {
                result = status;
                goto done;
            }
        } else if (spec.conversion == '%') {
            format_sink_write_byte(&sink, '%');
        } else {
            result = -ENOSYS;
            goto done;
        }

        literal = cursor;
    }

    format_sink_write(&sink, literal, cursor - literal);
    result = format_sink_finish(&sink);

done:
    va_end(format_args.args);
    return result;
}

int32 ATTR_PRINTF(3, 0)
format_vsnprintf(char *buffer, int64 capacity, char *format, va_list args) {
    return format_vsnprintf_impl(buffer, capacity, format, args);
}

int32 ATTR_PRINTF(3, 4)
format_snprintf(char *buffer, int64 capacity, char *format, ...) {
    va_list args;
    int32 len;

    va_start(args, format);
    len = format_vsnprintf(buffer, capacity, format, args);
    va_end(args);
    return len;
}

static int32
format_float_validate_buffer(char *buffer, int64 capacity) {
    if (buffer == NULL) {
        return -EINVAL;
    }
    if (capacity <= 0) {
        return -EINVAL;
    }

    return 0;
}

static int32
format_float_validate_precision(int32 precision) {
    if (precision < 0) {
        return -EINVAL;
    }
    if (precision > FORMAT_FLOAT_MAX_PRECISION) {
        return -ERANGE;
    }

    return 0;
}

static int32
format_float_copy(char *buffer, int64 capacity,
                  char *source, int32 source_len) {
    ASSERT(buffer != NULL);
    ASSERT_POSITIVE(capacity);
    ASSERT(source != NULL);
    ASSERT_NON_NEGATIVE(source_len);
    ASSERT_LESS(source_len, FORMAT_FLOAT_RYU_BUFFER_SIZE);

    if ((int64)source_len >= capacity) {
        return -ENOSPC;
    }

    memcpy(buffer, source, (size_t)source_len);
    buffer[source_len] = '\0';
    return source_len;
}

int32
format_float32_shortest(char *buffer, int64 capacity, float value) {
    int32 status;
    int32 len;
    char temp[FORMAT_FLOAT_RYU_BUFFER_SIZE];

    if ((status = format_float_validate_buffer(buffer, capacity)) < 0) {
        return status;
    }

    len = (int32)f2s_buffered_n(value, temp);
    return format_float_copy(buffer, capacity, temp, len);
}

int32
format_float64_shortest(char *buffer, int64 capacity, double value) {
    int32 status;
    int32 len;
    char temp[FORMAT_FLOAT_RYU_BUFFER_SIZE];

    if ((status = format_float_validate_buffer(buffer, capacity)) < 0) {
        return status;
    }

    len = (int32)d2s_buffered_n(value, temp);
    return format_float_copy(buffer, capacity, temp, len);
}

int32
format_float64_fixed(char *buffer, int64 capacity, double value,
                     int32 precision) {
    int32 status;
    int32 len;
    char temp[FORMAT_FLOAT_RYU_BUFFER_SIZE];

    if ((status = format_float_validate_buffer(buffer, capacity)) < 0) {
        return status;
    }
    if ((status = format_float_validate_precision(precision)) < 0) {
        return status;
    }

    len = (int32)d2fixed_buffered_n(value, (uint32_t)precision, temp);
    return format_float_copy(buffer, capacity, temp, len);
}

int32
format_float64_scientific(char *buffer, int64 capacity, double value,
                          int32 precision) {
    int32 status;
    int32 len;
    char temp[FORMAT_FLOAT_RYU_BUFFER_SIZE];

    if ((status = format_float_validate_buffer(buffer, capacity)) < 0) {
        return status;
    }
    if ((status = format_float_validate_precision(precision)) < 0) {
        return status;
    }

    len = (int32)d2exp_buffered_n(value, (uint32_t)precision, temp);
    return format_float_copy(buffer, capacity, temp, len);
}

void
sb_float64(StrBuilder *str_builder, double value) {
    int32 len;

    sb_reserve(str_builder, FORMAT_FLOAT_RYU_BUFFER_SIZE);
    len = (int32)d2s_buffered_n(value, str_builder->data + str_builder->len);
    str_builder->len += len;
    str_builder->data[str_builder->len] = '\0';
    return;
}

void
sb_float64_fixed(StrBuilder *sb, double value, int32 precision) {
    int32 status;
    int32 len;

    if ((status = format_float_validate_precision(precision)) < 0) {
        error("Invalid float precision %d.\n", precision);
        fatal(EXIT_FAILURE);
    }

    sb_reserve(sb, FORMAT_FLOAT_RYU_BUFFER_SIZE);
    len = d2fixed_buffered_n(value, (uint32_t)precision, sb->data + sb->len);
    sb->len += len;
    sb->data[sb->len] = '\0';
    return;
}

#if TESTING_format
#define CBASE_IMPLEMENT
#include "cbase.h"
#include "ryu.h"

static int32
format_test_snprintf(char *buffer, int64 capacity, char *format, ...) {
    va_list args;
    int32 len;

    va_start(args, format);
    len = format_vsnprintf_impl(buffer, capacity, format, args);
    va_end(args);
    return len;
}

static int32
format_test_validate(char *format) {
    char *cursor;

    if (format == NULL) {
        return -EINVAL;
    }

    cursor = format;
    while (*cursor != '\0') {
        FormatSpec spec;
        int32 status;

        if (*cursor != '%') {
            cursor += 1;
            continue;
        }

        cursor += 1;
        if ((status = format_parse_spec(cursor, &cursor, &spec)) < 0) {
            return status;
        }
    }

    return 0;
}

static FormatSpec
format_test_parse_one(char *format) {
    FormatSpec spec;
    char *next;
    int32 status;

    ASSERT(format != NULL);
    ASSERT_EQUAL(format[0], '%');

    next = NULL;
    status = format_parse_spec(format + 1, &next, &spec);
    ASSERT_EQUAL(status, 0);
    ASSERT_EQUAL(*next, '\0');
    return spec;
}

static void
test_format_parser_valid_specs(void) {
    FormatSpec spec;

    spec = format_test_parse_one("%%");
    ASSERT_EQUAL(spec.conversion, '%');
    ASSERT_EQUAL(spec.flags, 0);
    ASSERT(spec.length == FORMAT_LENGTH_NONE);

    spec = format_test_parse_one("%08.3d");
    ASSERT_EQUAL(spec.conversion, 'd');
    ASSERT_EQUAL(spec.flags, FORMAT_FLAG_ZERO);
    ASSERT(spec.width_kind == FORMAT_WIDTH_LITERAL);
    ASSERT_EQUAL(spec.width, 8);
    ASSERT(spec.precision_kind == FORMAT_PRECISION_LITERAL);
    ASSERT_EQUAL(spec.precision, 3);
    ASSERT(spec.length == FORMAT_LENGTH_NONE);

    spec = format_test_parse_one("%*.*f");
    ASSERT_EQUAL(spec.conversion, 'f');
    ASSERT(spec.width_kind == FORMAT_WIDTH_ARG);
    ASSERT(spec.precision_kind == FORMAT_PRECISION_ARG);
    ASSERT(spec.length == FORMAT_LENGTH_NONE);

    spec = format_test_parse_one("%-+ #0w32x");
    ASSERT_EQUAL(spec.conversion, 'x');
    ASSERT_EQUAL(spec.flags, FORMAT_FLAG_LEFT
                             |FORMAT_FLAG_SIGN
                             |FORMAT_FLAG_SPACE
                             |FORMAT_FLAG_ALTERNATE
                             |FORMAT_FLAG_ZERO);
    ASSERT(spec.length == FORMAT_LENGTH_W32);

    spec = format_test_parse_one("%hhd");
    ASSERT_EQUAL(spec.conversion, 'd');
    ASSERT(spec.length == FORMAT_LENGTH_HH);

    spec = format_test_parse_one("%llu");
    ASSERT_EQUAL(spec.conversion, 'u');
    ASSERT(spec.length == FORMAT_LENGTH_LL);

    spec = format_test_parse_one("%w64B");
    ASSERT_EQUAL(spec.conversion, 'B');
    ASSERT(spec.length == FORMAT_LENGTH_W64);

    spec = format_test_parse_one("%La");
    ASSERT_EQUAL(spec.conversion, 'a');
    ASSERT(spec.length == FORMAT_LENGTH_BIG_L);

    spec = format_test_parse_one("%w16n");
    ASSERT_EQUAL(spec.conversion, 'n');
    ASSERT(spec.length == FORMAT_LENGTH_W16);

    ASSERT_EQUAL(format_test_validate("a %% b %08d %*.*s"), 0);
    return;
}

static void
test_format_parser_invalid_specs(void) {
    ASSERT_EQUAL(format_test_validate("%"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%2$d"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%*2$d"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%.*2$s"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%m"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%q"), -EINVAL);

    ASSERT_EQUAL(format_test_validate("%ld"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%lc"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%.5ls"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%zd"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%td"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%jd"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%wfd"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%wf32d"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%w24d"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%wd"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%Lx"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%lf"), -EINVAL);

    ASSERT_EQUAL(format_test_validate("%+s"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%05s"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%.2c"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%#p"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%0p"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%+p"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%.2p"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%10n"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%-n"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%+n"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%.0n"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%ln"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%5%"), -EINVAL);
    ASSERT_EQUAL(format_test_validate("%.0%"), -EINVAL);

    return;
}

static void
test_format_sink_capacity(char *format, char *expected) {
    char buffer[128];
    int32 expected_len;

    expected_len = strlen32(expected);
    ASSERT_LESS(expected_len + 2, SIZEOF(buffer));

    for (int32 capacity = 0; capacity <= expected_len + 2; capacity += 1) {
        int32 copied;
        int32 len;

        memset(buffer, 0x7f, SIZEOF(buffer));
        len = format_test_snprintf(buffer, capacity, format);
        ASSERT_EQUAL(len, expected_len);

        if (capacity == 0) {
            ASSERT_EQUAL(buffer[0], (char)0x7f);
            continue;
        }

        copied = MIN(expected_len, capacity - 1);
        ASSERT_EQUAL(buffer, copied, expected, copied);
        ASSERT_EQUAL(buffer[copied], '\0');
        ASSERT_EQUAL(buffer[capacity], (char)0x7f);
    }

    return;
}

static void
test_format_integer_capacity(char *expected, char *format, ...) {
    char buffer[256];
    int32 expected_len;

    expected_len = strlen32(expected);
    ASSERT_LESS(expected_len + 2, SIZEOF(buffer));

    for (int32 capacity = 0; capacity <= expected_len + 2; capacity += 1) {
        va_list args;
        int32 copied;
        int32 len;

        memset(buffer, 0x7f, SIZEOF(buffer));
        va_start(args, format);
        len = format_vsnprintf_impl(buffer, capacity, format, args);
        va_end(args);
        ASSERT_EQUAL(len, expected_len);

        if (capacity == 0) {
            ASSERT_EQUAL(buffer[0], (char)0x7f);
            continue;
        }

        copied = MIN(expected_len, capacity - 1);
        ASSERT_EQUAL(buffer, copied, expected, copied);
        ASSERT_EQUAL(buffer[copied], '\0');
        ASSERT_EQUAL(buffer[capacity], (char)0x7f);
    }

    return;
}

static void
test_format_bytes_capacity(char *expected, int32 expected_len,
                           char *format, ...) {
    char buffer[256];

    ASSERT_NON_NEGATIVE(expected_len);
    ASSERT_LESS(expected_len + 2, SIZEOF(buffer));

    for (int32 capacity = 0; capacity <= expected_len + 2; capacity += 1) {
        va_list args;
        int32 copied;
        int32 len;

        memset(buffer, 0x7f, SIZEOF(buffer));
        va_start(args, format);
        len = format_vsnprintf_impl(buffer, capacity, format, args);
        va_end(args);
        ASSERT_EQUAL(len, expected_len);

        if (capacity == 0) {
            ASSERT_EQUAL(buffer[0], (char)0x7f);
            continue;
        }

        copied = MIN(expected_len, capacity - 1);
        ASSERT_EQUAL(buffer, copied, expected, copied);
        ASSERT_EQUAL(buffer[copied], '\0');
        ASSERT_EQUAL(buffer[capacity], (char)0x7f);
    }

    return;
}

static void
test_format_integer_outputs(void) {
    test_format_integer_capacity("0", "%d", 0);
    test_format_integer_capacity("-123", "%i", -123);
    test_format_integer_capacity("-2147483648", "%d", INT32_MIN);
    test_format_integer_capacity("4294967295", "%u", (uint32)UINT32_MAX);
    test_format_integer_capacity("12", "%o", (uint32)10);
    test_format_integer_capacity("abc", "%x", (uint32)0xabc);
    test_format_integer_capacity("ABC", "%X", (uint32)0xabc);
    test_format_integer_capacity("1010", "%b", (uint32)10);
    test_format_integer_capacity("1010", "%B", (uint32)10);

    test_format_integer_capacity("+42", "%+d", 42);
    test_format_integer_capacity(" 42", "% d", 42);
    test_format_integer_capacity("+42", "%+ d", 42);
    test_format_integer_capacity("-0000042", "%08d", -42);
    test_format_integer_capacity("42    ", "%-6d", 42);
    test_format_integer_capacity("   00042", "%8.5d", 42);
    test_format_integer_capacity("   00042", "%08.5d", 42);
    test_format_integer_capacity("00042   ", "%-8.5d", 42);
    test_format_integer_capacity("", "%.0d", 0);
    test_format_integer_capacity("     ", "%5.0d", 0);

    test_format_integer_capacity("012", "%#o", (uint32)10);
    test_format_integer_capacity("0", "%#.0o", (uint32)0);
    test_format_integer_capacity("    0", "%#5.0o", (uint32)0);
    test_format_integer_capacity("012", "%#.3o", (uint32)10);
    test_format_integer_capacity("00012", "%#.5o", (uint32)10);
    test_format_integer_capacity("0x2a", "%#x", (uint32)42);
    test_format_integer_capacity("0X2A", "%#X", (uint32)42);
    test_format_integer_capacity("0b101", "%#b", (uint32)5);
    test_format_integer_capacity("0B101", "%#B", (uint32)5);
    test_format_integer_capacity(" 0x0a", "%#5.2x", (uint32)10);
    test_format_integer_capacity("00000012", "%#08o", (uint32)10);

    test_format_integer_capacity("42    ", "%*d", -6, 42);
    test_format_integer_capacity("00042", "%0*d", 5, 42);
    test_format_integer_capacity("42", "%.*d", -1, 42);
    test_format_integer_capacity("00042", "%.*d", 5, 42);
    test_format_integer_capacity("   00042", "%*.*d", 8, 5, 42);

    test_format_integer_capacity("-1 2a 3", "%d %x %u", -1,
                                 (uint32)0x2a, (uint32)3);

    test_format_integer_capacity("18", "%hhd", 0x12);
    test_format_integer_capacity("44", "%hhu", 300);
    test_format_integer_capacity("-1234", "%hd", -1234);
    test_format_integer_capacity("65535", "%hu", 65535);
    test_format_integer_capacity("-9223372036854775808", "%lld",
                                 (int64)INT64_MIN);
    test_format_integer_capacity("18446744073709551615", "%llu",
                                 (uint64)UINT64_MAX);

    test_format_integer_capacity("-128", "%w8d", -128);
    test_format_integer_capacity("255", "%w8u", 255);
    test_format_integer_capacity("65535", "%w16u", 65535);
    test_format_integer_capacity("-2147483648", "%w32d", INT32_MIN);
    test_format_integer_capacity("4294967295", "%w32u",
                                 (uint32)UINT32_MAX);
    test_format_integer_capacity("-9223372036854775808", "%w64d",
                                 (int64)INT64_MIN);
    test_format_integer_capacity("18446744073709551615", "%w64u",
                                 (uint64)UINT64_MAX);

    return;
}

static void
test_format_char_string_outputs(void) {
    char nul_char_expected[] = {'\0'};
    char span[] = {'a', '\0', 'b', 'c'};
    char span_expected[] = {'a', '\0', 'b', 'c'};
    char span_width_expected[] = {' ', ' ', 'a', '\0', 'b'};
    char span_left_expected[] = {'a', '\0', 'b', ' ', ' '};
    char plain_precision_expected[] = {'a'};
    char spaces[] = {' ', ' ', ' '};
    char buffer[16];

    test_format_bytes_capacity("A", 1, "%c", 'A');
    test_format_bytes_capacity("  A", 3, "%3c", 'A');
    test_format_bytes_capacity("A  ", 3, "%-3c", 'A');
    test_format_bytes_capacity(nul_char_expected, 1, "%c", 0);

    test_format_bytes_capacity("abc", 3, "%s", "abc");
    test_format_bytes_capacity("  abc", 5, "%5s", "abc");
    test_format_bytes_capacity("abc  ", 5, "%-5s", "abc");
    test_format_bytes_capacity("ab", 2, "%.2s", "abc");
    test_format_bytes_capacity("   ab", 5, "%5.2s", "abc");
    test_format_bytes_capacity("(null)", 6, "%s", (char *)NULL);
    test_format_bytes_capacity("(nu", 3, "%.3s", (char *)NULL);
    test_format_bytes_capacity("     (nu", 8, "%8.3s", (char *)NULL);

    test_format_bytes_capacity(span_expected, 4, "%.*s", 4, span);
    test_format_bytes_capacity(span_width_expected, 5, "%5.*s", 3, span);
    test_format_bytes_capacity(span_left_expected, 5, "%-5.*s", 3, span);
    test_format_bytes_capacity(plain_precision_expected, 1, "%.4s", span);
    test_format_bytes_capacity("", 0, "%.*s", 0, (char *)NULL);
    test_format_bytes_capacity(spaces, 3, "%3.*s", 0, (char *)NULL);

    test_format_bytes_capacity("x=abc n=7 c=Z", 13, "x=%s n=%d c=%c",
                               "abc", 7, 'Z');

    memset(buffer, 0x7f, SIZEOF(buffer));
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%.*s", -1,
                                      "abc"), -EINVAL);
    ASSERT_EQUAL(buffer[0], '\0');
    ASSERT_EQUAL(buffer[1], (char)0x7f);

    memset(buffer, 0x7f, SIZEOF(buffer));
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%.*s", 1,
                                      (char *)NULL), -EINVAL);
    ASSERT_EQUAL(buffer[0], '\0');
    ASSERT_EQUAL(buffer[1], (char)0x7f);

    return;
}


static void
test_format_pointer_count_outputs(void) {
    char buffer[16];
    void *pointer;
    int8 count8;
    int16 count16;
    int32 count32;
    int64 count64;

    pointer = (void *)(uintptr)0x1234;
    test_format_bytes_capacity("0x0", 3, "%p", (void *)NULL);
    test_format_bytes_capacity("   0x0", 6, "%6p", (void *)NULL);
    test_format_bytes_capacity("0x0   ", 6, "%-6p", (void *)NULL);
    test_format_bytes_capacity("0x1234", 6, "%p", pointer);
    test_format_bytes_capacity("p=0x1234.", 9, "p=%p.", pointer);
    test_format_bytes_capacity("   0x0", 6, "%*p", 6, (void *)NULL);
    test_format_bytes_capacity("0x0   ", 6, "%*p", -6, (void *)NULL);

    count32 = -1;
    test_format_bytes_capacity("abcd", 4, "ab%ncd", &count32);
    ASSERT_EQUAL(count32, 2);

    memset(buffer, 0x7f, SIZEOF(buffer));
    count32 = -1;
    ASSERT_EQUAL(format_test_snprintf(buffer, 2, "abcd%n", &count32), 4);
    ASSERT_EQUAL(count32, 4);
    ASSERT_EQUAL(buffer[0], 'a');
    ASSERT_EQUAL(buffer[1], '\0');
    ASSERT_EQUAL(buffer[2], (char)0x7f);

    count8 = -1;
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "abc%hhn",
                                      &count8), 3);
    ASSERT_EQUAL(count8, 3);

    count16 = -1;
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "abc%hn",
                                      &count16), 3);
    ASSERT_EQUAL(count16, 3);

    count64 = -1;
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "abc%lln",
                                      &count64), 3);
    ASSERT_EQUAL(count64, 3);

    count8 = -1;
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "abc%w8n",
                                      &count8), 3);
    ASSERT_EQUAL(count8, 3);

    count16 = -1;
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "abc%w16n",
                                      &count16), 3);
    ASSERT_EQUAL(count16, 3);

    count32 = -1;
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "abc%w32n",
                                      &count32), 3);
    ASSERT_EQUAL(count32, 3);

    count64 = -1;
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "abc%w64n",
                                      &count64), 3);
    ASSERT_EQUAL(count64, 3);

    count8 = -7;
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%128d%hhn", 0,
                                      &count8), -EOVERFLOW);
    ASSERT_EQUAL(count8, -7);

    count16 = -7;
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%32768d%hn", 0,
                                      &count16), -EOVERFLOW);
    ASSERT_EQUAL(count16, -7);

    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "abc%n",
                                      (int32 *)NULL), -EINVAL);

    return;
}

static double
format_test_double_from_bits(uint64 bits) {
    double value;

    memcpy(&value, &bits, (size_t)SIZEOF(value));
    return value;
}

static double
format_test_positive_nan(void) {
    return format_test_double_from_bits(UINT64_C(0x7ff8000000000000));
}

static double
format_test_negative_nan(void) {
    return format_test_double_from_bits(UINT64_C(0xfff8000000000000));
}

static void
test_format_printf_float_outputs(void) {
    char buffer[64];
    double pos_nan;
    double neg_nan;

    pos_nan = format_test_positive_nan();
    neg_nan = format_test_negative_nan();

    test_format_bytes_capacity("1.250000", 8, "%f", 1.25);
    test_format_bytes_capacity("1.25", 4, "%.2f", 1.25);
    test_format_bytes_capacity("1", 1, "%.0f", 1.25);
    test_format_bytes_capacity("1.", 2, "%#.0f", 1.25);
    test_format_bytes_capacity("  1.25", 6, "%6.2f", 1.25);
    test_format_bytes_capacity("1.25  ", 6, "%-6.2f", 1.25);
    test_format_bytes_capacity("0000001.25", 10, "%010.2f", 1.25);
    test_format_bytes_capacity("+000001.25", 10, "%+010.2f", 1.25);
    test_format_bytes_capacity("-000001.25", 10, "%010.2f", -1.25);
    test_format_bytes_capacity(" 1.25", 5, "% .2f", 1.25);
    test_format_bytes_capacity("-0.000", 6, "%.3f", -0.0);

    test_format_bytes_capacity("1.250000e+00", 12, "%e", 1.25);
    test_format_bytes_capacity("1.25e+00", 8, "%.2e", 1.25);
    test_format_bytes_capacity("1e+00", 5, "%.0e", 1.25);
    test_format_bytes_capacity("1.e+00", 6, "%#.0e", 1.25);
    test_format_bytes_capacity("1.25E+00", 8, "%.2E", 1.25);
    test_format_bytes_capacity("+001.25e+00", 11, "%+011.2e", 1.25);

    test_format_bytes_capacity("inf", 3, "%f", HUGE_VAL);
    test_format_bytes_capacity("-inf", 4, "%f", -HUGE_VAL);
    test_format_bytes_capacity("+inf", 4, "%+f", HUGE_VAL);
    test_format_bytes_capacity(" inf", 4, "% f", HUGE_VAL);
    test_format_bytes_capacity("00000inf", 8, "%08f", HUGE_VAL);
    test_format_bytes_capacity("INF", 3, "%F", HUGE_VAL);
    test_format_bytes_capacity("INF", 3, "%E", HUGE_VAL);
    test_format_bytes_capacity("nan", 3, "%f", pos_nan);
    test_format_bytes_capacity("-nan", 4, "%f", neg_nan);
    test_format_bytes_capacity("NAN", 3, "%F", pos_nan);
    test_format_bytes_capacity("-NAN", 4, "%F", neg_nan);

    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%.*f",
                                      -1, 1.25), 8);
    ASSERT_EQUAL(buffer, "1.250000");
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%.1048576f",
                                      1.0), -EOVERFLOW);

    return;
}

static void
test_format_printf_general_outputs(void) {
    char buffer[64];

    test_format_bytes_capacity("1.25", 4, "%g", 1.25);
    test_format_bytes_capacity("123456", 6, "%g", 123456.0);
    test_format_bytes_capacity("1.23457e+06", 11, "%g", 1234567.0);
    test_format_bytes_capacity("0.0001", 6, "%g", 0.0001);
    test_format_bytes_capacity("9.9999e-05", 10, "%g", 0.000099999);
    test_format_bytes_capacity("1e-05", 5, "%g", 0.00001);
    test_format_bytes_capacity("99999.9", 7, "%g", 99999.9);
    test_format_bytes_capacity("1e+05", 5, "%.5g", 99999.9);
    test_format_bytes_capacity("1e+05", 5, "%.4g", 99999.0);
    test_format_bytes_capacity("0.0001", 6, "%.4g", 0.000099999);
    test_format_bytes_capacity("1", 1, "%.0g", 1.25);
    test_format_bytes_capacity("1.", 2, "%#.0g", 1.25);
    test_format_bytes_capacity("0.0000", 6, "%#.5g", 0.0);
    test_format_bytes_capacity("1.2500", 6, "%#.5g", 1.25);
    test_format_bytes_capacity("100.00", 6, "%#.5g", 100.0);
    test_format_bytes_capacity("9.9999e-05", 10, "%#.5g", 0.000099999);
    test_format_bytes_capacity("      1.25", 10, "%10.4g", 1.25);
    test_format_bytes_capacity("0000001.25", 10, "%010.4g", 1.25);
    test_format_bytes_capacity("-000000000", 10, "%010.4g", -0.0);
    test_format_bytes_capacity("1.25      ", 10, "%-10.4g", 1.25);
    test_format_bytes_capacity("1.23457E+06", 11, "%G", 1234567.0);
    test_format_bytes_capacity("9.99990E-05", 11, "%#.6G", 0.000099999);
    test_format_bytes_capacity("INF", 3, "%G", HUGE_VAL);
    test_format_bytes_capacity("NAN", 3, "%G", format_test_positive_nan());
    test_format_bytes_capacity("-NAN", 4, "%G", format_test_negative_nan());
    test_format_bytes_capacity("nan", 3, "%g", format_test_positive_nan());
    test_format_bytes_capacity("-nan", 4, "%g", format_test_negative_nan());

    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%.*g",
                                      -1, 1.25), 4);
    ASSERT_EQUAL(buffer, "1.25");
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%.*g",
                                      0, 123.0), 5);
    ASSERT_EQUAL(buffer, "1e+02");

    return;
}


static void
test_format_printf_hex_float_outputs(void) {
    char buffer[64];
    double lsb_after_one;
    double true_min;
    double normal_min;
    double largest_subnormal;
    double before_one;

    lsb_after_one = format_test_double_from_bits(UINT64_C(0x3ff0000000000001));
    true_min = format_test_double_from_bits(UINT64_C(0x0000000000000001));
    normal_min = format_test_double_from_bits(UINT64_C(0x0010000000000000));
    largest_subnormal = format_test_double_from_bits(
        UINT64_C(0x000fffffffffffff));
    before_one = format_test_double_from_bits(UINT64_C(0x3fefffffffffffff));

    test_format_bytes_capacity("0x0p+0", 6, "%a", 0.0);
    test_format_bytes_capacity("-0x0p+0", 7, "%a", -0.0);
    test_format_bytes_capacity("0x1p+0", 6, "%a", 1.0);
    test_format_bytes_capacity("0x1.8p+0", 8, "%a", 1.5);
    test_format_bytes_capacity("0X1.8P+0", 8, "%A", 1.5);
    test_format_bytes_capacity("+0x1.8p+0", 9, "%+a", 1.5);
    test_format_bytes_capacity(" 0x1.8p+0", 9, "% a", 1.5);
    test_format_bytes_capacity("   0x1.8p+0", 11, "%11a", 1.5);
    test_format_bytes_capacity("0x1.8p+0   ", 11, "%-11a", 1.5);
    test_format_bytes_capacity("0x0000001.8p+0", 14, "%014a", 1.5);
    test_format_bytes_capacity("+0x000001.8p+0", 14, "%+014a", 1.5);

    test_format_bytes_capacity("0x1.0000000000000p+0", 20, "%.13a", 1.0);
    test_format_bytes_capacity("0x1.p+0", 7, "%#.0a", 1.0);
    test_format_bytes_capacity("0x1.0p+0", 8, "%.1a", 1.0);
    test_format_bytes_capacity("0x2p+0", 6, "%.0a", 1.5);
    test_format_bytes_capacity("0x1p+0", 6, "%.0a", 1.25);
    test_format_bytes_capacity("0x2p-1", 6, "%.0a", before_one);
    test_format_bytes_capacity("0x2.0p-1", 8, "%.1a", before_one);

    test_format_bytes_capacity("0x1.0000000000001p+0", 20, "%a",
                               lsb_after_one);
    test_format_bytes_capacity("0x0.0000000000001p-1022", 23, "%a",
                               true_min);
    test_format_bytes_capacity("0x1p-1022", 9, "%a", normal_min);
    test_format_bytes_capacity("0x0.fffffffffffffp-1022", 23, "%a",
                               largest_subnormal);
    test_format_bytes_capacity("0x1p-1022", 9, "%.0a", largest_subnormal);
    test_format_bytes_capacity("0x1.0p-1022", 11, "%.1a",
                               largest_subnormal);

    test_format_bytes_capacity("INF", 3, "%A", HUGE_VAL);
    test_format_bytes_capacity("NAN", 3, "%A", format_test_positive_nan());
    test_format_bytes_capacity("-NAN", 4, "%A", format_test_negative_nan());
    test_format_bytes_capacity("nan", 3, "%a", format_test_positive_nan());
    test_format_bytes_capacity("-nan", 4, "%a", format_test_negative_nan());

    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%.*a",
                                      -1, 1.5), 8);
    ASSERT_EQUAL(buffer, "0x1.8p+0");
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%.1048576a",
                                      1.0), -EOVERFLOW);

    return;
}


static bool
format_test_long_double_supported(void) {
    FormatBinaryFloat parts;
    int32 status;

    status = format_decompose_long_double(1.0L, &parts);
    if (status == -ENOSYS) {
        return false;
    }
    ASSERT_EQUAL(status, 0);
    return true;
}

static void
test_format_long_double_parts(ldouble value, bool negative,
                              int32 bit_len, int32 binary_exponent) {
    FormatBinaryFloat parts;

    ASSERT_EQUAL(format_decompose_long_double(value, &parts), 0);
    ASSERT(parts.negative == negative);
    ASSERT(parts.zero == (bit_len == 0));
    ASSERT_EQUAL(parts.precision_bits, LDBL_MANT_DIG);
    ASSERT_EQUAL(format_big_uint_bit_len(&parts.significand), bit_len);
    ASSERT_EQUAL(parts.binary_exponent, binary_exponent);
    return;
}

static void
test_format_long_double_exact_integer(ldouble value, char *expected) {
    char buffer[128];
    FormatBinaryFloat parts;
    FormatBigUInt integer;
    int32 len;

    ASSERT_EQUAL(format_decompose_long_double(value, &parts), 0);
    ASSERT_EQUAL(format_binary_float_to_exact_integer(&parts, &integer), 0);
    len = format_big_uint_to_decimal(&integer, buffer, SIZEOF(buffer));
    ASSERT_EQUAL(len, strlen32(expected));
    ASSERT_EQUAL(buffer, expected);
    return;
}

static void
test_format_long_double_scaled(ldouble value, int32 decimal_places,
                               char *expected,
                               enum FormatRemainderHalf expected_rem) {
    char buffer[128];
    FormatBinaryFloat parts;
    FormatBigUInt integer;
    enum FormatRemainderHalf remainder;
    int32 len;

    ASSERT_EQUAL(format_decompose_long_double(value, &parts), 0);
    ASSERT_EQUAL(format_binary_float_scaled_decimal(&parts, decimal_places,
                                                    &integer, &remainder), 0);
    ASSERT(remainder == expected_rem);
    len = format_big_uint_to_decimal(&integer, buffer, SIZEOF(buffer));
    ASSERT_EQUAL(len, strlen32(expected));
    ASSERT_EQUAL(buffer, expected);
    return;
}

static void
test_format_long_double_decomposition(void) {
    FormatBinaryFloat parts;
    FormatBigUInt integer;
    ldouble true_min;
    int32 expected_exp;

    if (!format_test_long_double_supported()) {
        return;
    }

    ASSERT_EQUAL(format_decompose_long_double((ldouble)INFINITY, &parts),
                 -EINVAL);
    ASSERT_EQUAL(format_decompose_long_double((ldouble)NAN, &parts),
                 -EINVAL);

    test_format_long_double_parts(0.0L, false, 0, 0);
    test_format_long_double_parts(-0.0L, true, 0, 0);
    test_format_long_double_parts(1.0L, false, LDBL_MANT_DIG,
                                  1 - LDBL_MANT_DIG);
    test_format_long_double_parts(-1.0L, true, LDBL_MANT_DIG,
                                  1 - LDBL_MANT_DIG);
    test_format_long_double_parts(0.5L, false, LDBL_MANT_DIG,
                                  -LDBL_MANT_DIG);
    test_format_long_double_parts(2.0L, false, LDBL_MANT_DIG,
                                  2 - LDBL_MANT_DIG);

    expected_exp = LDBL_MIN_EXP - LDBL_MANT_DIG;
    test_format_long_double_parts(LDBL_MIN, false, LDBL_MANT_DIG,
                                  expected_exp);

    expected_exp = LDBL_MAX_EXP - LDBL_MANT_DIG;
    test_format_long_double_parts(LDBL_MAX, false, LDBL_MANT_DIG,
                                  expected_exp);
    ASSERT_EQUAL(format_decompose_long_double(LDBL_MAX, &parts), 0);
    ASSERT_EQUAL(format_binary_float_to_exact_integer(&parts, &integer), 0);
    ASSERT_EQUAL(format_big_uint_bit_len(&integer), LDBL_MAX_EXP);

    true_min = ldexpl(1.0L, LDBL_MIN_EXP - LDBL_MANT_DIG);
    if (true_min != 0.0L) {
        test_format_long_double_parts(true_min, false, 1,
                                      LDBL_MIN_EXP - LDBL_MANT_DIG);
    }

    return;
}

static void
test_format_long_double_decimal_helpers(void) {
    FormatBinaryFloat parts;
    FormatBigUInt integer;

    if (!format_test_long_double_supported()) {
        return;
    }

    test_format_long_double_exact_integer(0.0L, "0");
    test_format_long_double_exact_integer(1.0L, "1");
    test_format_long_double_exact_integer(2.0L, "2");
    test_format_long_double_exact_integer(ldexpl(1.0L, 64),
                                          "18446744073709551616");

    ASSERT_EQUAL(format_decompose_long_double(0.5L, &parts), 0);
    ASSERT_EQUAL(format_binary_float_to_exact_integer(&parts, &integer),
                 -ERANGE);

    test_format_long_double_scaled(0.125L, 3, "125",
                                   FORMAT_REMAINDER_ZERO);
    test_format_long_double_scaled(0.25L, 0, "0",
                                   FORMAT_REMAINDER_LESS_HALF);
    test_format_long_double_scaled(0.5L, 0, "0",
                                   FORMAT_REMAINDER_HALF);
    test_format_long_double_scaled(0.75L, 0, "0",
                                   FORMAT_REMAINDER_MORE_HALF);
    test_format_long_double_scaled(1.25L, 1, "12",
                                   FORMAT_REMAINDER_HALF);
    return;
}

static void
test_format_printf_long_double_outputs(void) {
    char buffer[128];
    ldouble precise;

    test_format_bytes_capacity("1.250000", 8, "%Lf", (ldouble)1.25);
    test_format_bytes_capacity("1.25", 4, "%.2Lf", (ldouble)1.25);
    test_format_bytes_capacity("1", 1, "%.0Lf", (ldouble)1.25);
    test_format_bytes_capacity("1.", 2, "%#.0Lf", (ldouble)1.25);
    test_format_bytes_capacity("  1.25", 6, "%6.2Lf", (ldouble)1.25);
    test_format_bytes_capacity("1.25  ", 6, "%-6.2Lf", (ldouble)1.25);
    test_format_bytes_capacity("0000001.25", 10, "%010.2Lf", (ldouble)1.25);
    test_format_bytes_capacity("+000001.25", 10, "%+010.2Lf", (ldouble)1.25);
    test_format_bytes_capacity("-000001.25", 10, "%010.2Lf", (ldouble)-1.25);
    test_format_bytes_capacity("-0.000", 6, "%.3Lf", (ldouble)-0.0L);
    test_format_bytes_capacity("0.125", 5, "%.3Lf", (ldouble)0.125L);
    test_format_bytes_capacity("2", 1, "%.0Lf", (ldouble)1.5L);
    test_format_bytes_capacity("2", 1, "%.0Lf", (ldouble)2.5L);

    test_format_bytes_capacity("1.250000e+00", 12, "%Le", (ldouble)1.25);
    test_format_bytes_capacity("1.25e+00", 8, "%.2Le", (ldouble)1.25);
    test_format_bytes_capacity("1e+00", 5, "%.0Le", (ldouble)1.25);
    test_format_bytes_capacity("1.e+00", 6, "%#.0Le", (ldouble)1.25);
    test_format_bytes_capacity("1.25E+00", 8, "%.2LE", (ldouble)1.25);
    test_format_bytes_capacity("+001.25e+00", 11, "%+011.2Le", (ldouble)1.25);
    test_format_bytes_capacity("1.250e-03", 9, "%.3Le", (ldouble)0.00125L);
    test_format_bytes_capacity("1.00e+03", 8, "%.2Le", (ldouble)999.9L);

    test_format_bytes_capacity("inf", 3, "%Lf", (ldouble)INFINITY);
    test_format_bytes_capacity("-inf", 4, "%Lf", (ldouble)-INFINITY);
    test_format_bytes_capacity("INF", 3, "%LF", (ldouble)INFINITY);
    test_format_bytes_capacity("INF", 3, "%LE", (ldouble)INFINITY);

    if (format_test_long_double_supported() && LDBL_MANT_DIG > DBL_MANT_DIG) {
        precise = ldexpl(1.0L, 63) + 1.0L;
        test_format_bytes_capacity("9223372036854775809", 19, "%.0Lf",
                                   precise);
    }

    test_format_bytes_capacity("1", 1, "%Lg", (ldouble)1.0);
    test_format_bytes_capacity("1.25", 4, "%Lg", (ldouble)1.25);
    test_format_bytes_capacity("1.23457e+06", 11, "%Lg", (ldouble)1234567.0L);
    test_format_bytes_capacity("0.0001", 6, "%Lg", (ldouble)0.0001L);
    test_format_bytes_capacity("1e-05", 5, "%Lg", (ldouble)0.00001L);
    test_format_bytes_capacity("1.", 2, "%#.0Lg", (ldouble)1.25L);
    test_format_bytes_capacity("1.2500", 6, "%#.5Lg", (ldouble)1.25L);
    test_format_bytes_capacity("      1.25", 10, "%10.4Lg", (ldouble)1.25L);
    test_format_bytes_capacity("0000001.25", 10, "%010.4Lg", (ldouble)1.25L);
    test_format_bytes_capacity("1.250E+00", 9, "%.3LE", (ldouble)1.25L);
    test_format_bytes_capacity("1.25", 4, "%.3LG", (ldouble)1.25L);
    test_format_bytes_capacity("INF", 3, "%LG", (ldouble)INFINITY);
    test_format_bytes_capacity("inf", 3, "%Lg", (ldouble)INFINITY);

    test_format_bytes_capacity("0x0p+0", 6, "%La", (ldouble)0.0L);
    test_format_bytes_capacity("-0x0p+0", 7, "%La", (ldouble)-0.0L);
    test_format_bytes_capacity("0x1p+0", 6, "%La", (ldouble)1.0L);
    test_format_bytes_capacity("0x1.8p+0", 8, "%La", (ldouble)1.5L);
    test_format_bytes_capacity("0X1.8P+0", 8, "%LA", (ldouble)1.5L);
    test_format_bytes_capacity("+0x1.8p+0", 9, "%+La", (ldouble)1.5L);
    test_format_bytes_capacity("0x0000001.8p+0", 14, "%014La", (ldouble)1.5L);
    test_format_bytes_capacity("0x1.0000p+0", 11, "%.4La", (ldouble)1.0L);
    test_format_bytes_capacity("0x1.p+0", 7, "%#.0La", (ldouble)1.0L);
    test_format_bytes_capacity("0x2p+0", 6, "%.0La", (ldouble)1.5L);
    test_format_bytes_capacity("INF", 3, "%LA", (ldouble)INFINITY);
    test_format_bytes_capacity("inf", 3, "%La", (ldouble)INFINITY);

    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%.*Lf",
                                      -1, (ldouble)1.25), 8);
    ASSERT_EQUAL(buffer, "1.250000");
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%.*Lg",
                                      -1, (ldouble)1.25), 4);
    ASSERT_EQUAL(buffer, "1.25");
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%.*La",
                                      -1, (ldouble)1.5), 8);
    ASSERT_EQUAL(buffer, "0x1.8p+0");

    return;
}

static int32
format_test_public_vsnprintf(char *buffer, int64 capacity, char *format, ...) {
    va_list args;
    int32 len;

    va_start(args, format);
    len = format_vsnprintf(buffer, capacity, format, args);
    va_end(args);
    return len;
}

static void
test_format_public_api(void) {
    char buffer[32];
    char tiny[4];
    int32 len;
    int32 count;

    len = format_snprintf(buffer, SIZEOF(buffer), "public:%d:%s",
                          42, "ok");
    ASSERT_EQUAL(len, 12);
    ASSERT_EQUAL(buffer, len + 1, "public:42:ok", 13);

    len = format_snprintf(tiny, SIZEOF(tiny), "abcdef");
    ASSERT_EQUAL(len, 6);
    ASSERT_EQUAL(tiny, SIZEOF(tiny), "abc", 4);

    len = format_snprintf(NULL, 0, "abcdef");
    ASSERT_EQUAL(len, 6);

    len = format_test_public_vsnprintf(buffer, SIZEOF(buffer), "%s:%.*s",
                                       NULL, 3, "a\0b");
    ASSERT_EQUAL(len, 10);
    ASSERT_EQUAL(buffer, len + 1, "(null):a\0b", 11);

    count = -1;
    len = format_snprintf(tiny, SIZEOF(tiny), "abcd%n", &count);
    ASSERT_EQUAL(len, 4);
    ASSERT_EQUAL(count, 4);
    ASSERT_EQUAL(tiny, SIZEOF(tiny), "abc", 4);
    return;
}

static void
test_format_sink_validation(void) {
    char buffer[8];
    FormatSink sink;

    ASSERT_EQUAL(format_test_snprintf(NULL, 0, "abc"), 3);
    ASSERT_EQUAL(format_test_snprintf(NULL, 1, "abc"), -EINVAL);
    ASSERT_EQUAL(format_test_snprintf(buffer, -1, "abc"), -EINVAL);
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), NULL), -EINVAL);
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%*d",
                                      INT32_MIN, 0), -EOVERFLOW);

    memset(buffer, 0x7f, SIZEOF(buffer));
    memset(buffer, 0x7f, SIZEOF(buffer));
    ASSERT_EQUAL(format_test_snprintf(buffer, SIZEOF(buffer), "%"), -EINVAL);
    ASSERT_EQUAL(buffer[0], '\0');
    ASSERT_EQUAL(buffer[1], (char)0x7f);

    ASSERT_EQUAL(format_sink_init(&sink, buffer, SIZEOF(buffer)), 0);
    sink.total = (int64)INT32_MAX + 1;
    ASSERT_EQUAL(format_sink_finish(&sink), -EOVERFLOW);

    return;
}

static void
test_format_float32_shortest(float value, char *expected) {
    char buffer[FORMAT_FLOAT_RYU_BUFFER_SIZE];
    int32 len;

    len = format_float32_shortest(buffer, SIZEOF(buffer), value);
    ASSERT_EQUAL(len, strlen32(expected));
    ASSERT_EQUAL(buffer, expected);

    return;
}

static void
test_format_float64_shortest(double value, char *expected) {
    char buffer[FORMAT_FLOAT_RYU_BUFFER_SIZE];
    int32 len;

    len = format_float64_shortest(buffer, SIZEOF(buffer), value);
    ASSERT_EQUAL(len, strlen32(expected));
    ASSERT_EQUAL(buffer, expected);

    return;
}

static void
test_format_float64_fixed(double value, int32 precision, char *expected) {
    char buffer[FORMAT_FLOAT_RYU_BUFFER_SIZE];
    int32 len;

    len = format_float64_fixed(buffer, SIZEOF(buffer), value, precision);
    ASSERT_EQUAL(len, strlen32(expected));
    ASSERT_EQUAL(buffer, expected);

    return;
}

static void
test_format_float64_scientific(double value, int32 precision, char *expected) {
    char buffer[FORMAT_FLOAT_RYU_BUFFER_SIZE];
    int32 len;

    len = format_float64_scientific(buffer, SIZEOF(buffer), value, precision);
    ASSERT_EQUAL(len, strlen32(expected));
    ASSERT_EQUAL(buffer, expected);

    return;
}

static uint32
test_format_float32_bits(float value) {
    uint32 bits;

    memcpy(&bits, &value, SIZEOF(bits));
    return bits;
}

static uint64
test_format_float64_bits(double value) {
    uint64 bits;

    memcpy(&bits, &value, SIZEOF(bits));
    return bits;
}

static void
test_format_float32_round_trip(float value) {
    char buffer[FORMAT_FLOAT_RYU_BUFFER_SIZE];
    char *end;
    float parsed;
    int32 len;

    len = format_float32_shortest(buffer, SIZEOF(buffer), value);
    ASSERT_POSITIVE(len);

    end = NULL;
    parsed = strtof(buffer, &end);
    ASSERT(end == buffer + len);
    ASSERT_EQUAL(test_format_float32_bits(parsed),
                 test_format_float32_bits(value));

    return;
}

static void
test_format_float64_round_trip(double value) {
    char buffer[FORMAT_FLOAT_RYU_BUFFER_SIZE];
    char *end;
    double parsed;
    int32 len;

    len = format_float64_shortest(buffer, SIZEOF(buffer), value);
    ASSERT_POSITIVE(len);

    end = NULL;
    parsed = strtod(buffer, &end);
    ASSERT(end == buffer + len);
    ASSERT_EQUAL(test_format_float64_bits(parsed),
                 test_format_float64_bits(value));

    return;
}

int
main(void) {
    char buffer[16];

    test_format_sink_capacity("", "");
    test_format_sink_capacity("abc", "abc");
    test_format_sink_capacity("%%", "%");
    test_format_sink_capacity("a%%b%%c", "a%b%c");
    test_format_sink_capacity("abc%%def", "abc%def");
    test_format_parser_valid_specs();
    test_format_parser_invalid_specs();
    test_format_integer_outputs();
    test_format_char_string_outputs();
    test_format_pointer_count_outputs();
    test_format_printf_float_outputs();
    test_format_printf_general_outputs();
    test_format_printf_hex_float_outputs();
    test_format_printf_long_double_outputs();
    test_format_long_double_decomposition();
    test_format_long_double_decimal_helpers();
    test_format_public_api();
    test_format_sink_validation();

    test_format_float64_shortest(0.0, "0E0");
    test_format_float64_shortest(-0.0, "-0E0");
    test_format_float64_shortest(1.0, "1E0");
    test_format_float64_shortest(0.1, "1E-1");
    test_format_float64_shortest(1234567.89, "1.23456789E6");
    test_format_float64_shortest(1e-7, "1E-7");

    test_format_float32_shortest(0.0f, "0E0");
    test_format_float32_shortest(-0.0f, "-0E0");
    test_format_float32_shortest(1.0f, "1E0");
    test_format_float32_shortest(0.1f, "1E-1");
    test_format_float32_shortest(1e-7f, "1E-7");

    test_format_float64_fixed(1.25, 2, "1.25");
    test_format_float64_fixed(1.2, 4, "1.2000");
    test_format_float64_fixed(-0.0, 3, "-0.000");

    test_format_float64_scientific(1234.0, 2, "1.23e+03");
    test_format_float64_scientific(0.00123, 3, "1.230e-03");

    {
        StrBuilder builder = {0};

        SB_APPEND(&builder, "x=");
        sb_float64(&builder, 0.1);
        SB_APPEND(&builder, " y=");
        sb_float64_fixed(&builder, 1.25, 2);
        ASSERT_EQUAL(builder.data, "x=1E-1 y=1.25");
        sb_free(&builder);
    }

    ASSERT_EQUAL(format_float64_shortest(NULL, 64, 1.0), -EINVAL);
    ASSERT_EQUAL(format_float64_shortest(buffer, 0, 1.0), -EINVAL);
    ASSERT_EQUAL(format_float64_fixed(buffer, SIZEOF(buffer), 1.0, -1),
                 -EINVAL);
    ASSERT_EQUAL(format_float64_fixed(buffer, 4, 1.25, 2), -ENOSPC);
    ASSERT_EQUAL(format_float64_fixed(buffer, SIZEOF(buffer), 1.0,
                                      FORMAT_FLOAT_MAX_PRECISION + 1),
                 -ERANGE);

    test_format_float64_round_trip(0.1);
    test_format_float32_round_trip(0.1f);

    exit(EXIT_SUCCESS);
}
#endif

#endif /* FORMAT_C */
