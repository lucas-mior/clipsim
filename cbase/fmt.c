// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(FMT_C)
#define FMT_C

#if !defined(TESTING_fmt)
#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_fmt 1
#else
#define TESTING_fmt 0
#endif
#endif

#include "cbase.h"

#if !defined(EOVERFLOW)
#define EOVERFLOW ERANGE
#endif

#if !defined(FMT_NULL_STRING)
#define FMT_NULL_STRING "null"
#endif

// For a binary floating type, MANT_DIG - MIN_EXP is the number of decimal
// fractional places needed to represent its smallest subnormal exactly.
enum {
    FMT_FLOAT_RYU_BUFFER_SIZE = 2000,
    FMT_DOUBLE_MAX_DECIMAL_PRECISION = DBL_MANT_DIG - DBL_MIN_EXP,
    FMT_LDOUBLE_MAX_DECIMAL_PRECISION = LDBL_MANT_DIG - LDBL_MIN_EXP,
    FMT_FLOAT_MAX_FIXED_PREFIX = 312,
    FMT_FLOAT_MAX_EXP_PREFIX = 8,
    FMT_LDOUBLE_MAX_FIXED_PREFIX = LDBL_MAX_10_EXP + 8,
    FMT_DOUBLE_PRINTF_BUFFER_SIZE = FMT_FLOAT_MAX_FIXED_PREFIX
                                    + FMT_DOUBLE_MAX_DECIMAL_PRECISION + 8,
    FMT_LDOUBLE_PRINTF_BUFFER_SIZE = FMT_LDOUBLE_MAX_FIXED_PREFIX
                                     + FMT_LDOUBLE_MAX_DECIMAL_PRECISION + 8,
    FMT_DOUBLE_HEX_DIGITS = 13,
    FMT_LDOUBLE_MAX_HEX_DIGITS = (LDBL_MANT_DIG - 1 + 3)/4,
    FMT_DOUBLE_FRACTION_BITS = 52,
    FMT_DOUBLE_EXPONENT_BIAS = 1023,
    FMT_DOUBLE_SUBNORMAL_EXPONENT = -1022,
    FMT_BIG_UINT_WORD_BITS = 32,
    // log2(10) is less than 10/3, so this bounds every decimal body.
    FMT_BIG_UINT_MAX_BITS = (FMT_LDOUBLE_PRINTF_BUFFER_SIZE*10 + 2)/3,
    FMT_BIG_UINT_MAX_WORDS = (FMT_BIG_UINT_MAX_BITS
                              + FMT_BIG_UINT_WORD_BITS - 1)
                             /FMT_BIG_UINT_WORD_BITS,
    FMT_LDOUBLE_DOUBLE_FRACTION_BITS = 52,
    FMT_LDOUBLE_DOUBLE_EXPONENT_BIAS = 1023,
    FMT_LDOUBLE_X87_FRACTION_BITS = 63,
    FMT_LDOUBLE_X87_EXPONENT_BIAS = 16383,
    FMT_LDOUBLE_X87_EXPONENT_MASK = 0x7fff,
    FMT_LDOUBLE_BINARY128_FRACTION_BITS = 112,
    FMT_LDOUBLE_BINARY128_EXPONENT_BIAS = 16383,
};

#if FLT_RADIX == 2 \
    && ((LDBL_MANT_DIG == DBL_MANT_DIG && LDBL_MAX_EXP == DBL_MAX_EXP) \
        || (LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384) \
        || (LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384))
#define FMT_LDOUBLE_SUPPORTED 1
#else
#define FMT_LDOUBLE_SUPPORTED 0
#endif

_Static_assert(FMT_PLAN_MAX_FORMAT_LEN - 1 <= UINT8_MAX,
               "format plan offsets do not fit in uint8");
_Static_assert(FMT_PLAN_MAX_FORMAT_LEN < FMT_MAX_FORMAT_LEN,
               "plan format limit must be smaller than normal limit");
_Static_assert(FMT_PLAN_MAX_SPECS <= UINT8_MAX,
               "format plan spec count does not fit in uint8");

_Static_assert(FMT_DOUBLE_PRINTF_BUFFER_SIZE < FMT_FLOAT_RYU_BUFFER_SIZE,
               "format fixed temporary buffer is too small");
_Static_assert(FMT_FLOAT_MAX_EXP_PREFIX
               + FMT_DOUBLE_MAX_DECIMAL_PRECISION
               < FMT_FLOAT_RYU_BUFFER_SIZE,
               "format exp temporary buffer is too small");

static char fmt_default_null_string[] = FMT_NULL_STRING;
static char *fmt_null_string = fmt_default_null_string;

void
fmt_set_null_string(char *string) {
    ASSERT(string != NULL);
    fmt_null_string = string;
    return;
}

#define ENUM_NAME FmtFlags
#define ENUM_BITFLAGS 1
#define ENUM_PREFIX_ FMT_FLAG_
#define ENUM_CHAR_REPR 1
#define ENUM_FIELDS             \
    XX(FMT_FLAG_LEFT,      '-') \
    XX(FMT_FLAG_SIGN,      '+') \
    XX(FMT_FLAG_SPACE,     ' ') \
    XX(FMT_FLAG_ALTERNATE, '#') \
    XX(FMT_FLAG_ZERO,      '0')
#define XENUMS_NO_TESTS 1
#include "xenums.c"
#undef XENUMS_NO_TESTS

enum FormatWidthKind {
    FMT_WIDTH_NONE,
    FMT_WIDTH_LITERAL,
    FMT_WIDTH_ARG,
};

enum FormatPrecisionKind {
    FMT_PRECISION_NONE,
    FMT_PRECISION_LITERAL,
    FMT_PRECISION_ARG,
};

enum FormatLength {
    FMT_LENGTH_NONE,
    FMT_LENGTH_HH,
    FMT_LENGTH_H,
    FMT_LENGTH_LL,
    FMT_LENGTH_BIG_L,
    FMT_LENGTH_W8,
    FMT_LENGTH_W16,
    FMT_LENGTH_W32,
    FMT_LENGTH_W64,
};

typedef struct FormatSpec {
    enum FmtFlags flags;
    int32 width;
    int32 precision;
    enum FormatWidthKind width_kind;
    enum FormatPrecisionKind precision_kind;
    enum FormatLength length;
    char conversion;
} FormatSpec;

static int32
fmt_digit_value(char byte) {
    ASSERT(is_digit(byte));
    return byte - '0';
}

static int32
fmt_parse_uint(char **cursor, int32 *value) {
    char *scan = *cursor;
    int32 result = 0;
    bool found_digit = false;

    while (is_digit(*scan)) {
        int32 digit = fmt_digit_value(*scan);

        if (result > (INT32_MAX - digit)/10) {
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
fmt_reject_star_positional(char *cursor) {
    char *scan;

    ASSERT(cursor != NULL);

    if (!is_digit(*cursor)) {
        return 0;
    }

    scan = cursor;
    while (is_digit(*scan)) {
        scan += 1;
    }
    if (*scan == '$') {
        return -EINVAL;
    }

    return 0;
}

static bool
fmt_is_integer_conversion(char conversion) {
    return conversion == 'd'
           || conversion == 'u'
           || conversion == 'o'
           || conversion == 'x'
           || conversion == 'X'
           || conversion == 'b'
           || conversion == 'B';
}

static bool
fmt_is_float_conversion(char conversion) {
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
fmt_length_is_integer(enum FormatLength length) {
    return length == FMT_LENGTH_NONE
           || length == FMT_LENGTH_HH
           || length == FMT_LENGTH_H
           || length == FMT_LENGTH_LL
           || length == FMT_LENGTH_W8
           || length == FMT_LENGTH_W16
           || length == FMT_LENGTH_W32
           || length == FMT_LENGTH_W64;
}

static bool
fmt_length_is_char_string(enum FormatLength length) {
    return length == FMT_LENGTH_NONE;
}

static bool
fmt_has_width(FormatSpec *spec) {
    ASSERT(spec != NULL);
    return spec->width_kind != FMT_WIDTH_NONE;
}

static bool
fmt_has_precision(FormatSpec *spec) {
    ASSERT(spec != NULL);
    return spec->precision_kind != FMT_PRECISION_NONE;
}

static int32
fmt_validate_flags(FormatSpec *spec, enum FmtFlags allowed_flags) {
    ASSERT(spec != NULL);

    if (spec->flags & ~allowed_flags) {
        return -EINVAL;
    }

    return 0;
}

static int32
fmt_parse_spec(char *cursor, char **next, FormatSpec *spec) {
    char *scan;
    int32 status;

    ASSERT(cursor != NULL);
    ASSERT(next != NULL);
    ASSERT(spec != NULL);

    *spec = (FormatSpec){0};

    if (*cursor == '\0') {
        return -EINVAL;
    }

    if (is_digit(*cursor)) {
        scan = cursor;
        while (is_digit(*scan)) {
            scan += 1;
        }
        if (*scan == '$') {
            return -EINVAL;
        }
    }

    spec->flags = FMT_FLAG_parse_chars(&cursor);

    if (*cursor == '*') {
        cursor += 1;
        if ((status = fmt_reject_star_positional(cursor)) < 0) {
            return status;
        }
        spec->width_kind = FMT_WIDTH_ARG;
    } else if (is_digit(*cursor)) {
        int32 width;

        spec->width_kind = FMT_WIDTH_LITERAL;
        if ((status = fmt_parse_uint(&cursor, &width)) < 0) {
            return status;
        }
        spec->width = width;
    }

    if (*cursor == '.') {
        cursor += 1;
        if (*cursor == '*') {
            cursor += 1;
            if ((status = fmt_reject_star_positional(cursor)) < 0) {
                return status;
            }
            spec->precision_kind = FMT_PRECISION_ARG;
        } else {
            spec->precision_kind = FMT_PRECISION_LITERAL;
            if (is_digit(*cursor)) {
                int32 precision;

                scan = cursor;
                while (is_digit(*scan)) {
                    scan += 1;
                }
                if (*scan == '$') {
                    return -EINVAL;
                }

                if ((status = fmt_parse_uint(&cursor, &precision)) < 0) {
                    return status;
                }
                spec->precision = precision;
            }
        }
    }

    if (cursor[0] == 'h' && cursor[1] == 'h') {
        spec->length = FMT_LENGTH_HH;
        cursor += 2;
    } else if (*cursor == 'h') {
        spec->length = FMT_LENGTH_H;
        cursor += 1;
    } else if (cursor[0] == 'l' && cursor[1] == 'l') {
        spec->length = FMT_LENGTH_LL;
        cursor += 2;
    } else if (*cursor == 'l') {
        return -EINVAL;
    } else if (*cursor == 'L') {
        spec->length = FMT_LENGTH_BIG_L;
        cursor += 1;
    } else if (*cursor == 'w') {
        int32 width;

        cursor += 1;
        if (*cursor == 'f') {
            return -EINVAL;
        }
        if (!is_digit(*cursor)) {
            return -EINVAL;
        }

        if ((status = fmt_parse_uint(&cursor, &width)) < 0) {
            return status;
        }

        if (width == 8) {
            spec->length = FMT_LENGTH_W8;
        } else if (width == 16) {
            spec->length = FMT_LENGTH_W16;
        } else if (width == 32) {
            spec->length = FMT_LENGTH_W32;
        } else if (width == 64) {
            spec->length = FMT_LENGTH_W64;
        } else {
            return -EINVAL;
        }
    } else if (*cursor == 'j' || *cursor == 'z' || *cursor == 't') {
        return -EINVAL;
    }

    if (*cursor == '\0') {
        return -EINVAL;
    }

    spec->conversion = *cursor;
    cursor += 1;

    if (fmt_is_integer_conversion(spec->conversion)) {
        if (!fmt_length_is_integer(spec->length)) {
            return -EINVAL;
        }
    } else if (fmt_is_float_conversion(spec->conversion)) {
        if (spec->length != FMT_LENGTH_NONE
            && spec->length != FMT_LENGTH_BIG_L) {
            return -EINVAL;
        }
    } else if (spec->conversion == 'c') {
        if (!fmt_length_is_char_string(spec->length)) {
            return -EINVAL;
        }
        if (fmt_has_precision(spec)) {
            return -EINVAL;
        }
        if ((status = fmt_validate_flags(spec, FMT_FLAG_LEFT)) < 0) {
            return status;
        }
    } else if (spec->conversion == 's') {
        if (!fmt_length_is_char_string(spec->length)) {
            return -EINVAL;
        }
        if ((status = fmt_validate_flags(spec, FMT_FLAG_LEFT)) < 0) {
            return status;
        }
    } else if (spec->conversion == 'p') {
        if (spec->length != FMT_LENGTH_NONE) {
            return -EINVAL;
        }
        if (fmt_has_precision(spec)) {
            return -EINVAL;
        }
        if ((status = fmt_validate_flags(spec, FMT_FLAG_LEFT)) < 0) {
            return status;
        }
    } else if (spec->conversion == 'n') {
        if (!fmt_length_is_integer(spec->length)) {
            return -EINVAL;
        }
        if (spec->flags
            || fmt_has_width(spec)
            || fmt_has_precision(spec)) {
            return -EINVAL;
        }
    } else if (spec->conversion == '%') {
        if (spec->length != FMT_LENGTH_NONE) {
            return -EINVAL;
        }
        if (spec->flags
            || fmt_has_width(spec)
            || fmt_has_precision(spec)) {
            return -EINVAL;
        }
    } else {
        return -EINVAL;
    }

    *next = cursor;
    return 0;
}

static void
fmt_plan_load_spec(FormatSpec *spec, FmtPlanSpec *plan_spec) {
    ASSERT(spec != NULL);
    ASSERT(plan_spec != NULL);

    spec->width = plan_spec->width;
    spec->precision = plan_spec->precision;
    spec->flags = (enum FmtFlags)plan_spec->flags;
    spec->width_kind = (enum FormatWidthKind)plan_spec->width_kind;
    spec->precision_kind = (enum FormatPrecisionKind)plan_spec->precision_kind;
    spec->length = (enum FormatLength)plan_spec->length;
    spec->conversion = plan_spec->conversion;

    return;
}

typedef struct FormatSink {
    char *buffer;
    int32 capacity;
    int32 written;
    int32 total;
    int32 status;
    bool unchecked;
    bool write_count;
} FormatSink;

static int32
fmt_sink_init(FormatSink *sink, char *buffer, int64 capacity) {
    if (capacity < 0) {
        return -EINVAL;
    }
    if (capacity > INT32_MAX) {
        return -EOVERFLOW;
    }
    if (capacity > 0 && buffer == NULL) {
        return -EINVAL;
    }

    sink->buffer = buffer;
    sink->capacity = (int32)capacity;
    sink->written = 0;
    sink->total = 0;
    sink->status = 0;
    sink->unchecked = false;
    sink->write_count = true;

    if (capacity > 0) {
        buffer[0] = '\0';
    }

    return 0;
}

static void
fmt_sink_add_total(FormatSink *sink, int64 len) {
    ASSERT(sink != NULL);
    ASSERT_GE(len, 0);

    if (sink->status < 0) {
        return;
    }
    if (len > (INT32_MAX - sink->total)) {
        sink->status = -EOVERFLOW;
        return;
    }

    sink->total += (int32)len;
    return;
}

static void
fmt_sink_write(FormatSink *sink, char *data, int64 len) {
    int32 available;
    int32 copy_len;

    ASSERT(sink != NULL);
    ASSERT(data != NULL);
    ASSERT_GE(len, 0);
    ASSERT_LT(len, INT32_MAX);

    if (sink->status < 0) {
        return;
    }

    if (sink->unchecked) {
        fmt_sink_add_total(sink, len);
        if (sink->status < 0) {
            return;
        }
        if (DEBUGGING) {
            ASSERT(sink->buffer != NULL);
            ASSERT_LT_VAR(sink->written + len, sink->capacity);
        }
        memcpy64(sink->buffer + sink->written, (void *)data, len);
        sink->written += (int32)len;
        sink->buffer[sink->written] = '\0';
        return;
    }

    fmt_sink_add_total(sink, len);
    if (sink->status < 0) {
        return;
    }
    if (sink->capacity <= 0) {
        return;
    }

    ASSERT(sink->buffer != NULL);
    ASSERT_LT_VAR(sink->written, sink->capacity);

    available = sink->capacity - 1 - sink->written;
    if (available <= 0) {
        return;
    }

    copy_len = (int32)MIN(len, available);
    memcpy64(sink->buffer + sink->written, (void *)data, copy_len);
    sink->written += copy_len;
    sink->buffer[sink->written] = '\0';
    return;
}

static void
fmt_sink_write_byte(FormatSink *sink, char byte) {
    fmt_sink_write(sink, &byte, 1);
    return;
}

static int32
fmt_sink_finish(FormatSink *sink) {
    ASSERT(sink != NULL);

    if (sink->status < 0) {
        return sink->status;
    }

    return sink->total;
}

static void
fmt_sink_write_repeat(FormatSink *sink, char byte, int64 len) {
    int32 available;
    int32 copy_len;

    ASSERT(sink != NULL);
    ASSERT_GE(len, 0);

    if (sink->status < 0) {
        return;
    }

    if (sink->unchecked) {
        fmt_sink_add_total(sink, len);
        if (sink->status < 0) {
            return;
        }
        if (DEBUGGING) {
            ASSERT(sink->buffer != NULL);
            ASSERT_LT_VAR(sink->written + len, sink->capacity);
        }
        memset64(sink->buffer + sink->written, byte, len);
        sink->written += (int32)len;
        sink->buffer[sink->written] = '\0';
        return;
    }

    fmt_sink_add_total(sink, len);
    if (sink->status < 0) {
        return;
    }
    if (sink->capacity <= 0) {
        return;
    }

    ASSERT(sink->buffer != NULL);
    ASSERT_LT_VAR(sink->written, sink->capacity);

    available = sink->capacity - 1 - sink->written;
    if (available <= 0) {
        return;
    }

    copy_len = (int32)MIN(len, (int64)available);
    memset64(sink->buffer + sink->written, byte, copy_len);
    sink->written += copy_len;
    sink->buffer[sink->written] = '\0';
    return;
}

static bool
fmt_is_signed_integer_conversion(char conversion) {
    return conversion == 'd';
}

typedef struct FormatArgs {
    va_list args;
} FormatArgs;

static int32
fmt_load_dynamic_width(FormatSpec *spec, FormatArgs *args) {
    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if (spec->width_kind == FMT_WIDTH_ARG) {
        int32 width = va_arg(args->args, int32);

        if (width < 0) {
            if (width == INT32_MIN) {
                return -EOVERFLOW;
            }
            spec->flags |= FMT_FLAG_LEFT;
            spec->width = -width;
        } else {
            spec->width = width;
        }
        spec->width_kind = FMT_WIDTH_LITERAL;
    }

    return 0;
}

static int32
fmt_load_dynamic_width_precision(FormatSpec *spec, FormatArgs *args) {
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if ((status = fmt_load_dynamic_width(spec, args)) < 0) {
        return status;
    }

    if (spec->precision_kind == FMT_PRECISION_ARG) {
        int32 precision = va_arg(args->args, int32);

        if (precision < 0) {
            spec->precision = 0;
            spec->precision_kind = FMT_PRECISION_NONE;
        } else {
            spec->precision = precision;
            spec->precision_kind = FMT_PRECISION_LITERAL;
        }
    }

    return 0;
}

typedef struct FormatIntegerValue {
    uint64 magnitude;
    bool negative;
} FormatIntegerValue;

static FormatIntegerValue
fmt_read_signed_integer(FormatSpec *spec, FormatArgs *args) {
    FormatIntegerValue value;
    int64 signed_value;

    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if (spec->length == FMT_LENGTH_HH) {
        signed_value = (int8)va_arg(args->args, int32);
    } else if (spec->length == FMT_LENGTH_H) {
        signed_value = (int16)va_arg(args->args, int32);
    } else if (spec->length == FMT_LENGTH_LL) {
        signed_value = va_arg(args->args, int64);
    } else if (spec->length == FMT_LENGTH_W8) {
        signed_value = (int8)va_arg(args->args, int32);
    } else if (spec->length == FMT_LENGTH_W16) {
        signed_value = (int16)va_arg(args->args, int32);
    } else if (spec->length == FMT_LENGTH_W32) {
        signed_value = va_arg(args->args, int32);
    } else if (spec->length == FMT_LENGTH_W64) {
        signed_value = va_arg(args->args, int64);
    } else {
        ASSERT(spec->length == FMT_LENGTH_NONE);
        signed_value = va_arg(args->args, int32);
    }

    if (signed_value < 0) {
        value.negative = true;
        value.magnitude = (uint64)(-(signed_value + 1)) + 1;
    } else {
        value.negative = false;
        value.magnitude = (uint64)signed_value;
    }
    return value;
}

static FormatIntegerValue
fmt_read_unsigned_integer(FormatSpec *spec, FormatArgs *args) {
    FormatIntegerValue value;

    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    value.negative = false;
    if (spec->length == FMT_LENGTH_HH) {
        value.magnitude = (uint8)va_arg(args->args, int32);
    } else if (spec->length == FMT_LENGTH_H) {
        value.magnitude = (uint16)va_arg(args->args, int32);
    } else if (spec->length == FMT_LENGTH_LL) {
        value.magnitude = va_arg(args->args, uint64);
    } else if (spec->length == FMT_LENGTH_W8) {
        value.magnitude = (uint8)va_arg(args->args, int32);
    } else if (spec->length == FMT_LENGTH_W16) {
        value.magnitude = (uint16)va_arg(args->args, int32);
    } else if (spec->length == FMT_LENGTH_W32) {
        value.magnitude = va_arg(args->args, uint32);
    } else if (spec->length == FMT_LENGTH_W64) {
        value.magnitude = va_arg(args->args, uint64);
    } else {
        ASSERT(spec->length == FMT_LENGTH_NONE);
        value.magnitude = va_arg(args->args, uint32);
    }

    return value;
}

static int32
fmt_integer_base(char conversion) {
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
fmt_integer_digits(char *digits, uint64 value, int32 base, bool upper) {
    char *lower_digits = "0123456789abcdef";
    char *upper_digits = "0123456789ABCDEF";
    char *digit_table;
    char reversed[64];
    int32 len;

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
        uint64 digit;

        switch (base) {
            case 16:
                digit = value & 15;
                value >>= 4;
                break;
            case 10:
                digit = value % 10;
                value /= 10;
                break;
            case 8:
                digit = value & 7;
                value >>= 3;
                break;
            case 2:
                digit = value & 1;
                value >>= 1;
                break;
            default:
                digit = value % (uint64)base;
                value /= (uint64)base;
                break;
        }

        reversed[len] = digit_table[digit];
        len += 1;
    }

    for (int32 i = 0; i < len; i += 1) {
        digits[i] = reversed[len - 1 - i];
    }

    return len;
}

static int64
fmt_pad_len(int64 width, int64 used) {
    if (width > used) {
        return width - used;
    }

    return 0;
}

static int32
fmt_integer_prefix(FormatSpec *spec, FormatIntegerValue value,
                   char *prefix, int32 digit_len, int64 precision_zeros) {
    ASSERT(spec != NULL);
    ASSERT(prefix != NULL);
    ASSERT_GE(digit_len, 0);
    ASSERT_GE(precision_zeros, 0);

    if (fmt_is_signed_integer_conversion(spec->conversion)) {
        if (value.negative) {
            prefix[0] = '-';
            return 1;
        }
        if (spec->flags & FMT_FLAG_SIGN) {
            prefix[0] = '+';
            return 1;
        }
        if (spec->flags & FMT_FLAG_SPACE) {
            prefix[0] = ' ';
            return 1;
        }
        return 0;
    }

    if ((spec->flags & FMT_FLAG_ALTERNATE) == 0) {
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
fmt_write_integer(FormatSink *sink, FormatSpec *spec,
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

    base = fmt_integer_base(spec->conversion);
    upper = spec->conversion == 'X' || spec->conversion == 'B';
    if (value.magnitude == 0 && fmt_has_precision(spec)
        && spec->precision == 0) {
        digit_len = 0;
    } else {
        digit_len = fmt_integer_digits(digits, value.magnitude, base, upper);
    }

    precision_zeros = 0;
    if (fmt_has_precision(spec) && spec->precision > digit_len) {
        precision_zeros = spec->precision - digit_len;
    }

    prefix_len = fmt_integer_prefix(spec, value, prefix, digit_len,
                                    precision_zeros);
    inner_len = prefix_len + precision_zeros + digit_len;

    zero_pad = 0;
    if ((spec->flags & FMT_FLAG_ZERO)
        && !(spec->flags & FMT_FLAG_LEFT)
        && !fmt_has_precision(spec)) {
        zero_pad = fmt_pad_len(spec->width, inner_len);
    }

    spaces = fmt_pad_len(spec->width, inner_len + zero_pad);
    if ((spec->flags & FMT_FLAG_LEFT) == 0) {
        fmt_sink_write_repeat(sink, ' ', spaces);
    }
    if (prefix_len > 0) {
        fmt_sink_write(sink, prefix, prefix_len);
    }
    fmt_sink_write_repeat(sink, '0', zero_pad);
    fmt_sink_write_repeat(sink, '0', precision_zeros);
    if (digit_len > 0) {
        fmt_sink_write(sink, digits, digit_len);
    }
    if (spec->flags & FMT_FLAG_LEFT) {
        fmt_sink_write_repeat(sink, ' ', spaces);
    }
    return;
}

static void
fmt_write_padded_bytes(FormatSink *sink, FormatSpec *spec,
                       char *data, int64 len) {
    int64 spaces;

    ASSERT(sink != NULL);
    ASSERT(spec != NULL);
    ASSERT_GE(len, 0);
    ASSERT(data != NULL || len == 0);

    spaces = fmt_pad_len(spec->width, len);
    if ((spec->flags & FMT_FLAG_LEFT) == 0) {
        fmt_sink_write_repeat(sink, ' ', spaces);
    }
    if (len > 0) {
        fmt_sink_write(sink, data, len);
    }
    if (spec->flags & FMT_FLAG_LEFT) {
        fmt_sink_write_repeat(sink, ' ', spaces);
    }
    return;
}

static int32
fmt_store_count(FormatSpec *spec, FormatArgs *args, int64 count,
                bool write_count) {
    ASSERT(spec != NULL);
    ASSERT(args != NULL);
    ASSERT_GE(count, 0);

    if (spec->length == FMT_LENGTH_HH
        || spec->length == FMT_LENGTH_W8) {
        if (count > INT8_MAX) {
            return -EOVERFLOW;
        }
    } else if (spec->length == FMT_LENGTH_H
               || spec->length == FMT_LENGTH_W16) {
        if (count > INT16_MAX) {
            return -EOVERFLOW;
        }
    } else if (spec->length == FMT_LENGTH_LL
               || spec->length == FMT_LENGTH_W64) {
        if (count > INT64_MAX) {
            return -EOVERFLOW;
        }
    } else {
        ASSERT(spec->length == FMT_LENGTH_NONE
               || spec->length == FMT_LENGTH_W32);
        if (count > INT32_MAX) {
            return -EOVERFLOW;
        }
    }

    if (spec->length == FMT_LENGTH_HH) {
        int8 *pointer = va_arg(args->args, int8 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        if (write_count) {
            *pointer = (int8)count;
        }
        return 0;
    }
    if (spec->length == FMT_LENGTH_H) {
        int16 *pointer = va_arg(args->args, int16 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        if (write_count) {
            *pointer = (int16)count;
        }
        return 0;
    }
    if (spec->length == FMT_LENGTH_LL) {
        int64 *pointer = va_arg(args->args, int64 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        if (write_count) {
            *pointer = count;
        }
        return 0;
    }
    if (spec->length == FMT_LENGTH_W8) {
        int8 *pointer = va_arg(args->args, int8 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        if (write_count) {
            *pointer = (int8)count;
        }
        return 0;
    }
    if (spec->length == FMT_LENGTH_W16) {
        int16 *pointer = va_arg(args->args, int16 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        if (write_count) {
            *pointer = (int16)count;
        }
        return 0;
    }
    if (spec->length == FMT_LENGTH_W32) {
        int32 *pointer = va_arg(args->args, int32 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        if (write_count) {
            *pointer = (int32)count;
        }
        return 0;
    }
    if (spec->length == FMT_LENGTH_W64) {
        int64 *pointer = va_arg(args->args, int64 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        if (write_count) {
            *pointer = count;
        }
        return 0;
    }

    ASSERT(spec->length == FMT_LENGTH_NONE);
    {
        int32 *pointer = va_arg(args->args, int32 *);

        if (pointer == NULL) {
            return -EINVAL;
        }
        if (write_count) {
            *pointer = (int32)count;
        }
        return 0;
    }
}

typedef struct FormatBigUInt {
    uint32 words[FMT_BIG_UINT_MAX_WORDS];
    int32 len;
} FormatBigUInt;

enum FormatRemainderHalf {
    FMT_REMAINDER_ZERO,
    FMT_REMAINDER_LESS_HALF,
    FMT_REMAINDER_HALF,
    FMT_REMAINDER_MORE_HALF,
};

typedef struct FormatBinaryFloat {
    uint64 significand_low;
    uint64 significand_high;
    int32 binary_exponent;
    int32 precision_bits;
    bool negative;
    bool zero;
} FormatBinaryFloat;

static void
fmt_big_uint_zero(FormatBigUInt *value) {
    ASSERT(value != NULL);

    memset64(value->words, 0, SIZEOF(value->words));
    value->len = 0;
    return;
}

static void
fmt_binary_float_zero_significand(FormatBinaryFloat *parts) {
    ASSERT(parts != NULL);

    parts->significand_low = 0;
    parts->significand_high = 0;
    return;
}

static void
fmt_binary_float_set_significand_uint64(FormatBinaryFloat *parts,
                                        uint64 value) {
    ASSERT(parts != NULL);

    parts->significand_low = value;
    parts->significand_high = 0;
    return;
}

static void UNUSED
fmt_binary_float_set_significand_uint128(FormatBinaryFloat *parts,
                                         uint64 low, uint64 high) {
    ASSERT(parts != NULL);

    parts->significand_low = low;
    parts->significand_high = high;
    return;
}

static int32 UNUSED
fmt_binary_float_set_significand_bit(FormatBinaryFloat *parts,
                                     int32 bit_index) {
    ASSERT(parts != NULL);
    ASSERT_GE(bit_index, 0);

    if (bit_index < 64) {
        parts->significand_low |= UINT64_C(1) << bit_index;
        return 0;
    }
    if (bit_index < 128) {
        parts->significand_high |= UINT64_C(1) << (bit_index - 64);
        return 0;
    }
    return -EOVERFLOW;
}

static bool
fmt_binary_float_test_significand_bit(FormatBinaryFloat *parts,
                                      int32 bit_index) {
    ASSERT(parts != NULL);
    ASSERT_GE(bit_index, 0);

    if (bit_index < 64) {
        return parts->significand_low & (UINT64_C(1) << bit_index);
    }
    if (bit_index < 128) {
        return parts->significand_high
               & (UINT64_C(1) << (bit_index - 64));
    }
    return false;
}

static int32 UNUSED
fmt_binary_float_significand_bit_len(FormatBinaryFloat *parts) {
    uint64 value;
    int32 bits;

    ASSERT(parts != NULL);

    if (parts->significand_high != 0) {
        value = parts->significand_high;
        bits = 64;
    } else {
        value = parts->significand_low;
        bits = 0;
    }
    while (value != 0) {
        bits += 1;
        value >>= 1;
    }
    return bits;
}

static void
fmt_binary_float_copy_significand(FormatBinaryFloat *parts,
                                  FormatBigUInt *integer) {
    ASSERT(parts != NULL);
    ASSERT(integer != NULL);
    ASSERT_GE(FMT_BIG_UINT_MAX_WORDS, 4);

    integer->words[0] = (uint32)parts->significand_low;
    integer->words[1] = (uint32)(parts->significand_low >> 32);
    integer->words[2] = (uint32)parts->significand_high;
    integer->words[3] = (uint32)(parts->significand_high >> 32);
    if (integer->words[3] != 0) {
        integer->len = 4;
    } else if (integer->words[2] != 0) {
        integer->len = 3;
    } else if (integer->words[1] != 0) {
        integer->len = 2;
    } else if (integer->words[0] != 0) {
        integer->len = 1;
    } else {
        integer->len = 0;
    }
    return;
}

static void
fmt_big_uint_normalize(FormatBigUInt *value) {
    ASSERT(value != NULL);
    ASSERT_GE(value->len, 0);
    ASSERT_LE(value->len, FMT_BIG_UINT_MAX_WORDS);

    while (value->len > 0 && value->words[value->len - 1] == 0) {
        value->len -= 1;
    }
    return;
}

static int32
fmt_big_uint_ensure_word(FormatBigUInt *value, int32 index) {
    ASSERT(value != NULL);
    ASSERT_GE(index, 0);

    if (index >= FMT_BIG_UINT_MAX_WORDS) {
        return -EOVERFLOW;
    }
    while (value->len <= index) {
        value->words[value->len] = 0;
        value->len += 1;
    }
    return 0;
}

static int32 UNUSED
fmt_big_uint_add_one(FormatBigUInt *value) {
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

        status = fmt_big_uint_ensure_word(value, value->len);
        if (status < 0) {
            return status;
        }
        value->words[value->len - 1] = (uint32)carry;
    }
    return 0;
}

static uint64 UNUSED
fmt_read_le_uint64(uchar *bytes) {
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
fmt_read_be_uint64(uchar *bytes) {
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
fmt_host_is_little_endian(void) {
    uint32 one;
    uchar bytes[SIZEOF(one)];

    one = 1;
    memcpy64(bytes, &one, SIZEOF(one));
    return bytes[0] == 1;
}

static int32 UNUSED
fmt_big_uint_bit_len(FormatBigUInt *value) {
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
    return (value->len - 1)*FMT_BIG_UINT_WORD_BITS + bits;
}

static bool
fmt_big_uint_test_bit(FormatBigUInt *value, int32 bit_index) {
    int32 word_index;
    int32 bit_offset;

    ASSERT(value != NULL);
    ASSERT_GE(bit_index, 0);

    word_index = bit_index / FMT_BIG_UINT_WORD_BITS;
    bit_offset = bit_index % FMT_BIG_UINT_WORD_BITS;
    if (word_index >= value->len) {
        return false;
    }
    return (value->words[word_index] & (UINT32_C(1) << bit_offset));
}

static int32
fmt_big_uint_shift_left(FormatBigUInt *value, int32 shift) {
    int32 original_len;
    int32 word_shift;
    int32 bit_shift;
    int32 max_new_len;

    ASSERT(value != NULL);
    ASSERT_GE(shift, 0);

    if (value->len == 0 || shift == 0) {
        return 0;
    }

    original_len = value->len;
    word_shift = shift / FMT_BIG_UINT_WORD_BITS;
    bit_shift = shift % FMT_BIG_UINT_WORD_BITS;
    max_new_len = original_len + word_shift;
    if (bit_shift != 0) {
        max_new_len += 1;
    }
    if (max_new_len > FMT_BIG_UINT_MAX_WORDS) {
        return -EOVERFLOW;
    }

    if (word_shift != 0) {
        for (int32 i = original_len - 1; i >= 0; i -= 1) {
            value->words[i + word_shift] = value->words[i];
        }
        for (int32 i = 0; i < word_shift; i += 1) {
            value->words[i] = 0;
        }
    }

    value->len = original_len + word_shift;
    if (bit_shift != 0) {
        uint32 carry = 0;

        for (int32 i = word_shift; i < value->len; i += 1) {
            uint64 shifted;

            shifted = ((uint64)value->words[i] << bit_shift) | carry;
            value->words[i] = (uint32)shifted;
            carry = (uint32)(shifted >> 32);
        }
        if (carry != 0) {
            value->words[value->len] = carry;
            value->len += 1;
        }
    }
    fmt_big_uint_normalize(value);
    return 0;
}

static void
fmt_big_uint_shift_right(FormatBigUInt *value, int32 shift) {
    int32 word_shift;
    int32 bit_shift;

    ASSERT(value != NULL);
    ASSERT_GE(shift, 0);

    if (value->len == 0 || shift == 0) {
        return;
    }

    word_shift = shift / FMT_BIG_UINT_WORD_BITS;
    bit_shift = shift % FMT_BIG_UINT_WORD_BITS;
    if (word_shift >= value->len) {
        fmt_big_uint_zero(value);
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
                              |(high << (FMT_BIG_UINT_WORD_BITS
                                         - bit_shift));
        }
    }
    value->len -= word_shift;
    fmt_big_uint_normalize(value);
    return;
}

static bool
fmt_big_uint_has_low_bits(FormatBigUInt *value, int32 bits) {
    int32 full_words;
    int32 partial_bits;

    ASSERT(value != NULL);
    ASSERT_GE(bits, 0);

    if (bits == 0 || value->len == 0) {
        return false;
    }

    full_words = bits / FMT_BIG_UINT_WORD_BITS;
    partial_bits = bits % FMT_BIG_UINT_WORD_BITS;
    for (int32 i = 0; i < full_words && i < value->len; i += 1) {
        if (value->words[i] != 0) {
            return true;
        }
    }
    if (partial_bits != 0 && full_words < value->len) {
        uint32 mask;

        mask = (UINT32_C(1) << partial_bits) - 1;
        if (value->words[full_words] & mask) {
            return true;
        }
    }
    return false;
}

static enum FormatRemainderHalf
fmt_big_uint_remainder_half(FormatBigUInt *value, int32 bits) {
    int32 half_bit;

    ASSERT(value != NULL);
    ASSERT_GT(bits, 0);

    if (!fmt_big_uint_has_low_bits(value, bits)) {
        return FMT_REMAINDER_ZERO;
    }

    half_bit = bits - 1;
    if (!fmt_big_uint_test_bit(value, half_bit)) {
        return FMT_REMAINDER_LESS_HALF;
    }
    if (fmt_big_uint_has_low_bits(value, half_bit)) {
        return FMT_REMAINDER_MORE_HALF;
    }
    return FMT_REMAINDER_HALF;
}

static int32
fmt_big_uint_mul_small(FormatBigUInt *value, uint32 factor) {
    uint64 carry;

    ASSERT(value != NULL);

    if (value->len == 0 || factor == 1) {
        return 0;
    }
    if (factor == 0) {
        fmt_big_uint_zero(value);
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

        status = fmt_big_uint_ensure_word(value, value->len);
        if (status < 0) {
            return status;
        }
        value->words[value->len - 1] = (uint32)carry;
    }
    return 0;
}

static uint32
fmt_big_uint_div_small(FormatBigUInt *value, uint32 divisor) {
    uint64 remainder;

    ASSERT(value != NULL);
    ASSERT_GT(divisor, 0);

    remainder = 0;
    for (int32 i = value->len - 1; i >= 0; i -= 1) {
        uint64 current = (remainder << 32) | value->words[i];
        uint64 quotient = current / divisor;
        remainder = current % divisor;
        value->words[i] = (uint32)quotient;
    }
    fmt_big_uint_normalize(value);
    return (uint32)remainder;
}

static int32 UNUSED
fmt_big_uint_to_decimal(FormatBigUInt *value, char *buffer, int32 capacity) {
    enum { GROUP_BASE = 1000000000, GROUP_DIGITS = 9 };
    int32 write_index;
    int32 len;

    ASSERT(value != NULL);
    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT_BETWEEN(value->len, 0, FMT_BIG_UINT_MAX_WORDS);

    if (value->len == 0) {
        if (capacity < 2) {
            return -EOVERFLOW;
        }
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }

    write_index = capacity - 1;
    buffer[write_index] = '\0';
    do {
        uint32 group;

        group = fmt_big_uint_div_small(value, GROUP_BASE);
        if (value->len == 0) {
            do {
                if (write_index == 0) {
                    return -EOVERFLOW;
                }
                write_index -= 1;
                buffer[write_index] = (char)('0' + group%10);
                group /= 10;
            } while (group != 0);
        } else {
            for (int32 i = 0; i < GROUP_DIGITS; i += 1) {
                if (write_index == 0) {
                    return -EOVERFLOW;
                }
                write_index -= 1;
                buffer[write_index] = (char)('0' + group%10);
                group /= 10;
            }
        }
    } while (value->len > 0);

    len = capacity - 1 - write_index;
    memmove64(buffer, buffer + write_index, len + 1);
    return len;
}

static int32
fmt_big_uint_decimal_digit_count(FormatBigUInt *value) {
    enum { GROUP_BASE = 1000000000, GROUP_DIGITS = 9 };
    uint32 top_group;
    int32 group_count;
    int32 top_digits;

    ASSERT(value != NULL);

    if (value->len == 0) {
        return 1;
    }

    group_count = 0;
    do {
        top_group = fmt_big_uint_div_small(value, GROUP_BASE);
        group_count += 1;
    } while (value->len > 0);

    top_digits = 1;
    while (top_group >= 10) {
        top_group /= 10;
        top_digits += 1;
    }
    return (group_count - 1)*GROUP_DIGITS + top_digits;
}

static int32 UNUSED
fmt_binary_float_to_exact_integer(FormatBinaryFloat *parts,
                                  FormatBigUInt *integer) {
    int32 shift;

    ASSERT(parts != NULL);
    ASSERT(integer != NULL);

    fmt_binary_float_copy_significand(parts, integer);
    if (parts->zero) {
        return 0;
    }

    if (parts->binary_exponent >= 0) {
        return fmt_big_uint_shift_left(integer, parts->binary_exponent);
    }

    shift = -parts->binary_exponent;
    if (fmt_big_uint_has_low_bits(integer, shift)) {
        return -ERANGE;
    }
    fmt_big_uint_shift_right(integer, shift);
    return 0;
}

static int32 UNUSED
fmt_binary_float_scaled_decimal(FormatBinaryFloat *parts,
                                int32 decimal_places,
                                FormatBigUInt *integer,
                                enum FormatRemainderHalf *remainder) {
    int32 binary_shift;
    int32 status;

    ASSERT(parts != NULL);
    ASSERT_GE(decimal_places, 0);
    ASSERT(integer != NULL);
    ASSERT(remainder != NULL);

    fmt_binary_float_copy_significand(parts, integer);
    *remainder = FMT_REMAINDER_ZERO;
    if (parts->zero) {
        return 0;
    }

    for (int32 i = 0; i < decimal_places; i += 1) {
        if ((status = fmt_big_uint_mul_small(integer, 5)) < 0) {
            return status;
        }
    }

    binary_shift = parts->binary_exponent + decimal_places;
    if (binary_shift >= 0) {
        return fmt_big_uint_shift_left(integer, binary_shift);
    }

    binary_shift = -binary_shift;
    *remainder = fmt_big_uint_remainder_half(integer, binary_shift);
    fmt_big_uint_shift_right(integer, binary_shift);
    return 0;
}

static int32 UNUSED
fmt_binary_float_set_zero(FormatBinaryFloat *parts, bool negative,
                          int32 precision_bits) {
    ASSERT(parts != NULL);
    ASSERT_GT(precision_bits, 0);

    fmt_binary_float_zero_significand(parts);
    parts->binary_exponent = 0;
    parts->precision_bits = precision_bits;
    parts->negative = negative;
    parts->zero = true;
    return 0;
}

#if FMT_LDOUBLE_SUPPORTED

#if LDBL_MANT_DIG == DBL_MANT_DIG && LDBL_MAX_EXP == DBL_MAX_EXP
static int32
fmt_decode_binary64_ldouble(ldouble value, FormatBinaryFloat *parts) {
    uint64 fraction_mask;
    uint64 exponent_bits;
    uint64 fraction;
    uint64 bits;
    bool negative;

    ASSERT(parts != NULL);
    ASSERT(SIZEOF(ldouble) == SIZEOF(double));

    memcpy64(&bits, &value, SIZEOF(bits));
    negative = (bits >> 63) != 0;
    fraction_mask = (UINT64_C(1)
                     << FMT_LDOUBLE_DOUBLE_FRACTION_BITS) - 1;
    exponent_bits = (bits >> FMT_LDOUBLE_DOUBLE_FRACTION_BITS)
                    & 0x7ff;
    fraction = bits & fraction_mask;

    if (exponent_bits == 0 && fraction == 0) {
        return fmt_binary_float_set_zero(parts, negative, DBL_MANT_DIG);
    }
    if (exponent_bits == 0x7ff) {
        return -EINVAL;
    }

    if (exponent_bits == 0) {
        fmt_binary_float_set_significand_uint64(parts, fraction);
        parts->binary_exponent = 1 - FMT_LDOUBLE_DOUBLE_EXPONENT_BIAS
                                 - FMT_LDOUBLE_DOUBLE_FRACTION_BITS;
    } else {
        uint64 significand =
            (UINT64_C(1) << FMT_LDOUBLE_DOUBLE_FRACTION_BITS) | fraction;

        fmt_binary_float_set_significand_uint64(parts, significand);
        parts->binary_exponent = (int32)exponent_bits
                                 - FMT_LDOUBLE_DOUBLE_EXPONENT_BIAS
                                 - FMT_LDOUBLE_DOUBLE_FRACTION_BITS;
    }
    parts->precision_bits = DBL_MANT_DIG;
    parts->negative = negative;
    parts->zero = false;
    return 0;
}
#endif

#if LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384
static int32
fmt_decode_x87_ldouble(ldouble value, FormatBinaryFloat *parts) {
    uchar bytes[SIZEOF(ldouble)];
    uint64 significand;
    uint32 sign_exp;
    uint32 exponent_bits;
    bool negative;

    ASSERT(parts != NULL);

    if (!fmt_host_is_little_endian() || SIZEOF(ldouble) < 10) {
        return -ENOSYS;
    }

    memcpy64(bytes, &value, SIZEOF(bytes));
    significand = fmt_read_le_uint64(bytes);
    sign_exp = (uint32)bytes[8] | ((uint32)bytes[9] << 8);
    negative = (sign_exp & UINT32_C(0x8000));
    exponent_bits = sign_exp & FMT_LDOUBLE_X87_EXPONENT_MASK;

    if (exponent_bits == 0 && significand == 0) {
        return fmt_binary_float_set_zero(parts, negative, LDBL_MANT_DIG);
    }
    if (exponent_bits == FMT_LDOUBLE_X87_EXPONENT_MASK) {
        return -EINVAL;
    }
    if (exponent_bits != 0
        && (significand & (UINT64_C(1)
                           << FMT_LDOUBLE_X87_FRACTION_BITS)) == 0) {
        return -EINVAL;
    }

    fmt_binary_float_set_significand_uint64(parts, significand);
    if (exponent_bits == 0) {
        parts->binary_exponent = 1 - FMT_LDOUBLE_X87_EXPONENT_BIAS
                                 - FMT_LDOUBLE_X87_FRACTION_BITS;
    } else {
        parts->binary_exponent = (int32)exponent_bits
                                 - FMT_LDOUBLE_X87_EXPONENT_BIAS
                                 - FMT_LDOUBLE_X87_FRACTION_BITS;
    }
    parts->precision_bits = LDBL_MANT_DIG;
    parts->negative = negative;
    parts->zero = false;
    return 0;
}
#endif

#if LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384
static int32
fmt_decode_binary128_ldouble(ldouble value, FormatBinaryFloat *parts) {
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

    memcpy64(bytes, &value, SIZEOF(bytes));
    if (fmt_host_is_little_endian()) {
        low = fmt_read_le_uint64(bytes);
        high = fmt_read_le_uint64(bytes + 8);
    } else {
        high = fmt_read_be_uint64(bytes);
        low = fmt_read_be_uint64(bytes + 8);
    }

    negative = (high >> 63) != 0;
    exponent_bits = (high >> 48) & 0x7fff;
    fraction_high = high & UINT64_C(0x0000ffffffffffff);

    if (exponent_bits == 0 && fraction_high == 0 && low == 0) {
        return fmt_binary_float_set_zero(parts, negative, LDBL_MANT_DIG);
    }
    if (exponent_bits == 0x7fff) {
        return -EINVAL;
    }

    fmt_binary_float_set_significand_uint128(parts, low, fraction_high);
    if (exponent_bits != 0) {
        int32 status;
        int32 fraction_bits = FMT_LDOUBLE_BINARY128_FRACTION_BITS;

        status = fmt_binary_float_set_significand_bit(parts, fraction_bits);
        if (status < 0) {
            return status;
        }
        parts->binary_exponent = (int32)exponent_bits
            - FMT_LDOUBLE_BINARY128_EXPONENT_BIAS
            - FMT_LDOUBLE_BINARY128_FRACTION_BITS;
    } else {
        parts->binary_exponent = 1
            - FMT_LDOUBLE_BINARY128_EXPONENT_BIAS
            - FMT_LDOUBLE_BINARY128_FRACTION_BITS;
    }
    parts->precision_bits = LDBL_MANT_DIG;
    parts->negative = negative;
    parts->zero = false;
    return 0;
}
#endif

static int32 UNUSED
fmt_decompose_ldouble(ldouble value, FormatBinaryFloat *parts) {
    ASSERT(parts != NULL);

    if (!isfinite(value)) {
        return -EINVAL;
    }
    fmt_binary_float_zero_significand(parts);
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
        return fmt_decode_binary64_ldouble(value, parts);
    }
#elif LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384
    return fmt_decode_x87_ldouble(value, parts);
#elif LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384
    return fmt_decode_binary128_ldouble(value, parts);
#else
    return -ENOSYS;
#endif
}

#endif

static bool
fmt_float_is_upper(char conversion) {
    return conversion == 'F' || conversion == 'E' || conversion == 'G'
           || conversion == 'A';
}

static bool
fmt_float_is_fixed(char conversion) {
    return conversion == 'f' || conversion == 'F';
}

static bool
fmt_float_is_exp(char conversion) {
    return conversion == 'e' || conversion == 'E';
}

static bool
fmt_float_is_general(char conversion) {
    return conversion == 'g' || conversion == 'G';
}

static bool
fmt_float_is_hex(char conversion) {
    return conversion == 'a' || conversion == 'A';
}

static int32
fmt_load_float_width_precision(FormatSpec *spec, FormatArgs *args) {
    int32 max_precision;
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(args != NULL);

    if ((status = fmt_load_dynamic_width_precision(spec, args)) < 0) {
        return status;
    }
    if (!fmt_has_precision(spec)) {
        if (fmt_float_is_hex(spec->conversion)) {
            spec->precision = -1;
        } else {
            spec->precision = 6;
            spec->precision_kind = FMT_PRECISION_LITERAL;
        }
    }

    if (spec->length == FMT_LENGTH_BIG_L) {
        max_precision = FMT_LDOUBLE_MAX_DECIMAL_PRECISION;
    } else {
        max_precision = FMT_DOUBLE_MAX_DECIMAL_PRECISION;
    }
    if (spec->precision > max_precision) {
        return -ERANGE;
    }

    return 0;
}


static int32
fmt_float_special_body(double value, FormatSpec *spec,
                       char *body, int32 *body_len) {
    ASSERT(spec != NULL);
    ASSERT(body != NULL);
    ASSERT(body_len != NULL);

    if (isnan(value)) {
        if (fmt_float_is_upper(spec->conversion)) {
            memcpy64(body, "NAN", 3);
        } else {
            memcpy64(body, "nan", 3);
        }
        *body_len = 3;
        return 0;
    }
    if (isinf(value)) {
        if (fmt_float_is_upper(spec->conversion)) {
            memcpy64(body, "INF", 3);
        } else {
            memcpy64(body, "inf", 3);
        }
        *body_len = 3;
        return 0;
    }

    return -EINVAL;
}

static void
fmt_float_uppercase_body(char *body, int32 body_len) {
    ASSERT(body != NULL);
    ASSERT_GE(body_len, 0);

    for (int32 i = 0; i < body_len; i += 1) {
        if (body[i] == 'e') {
            body[i] = 'E';
        }
    }
    return;
}

static int32
fmt_float_force_decimal_point(char *body, int32 body_len, int32 capacity) {
    int32 exponent_index;
    bool found_point;

    ASSERT(body != NULL);
    ASSERT_GE(body_len, 0);
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

    memmove64(body + exponent_index + 1, body + exponent_index,
              (body_len - exponent_index));
    body[exponent_index] = '.';
    return body_len + 1;
}


static int32
fmt_hex_digit_value(char digit) {
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
fmt_hex_digit_char(int32 digit, bool upper) {
    char lower_digits[] = "0123456789abcdef";
    char upper_digits[] = "0123456789ABCDEF";

    ASSERT_GE(digit, 0);
    ASSERT_LE(digit, 15);

    if (upper) {
        return upper_digits[digit];
    }
    return lower_digits[digit];
}

static void
fmt_float_hex_fraction_digits(uint64 fraction, char *digits, bool upper) {
    ASSERT(digits != NULL);

    for (int32 i = 0; i < FMT_DOUBLE_HEX_DIGITS; i += 1) {
        int32 shift;
        int32 digit;

        shift = FMT_DOUBLE_FRACTION_BITS - 4*(i + 1);
        digit = (int32)((fraction >> shift) & 0xf);
        digits[i] = fmt_hex_digit_char(digit, upper);
    }
    return;
}

static bool
fmt_float_hex_has_nonzero_tail(char *digits, int32 start) {
    ASSERT(digits != NULL);
    ASSERT_GE(start, 0);
    ASSERT_LE(start, FMT_DOUBLE_HEX_DIGITS);

    for (int32 i = start; i < FMT_DOUBLE_HEX_DIGITS; i += 1) {
        if (fmt_hex_digit_value(digits[i]) != 0) {
            return true;
        }
    }
    return false;
}

static bool
fmt_float_hex_should_round(char first_digit, char *digits, int32 precision) {
    int32 round_digit;
    int32 even_digit;

    ASSERT(digits != NULL);
    ASSERT_GE(precision, 0);
    ASSERT_LT(precision, FMT_DOUBLE_HEX_DIGITS);

    round_digit = fmt_hex_digit_value(digits[precision]);
    if (round_digit > 8) {
        return true;
    }
    if (round_digit < 8) {
        return false;
    }
    if (fmt_float_hex_has_nonzero_tail(digits, precision + 1)) {
        return true;
    }

    if (precision > 0) {
        even_digit = fmt_hex_digit_value(digits[precision - 1]);
    } else {
        even_digit = fmt_hex_digit_value(first_digit);
    }
    return even_digit & 1;
}

static void
fmt_float_hex_round(char *first_digit, char *digits, int32 precision,
                    bool upper) {
    ASSERT(first_digit != NULL);
    ASSERT(digits != NULL);
    ASSERT_GE(precision, 0);
    ASSERT_LT(precision, FMT_DOUBLE_HEX_DIGITS);

    if (!fmt_float_hex_should_round(*first_digit, digits, precision)) {
        return;
    }

    for (int32 i = precision - 1; i >= 0; i -= 1) {
        int32 digit;

        digit = fmt_hex_digit_value(digits[i]);
        if (digit < 15) {
            digits[i] = fmt_hex_digit_char(digit + 1, upper);
            return;
        }
        digits[i] = '0';
    }

    *first_digit = fmt_hex_digit_char(fmt_hex_digit_value(*first_digit) + 1,
                                      upper);
    return;
}

static int32
fmt_float_hex_trim_digits(char *digits) {
    int32 len;

    ASSERT(digits != NULL);

    len = FMT_DOUBLE_HEX_DIGITS;
    while (len > 0 && digits[len - 1] == '0') {
        len -= 1;
    }
    return len;
}

static int32
fmt_buffer_put(char *buffer, int32 capacity, int32 len, char byte) {
    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT_GE(len, 0);

    if (len >= capacity) {
        return -EOVERFLOW;
    }
    buffer[len] = byte;
    return len + 1;
}

static int32
fmt_buffer_write(char *buffer, int32 capacity, int32 len,
                 char *source, int32 source_len) {
    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT_GE(len, 0);
    ASSERT(source != NULL);
    ASSERT_GE(source_len, 0);

    if (source_len > capacity - len) {
        return -EOVERFLOW;
    }
    memcpy64(buffer + len, source, source_len);
    return len + source_len;
}

static int32
fmt_float_hex_append_exponent(char *buffer, int32 capacity, int32 len,
                              int32 exponent, bool upper) {
    char digits[16];
    uint64 magnitude;
    int32 digit_len;
    int32 status;

    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT_GE(len, 0);

    if (upper) {
        if ((status = fmt_buffer_put(buffer, capacity, len, 'P')) < 0) {
            return status;
        }
    } else {
        if ((status = fmt_buffer_put(buffer, capacity, len, 'p')) < 0) {
            return status;
        }
    }
    len = status;

    if (exponent < 0) {
        magnitude = (uint64)(-(int64)exponent);
        if ((status = fmt_buffer_put(buffer, capacity, len, '-')) < 0) {
            return status;
        }
    } else {
        magnitude = (uint64)exponent;
        if ((status = fmt_buffer_put(buffer, capacity, len, '+')) < 0) {
            return status;
        }
    }
    len = status;

    digit_len = fmt_integer_digits(digits, magnitude, 10, false);
    return fmt_buffer_write(buffer, capacity, len, digits, digit_len);
}

static int32 UNUSED
fmt_float_decimal_append_exponent(char *buffer, int32 capacity,
                                  int32 len, int32 exponent, bool upper) {
    char digits[16];
    uint64 magnitude;
    int32 digit_len;
    int32 status;

    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT_GE(len, 0);

    if (upper) {
        if ((status = fmt_buffer_put(buffer, capacity, len, 'E')) < 0) {
            return status;
        }
    } else {
        if ((status = fmt_buffer_put(buffer, capacity, len, 'e')) < 0) {
            return status;
        }
    }
    len = status;

    if (exponent < 0) {
        magnitude = (uint64)(-(int64)exponent);
        if ((status = fmt_buffer_put(buffer, capacity, len, '-')) < 0) {
            return status;
        }
    } else {
        magnitude = (uint64)exponent;
        if ((status = fmt_buffer_put(buffer, capacity, len, '+')) < 0) {
            return status;
        }
    }
    len = status;

    digit_len = fmt_integer_digits(digits, magnitude, 10, false);
    if (digit_len < 2) {
        if ((status = fmt_buffer_put(buffer, capacity, len, '0')) < 0) {
            return status;
        }
        len = status;
    }

    return fmt_buffer_write(buffer, capacity, len, digits, digit_len);
}

static bool UNUSED
fmt_remainder_should_round(enum FormatRemainderHalf remainder, bool odd) {
    if (remainder == FMT_REMAINDER_MORE_HALF) {
        return true;
    }
    if (remainder == FMT_REMAINDER_HALF && odd) {
        return true;
    }
    return false;
}

static bool UNUSED
fmt_big_uint_is_odd(FormatBigUInt *value) {
    ASSERT(value != NULL);

    return value->len > 0 && (value->words[0] & 1);
}

#if FMT_LDOUBLE_SUPPORTED

static int32
fmt_big_uint_round_half_even(FormatBigUInt *value,
                             enum FormatRemainderHalf remainder) {
    ASSERT(value != NULL);

    if (fmt_remainder_should_round(remainder, fmt_big_uint_is_odd(value))) {
        return fmt_big_uint_add_one(value);
    }
    return 0;
}

static int32
fmt_ldouble_special_body(ldouble value, FormatSpec *spec,
                         char *body, int32 *body_len) {
    ASSERT(spec != NULL);
    ASSERT(body != NULL);
    ASSERT(body_len != NULL);

    if (isnan(value)) {
        if (fmt_float_is_upper(spec->conversion)) {
            memcpy64(body, "NAN", 3);
        } else {
            memcpy64(body, "nan", 3);
        }
        *body_len = 3;
        return 0;
    }
    if (isinf(value)) {
        if (fmt_float_is_upper(spec->conversion)) {
            memcpy64(body, "INF", 3);
        } else {
            memcpy64(body, "inf", 3);
        }
        *body_len = 3;
        return 0;
    }

    return -EINVAL;
}

static char
fmt_ldouble_sign(ldouble value, FormatSpec *spec) {
    ASSERT(spec != NULL);

    if (signbit(value)) {
        return '-';
    }
    if (spec->flags & FMT_FLAG_SIGN) {
        return '+';
    }
    if (spec->flags & FMT_FLAG_SPACE) {
        return ' ';
    }

    return '\0';
}

static int32
fmt_ldouble_format_fixed_digits(char *buffer, int32 capacity,
                                int32 digit_len, int32 precision,
                                bool alternate) {
    int32 integer_len;
    int32 zeros;
    int32 len;

    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT_GT(digit_len, 0);
    ASSERT_GE(precision, 0);

    if (precision == 0) {
        if (!alternate) {
            return digit_len;
        }
        return fmt_buffer_put(buffer, capacity, digit_len, '.');
    }

    integer_len = digit_len - precision;
    if (integer_len > 0) {
        len = digit_len + 1;
        if (len > capacity) {
            return -EOVERFLOW;
        }
        memmove64(buffer + integer_len + 1, buffer + integer_len,
                  digit_len - integer_len);
        buffer[integer_len] = '.';
        return len;
    }

    zeros = 0;
    if (integer_len < 0) {
        zeros = -integer_len;
    }
    len = 2 + zeros + digit_len;
    if (len > capacity) {
        return -EOVERFLOW;
    }

    memmove64(buffer + 2 + zeros, buffer, digit_len);
    buffer[0] = '0';
    buffer[1] = '.';
    for (int32 i = 0; i < zeros; i += 1) {
        buffer[2 + i] = '0';
    }
    return len;
}

static int32
fmt_ldouble_generate_fixed_body(FormatSpec *spec, ldouble value,
                                char *buffer, int32 capacity,
                                FormatBigUInt *integer) {
    enum FormatRemainderHalf remainder;
    FormatBinaryFloat parts;
    int32 digit_len;
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT(integer != NULL);
    ASSERT(fmt_float_is_fixed(spec->conversion));
    ASSERT_GE(spec->precision, 0);

    if ((status = fmt_decompose_ldouble(value, &parts)) < 0) {
        return status;
    }
    if ((status = fmt_binary_float_scaled_decimal(&parts,
                                                  spec->precision,
                                                  integer, &remainder)) < 0) {
        return status;
    }
    if ((status = fmt_big_uint_round_half_even(integer, remainder)) < 0) {
        return status;
    }

    digit_len = fmt_big_uint_to_decimal(integer, buffer, capacity);
    if (digit_len < 0) {
        return digit_len;
    }

    return fmt_ldouble_format_fixed_digits(buffer, capacity, digit_len,
                                           spec->precision,
                                           spec->flags & FMT_FLAG_ALTERNATE);
}

static int32
fmt_decimal_round_digits(char *digits, int32 *len, int32 keep, bool round_up) {
    ASSERT(digits != NULL);
    ASSERT(len != NULL);
    ASSERT_GT(*len, 0);
    ASSERT_GT(keep, 0);
    ASSERT_LE_VAR(keep, *len);

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
    memmove64(digits + 1, digits, keep);
    digits[0] = '1';
    *len = keep + 1;
    return 0;
}

static int32
fmt_ldouble_exp_exponent_small(FormatBinaryFloat *parts,
                               ldouble value, int32 *exponent,
                               FormatBigUInt *integer) {
    enum FormatRemainderHalf remainder;
    ldouble estimate_float;
    int32 estimate;
    int32 digit_len;
    int32 status;

    ASSERT(parts != NULL);
    ASSERT(exponent != NULL);
    ASSERT(integer != NULL);
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
        if ((status = fmt_binary_float_scaled_decimal(parts, -estimate,
                                                      integer,
                                                      &remainder)) < 0) {
            return status;
        }
        if (integer->len == 0) {
            estimate -= 1;
            continue;
        }

        digit_len = fmt_big_uint_decimal_digit_count(integer);
        if (digit_len > 1) {
            estimate += 1;
        } else {
            *exponent = estimate;
            return 0;
        }
    }

    return -ERANGE;
}

static int32
fmt_ldouble_exp_exponent(FormatBinaryFloat *parts,
                         ldouble value, int32 *exponent,
                         FormatBigUInt *integer) {
    enum FormatRemainderHalf remainder;
    int32 digit_len;
    int32 status;

    ASSERT(parts != NULL);
    ASSERT(exponent != NULL);
    ASSERT(integer != NULL);
    ASSERT(value >= 0.0L);

    if (parts->zero) {
        *exponent = 0;
        return 0;
    }
    if (value < 1.0L) {
        return fmt_ldouble_exp_exponent_small(parts, value, exponent, integer);
    }

    if ((status = fmt_binary_float_scaled_decimal(parts, 0,
                                                  integer, &remainder)) < 0) {
        return status;
    }
    digit_len = fmt_big_uint_decimal_digit_count(integer);
    *exponent = digit_len - 1;
    return 0;
}

static int32
fmt_ldouble_exp_digits(FormatBinaryFloat *parts, ldouble value,
                       int32 exponent, int32 significant_len,
                       char *digits, int32 capacity,
                       int32 *digits_len,
                       int32 *decimal_exponent,
                       FormatBigUInt *integer) {
    enum FormatRemainderHalf remainder;
    int32 decimal_places;
    int32 integer_digit_len;
    int32 status;

    ASSERT(parts != NULL);
    ASSERT_GE(significant_len, 0);
    ASSERT(digits != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT(digits_len != NULL);
    ASSERT(decimal_exponent != NULL);
    ASSERT(integer != NULL);

    *decimal_exponent = exponent;
    if (parts->zero) {
        digits[0] = '0';
        *digits_len = 1;
        return 0;
    }

    if (value >= 1.0L) {
        if ((status = fmt_binary_float_scaled_decimal(parts, 0,
                                                      integer,
                                                      &remainder)) < 0) {
            return status;
        }
        integer_digit_len = fmt_big_uint_to_decimal(integer, digits, capacity);
        if (integer_digit_len < 0) {
            return integer_digit_len;
        }
        if (integer_digit_len > significant_len) {
            int32 round_digit;
            bool tail_nonzero;
            bool round_up;

            round_digit = digits[significant_len] - '0';
            tail_nonzero = false;
            for (int32 i = significant_len + 1; i < integer_digit_len;
                 i += 1) {
                if (digits[i] != '0') {
                    tail_nonzero = true;
                    break;
                }
            }
            if (remainder != FMT_REMAINDER_ZERO) {
                tail_nonzero = true;
            }
            if (round_digit > 5) {
                round_up = true;
            } else if (round_digit < 5) {
                round_up = false;
            } else if (tail_nonzero) {
                round_up = true;
            } else {
                round_up = (digits[significant_len - 1] - '0') & 1;
            }
            status = fmt_decimal_round_digits(digits, &integer_digit_len,
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
            int32 last_digit = digits[integer_digit_len - 1] - '0';

            round_up = fmt_remainder_should_round(remainder, last_digit & 1);
            status = fmt_decimal_round_digits(digits, &integer_digit_len,
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
    if ((status = fmt_binary_float_scaled_decimal(parts, decimal_places,
                                                  integer, &remainder)) < 0) {
        return status;
    }
    if ((status = fmt_big_uint_round_half_even(integer, remainder)) < 0) {
        return status;
    }

    integer_digit_len = fmt_big_uint_to_decimal(integer, digits, capacity);
    if (integer_digit_len < 0) {
        return integer_digit_len;
    }
    while (integer_digit_len < significant_len) {
        memmove64(digits + 1, digits, integer_digit_len);
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
fmt_ldouble_format_exp_digits(char *buffer, int32 capacity,
                              int32 digit_len, int32 precision,
                              int32 exponent, bool alternate,
                              bool upper) {
    int32 len;

    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT_GT(digit_len, 0);
    ASSERT_GE(precision, 0);

    if (precision > 0 || alternate) {
        len = 2 + precision;
        if (len > capacity) {
            return -EOVERFLOW;
        }
        if (digit_len > 1) {
            memmove64(buffer + 2, buffer + 1, digit_len - 1);
        }
        buffer[1] = '.';
        for (int32 i = digit_len + 1; i < len; i += 1) {
            buffer[i] = '0';
        }
    } else {
        len = 1;
    }

    return fmt_float_decimal_append_exponent(buffer, capacity, len,
                                             exponent, upper);
}

static int32
fmt_ldouble_generate_exp_body(FormatSpec *spec, ldouble value,
                              char *buffer, int32 capacity,
                              FormatBigUInt *integer) {
    FormatBinaryFloat parts;
    int32 significant_len;
    int32 decimal_exponent;
    int32 exponent;
    int32 digit_len;
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT(integer != NULL);
    ASSERT(fmt_float_is_exp(spec->conversion));
    ASSERT_GE(spec->precision, 0);

    if ((status = fmt_decompose_ldouble(value, &parts)) < 0) {
        return status;
    }
    if ((status = fmt_ldouble_exp_exponent(&parts,
                                           fabsl(value), &exponent,
                                           integer)) < 0) {
        return status;
    }

    significant_len = spec->precision + 1;
    status = fmt_ldouble_exp_digits(&parts, fabsl(value),
                                    exponent, significant_len,
                                    buffer, capacity, &digit_len,
                                    &decimal_exponent, integer);
    if (status == 0) {
        bool alternate = spec->flags & FMT_FLAG_ALTERNATE;
        bool upper = fmt_float_is_upper(spec->conversion);
        status = fmt_ldouble_format_exp_digits(buffer, capacity, digit_len,
                                               spec->precision,
                                               decimal_exponent,
                                               alternate, upper);
    }
    return status;
}


static int32
fmt_ldouble_generate_body(FormatSpec *spec, ldouble value,
                          char *buffer, int32 capacity,
                          FormatBigUInt *integer);

static int32
fmt_ldouble_parse_decimal_exponent(char *body, int32 body_len,
                                   int32 *exponent) {
    int32 index;
    int64 value;
    int32 sign;

    ASSERT(body != NULL);
    ASSERT_GE(body_len, 0);
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
    if (index >= body_len || !is_digit(body[index])) {
        return -EINVAL;
    }

    value = 0;
    while (index < body_len) {
        int32 digit;

        if (!is_digit(body[index])) {
            return -EINVAL;
        }
        digit = fmt_digit_value(body[index]);
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
fmt_ldouble_strip_trailing_zeros(char *body, int32 body_len) {
    int32 exponent_index;
    int32 point_index;
    int32 end;

    ASSERT(body != NULL);
    ASSERT_GE(body_len, 0);

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
        memmove64(body + end, body + exponent_index,
                  (body_len - exponent_index));
        end += body_len - exponent_index;
    }

    return end;
}

static int32
fmt_ldouble_generate_general_body(FormatSpec *spec, ldouble value,
                                  char *buffer, int32 capacity,
                                  FormatBigUInt *integer) {
    FormatSpec work_spec;
    int32 exponent;
    int32 body_len;
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT(integer != NULL);
    ASSERT(fmt_float_is_general(spec->conversion));
    ASSERT(spec->precision >= 1);

    work_spec = *spec;
    if (fmt_float_is_upper(spec->conversion)) {
        work_spec.conversion = 'E';
    } else {
        work_spec.conversion = 'e';
    }
    work_spec.precision = spec->precision - 1;
    body_len = fmt_ldouble_generate_body(&work_spec, value, buffer, capacity,
                                         integer);
    if (body_len < 0) {
        return body_len;
    }

    if ((status = fmt_ldouble_parse_decimal_exponent(buffer, body_len,
                                                     &exponent)) < 0) {
        return status;
    }

    if (exponent >= -4 && exponent < spec->precision) {
        work_spec = *spec;
        if (fmt_float_is_upper(spec->conversion)) {
            work_spec.conversion = 'F';
        } else {
            work_spec.conversion = 'f';
        }
        work_spec.precision = spec->precision - (exponent + 1);
        body_len = fmt_ldouble_generate_body(&work_spec, value,
                                             buffer, capacity, integer);
        if (body_len < 0) {
            return body_len;
        }
    }

    if ((spec->flags & FMT_FLAG_ALTERNATE) == 0) {
        body_len = fmt_ldouble_strip_trailing_zeros(buffer, body_len);
    }

    return body_len;
}

static int32
fmt_ldouble_hex_digit_count(FormatBinaryFloat *parts) {
    int32 fraction_bits;

    ASSERT(parts != NULL);
    ASSERT_GT(parts->precision_bits, 0);

    fraction_bits = parts->precision_bits - 1;
    return (fraction_bits + 3)/4;
}

static void
fmt_ldouble_hex_fraction_digits(FormatBinaryFloat *parts,
                                char *digits, int32 digit_len, bool upper) {
    int32 fraction_bits;

    ASSERT(parts != NULL);
    ASSERT(digits != NULL);
    ASSERT_GE(digit_len, 0);

    fraction_bits = parts->precision_bits - 1;
    for (int32 i = 0; i < digit_len; i += 1) {
        int32 digit;

        digit = 0;
        for (int32 j = 0; j < 4; j += 1) {
            int32 bit_index;

            bit_index = fraction_bits - 1 - i*4 - j;
            if (bit_index >= 0
                && fmt_binary_float_test_significand_bit(parts, bit_index)) {
                digit |= 1 << (3 - j);
            }
        }
        digits[i] = fmt_hex_digit_char(digit, upper);
    }
    return;
}

static bool
fmt_ldouble_hex_has_nonzero_tail(char *digits, int32 start, int32 digit_len) {
    ASSERT(digits != NULL);
    ASSERT_GE(start, 0);
    ASSERT_LE_VAR(start, digit_len);

    for (int32 i = start; i < digit_len; i += 1) {
        if (fmt_hex_digit_value(digits[i]) != 0) {
            return true;
        }
    }
    return false;
}

static bool
fmt_ldouble_hex_should_round(char first_digit,
                             char *digits, int32 digit_len, int32 precision) {
    int32 round_digit;
    int32 even_digit;

    ASSERT(digits != NULL);
    ASSERT_GE(digit_len, 0);
    ASSERT_GE(precision, 0);
    ASSERT_LT_VAR(precision, digit_len);

    round_digit = fmt_hex_digit_value(digits[precision]);
    if (round_digit > 8) {
        return true;
    }
    if (round_digit < 8) {
        return false;
    }
    if (fmt_ldouble_hex_has_nonzero_tail(digits, precision + 1, digit_len)) {
        return true;
    }

    if (precision > 0) {
        even_digit = fmt_hex_digit_value(digits[precision - 1]);
    } else {
        even_digit = fmt_hex_digit_value(first_digit);
    }
    return (even_digit & 1);
}

static void
fmt_ldouble_hex_round(char *first_digit, char *digits,
                      int32 digit_len, int32 precision, bool upper) {
    ASSERT(first_digit != NULL);
    ASSERT(digits != NULL);
    ASSERT_GE(digit_len, 0);
    ASSERT_GE(precision, 0);
    ASSERT_LT_VAR(precision, digit_len);

    if (!fmt_ldouble_hex_should_round(*first_digit,
                                      digits, digit_len, precision)) {
        return;
    }

    for (int32 i = precision - 1; i >= 0; i -= 1) {
        int32 digit;

        digit = fmt_hex_digit_value(digits[i]);
        if (digit < 15) {
            digits[i] = fmt_hex_digit_char(digit + 1, upper);
            return;
        }
        digits[i] = '0';
    }

    *first_digit = fmt_hex_digit_char(fmt_hex_digit_value(*first_digit) + 1,
                                      upper);
    return;
}

static int32
fmt_ldouble_hex_trim_digits(char *digits, int32 digit_len) {
    ASSERT(digits != NULL);
    ASSERT_GE(digit_len, 0);

    while (digit_len > 0 && digits[digit_len - 1] == '0') {
        digit_len -= 1;
    }
    return digit_len;
}

static int32
fmt_ldouble_write_hex_body(FormatSpec *spec, char *buffer,
                           int32 capacity, char first_digit,
                           char *digits, int32 stored_digit_len,
                           int32 output_digit_len,
                           int32 exponent) {
    bool upper;
    int32 len;
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT(digits != NULL);
    ASSERT_GE(stored_digit_len, 0);
    ASSERT_GE(output_digit_len, 0);

    upper = fmt_float_is_upper(spec->conversion);
    len = 0;
    if (upper) {
        if ((status = fmt_buffer_write(buffer, capacity, len, "0X", 2)) < 0) {
            return status;
        }
    } else {
        if ((status = fmt_buffer_write(buffer, capacity, len, "0x", 2)) < 0) {
            return status;
        }
    }
    len = status;

    if ((status = fmt_buffer_put(buffer, capacity, len, first_digit)) < 0) {
        return status;
    }
    len = status;

    if ((output_digit_len > 0) || (spec->flags & FMT_FLAG_ALTERNATE)) {
        if ((status = fmt_buffer_put(buffer, capacity, len, '.')) < 0) {
            return status;
        }
        len = status;
    }

    for (int32 i = 0; i < output_digit_len; i += 1) {
        char digit;

        if (i < stored_digit_len) {
            digit = digits[i];
        } else {
            digit = '0';
        }
        if ((status = fmt_buffer_put(buffer, capacity, len, digit)) < 0) {
            return status;
        }
        len = status;
    }

    return fmt_float_hex_append_exponent(buffer, capacity, len,
                                         exponent, upper);
}

static int32
fmt_ldouble_generate_hex_body(FormatSpec *spec, ldouble value,
                              char *buffer, int32 capacity) {
    FormatBinaryFloat parts;
    char digits[FMT_LDOUBLE_MAX_HEX_DIGITS];
    char first_digit;
    int32 available_digits;
    int32 digit_len;
    int32 exponent;
    int32 status;
    bool upper;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT(fmt_float_is_hex(spec->conversion));

    upper = fmt_float_is_upper(spec->conversion);
    if ((status = fmt_decompose_ldouble(value, &parts)) < 0) {
        return status;
    }

    available_digits = fmt_ldouble_hex_digit_count(&parts);
    if (available_digits > SIZEOF(digits)) {
        return -EOVERFLOW;
    }
    memset64(digits, '0', SIZEOF(digits));

    if (parts.zero) {
        first_digit = '0';
        exponent = 0;
    } else {
        exponent = parts.binary_exponent + parts.precision_bits - 1;
        if (fmt_binary_float_test_significand_bit(&parts,
                                                  parts.precision_bits - 1)) {
            first_digit = '1';
        } else {
            first_digit = '0';
        }
        fmt_ldouble_hex_fraction_digits(&parts,
                                        digits, available_digits, upper);
    }

    if (spec->precision < 0) {
        digit_len = fmt_ldouble_hex_trim_digits(digits, available_digits);
    } else {
        if (spec->precision < available_digits) {
            fmt_ldouble_hex_round(&first_digit, digits,
                                  available_digits, spec->precision,
                                  upper);
        }
        digit_len = spec->precision;
    }

    status = fmt_ldouble_write_hex_body(spec, buffer, capacity,
                                        first_digit, digits,
                                        available_digits, digit_len,
                                        exponent);
    return status;
}

static int32
fmt_ldouble_generate_body(FormatSpec *spec, ldouble value,
                          char *buffer, int32 capacity,
                          FormatBigUInt *integer) {
    int32 body_len;
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT(integer != NULL);

    if (isnan(value) || isinf(value)) {
        status = fmt_ldouble_special_body(value, spec, buffer, &body_len);
        if (status < 0) {
            return status;
        }
        return body_len;
    }

    if (fmt_float_is_general(spec->conversion)) {
        return fmt_ldouble_generate_general_body(spec, value, buffer, capacity,
                                                 integer);
    }
    if (fmt_float_is_hex(spec->conversion)) {
        return fmt_ldouble_generate_hex_body(spec, value, buffer, capacity);
    }
    if (fmt_float_is_fixed(spec->conversion)) {
        return fmt_ldouble_generate_fixed_body(spec, value, buffer, capacity,
                                               integer);
    }
    if (fmt_float_is_exp(spec->conversion)) {
        return fmt_ldouble_generate_exp_body(spec, value,
                                             buffer, capacity, integer);
    }

    return -ENOSYS;
}

#else

static int32 UNUSED
fmt_decompose_ldouble(ldouble value, FormatBinaryFloat *parts) {
    ASSERT(parts != NULL);

    (void)value;
    return -ENOSYS;
}

static char
fmt_ldouble_sign(ldouble value, FormatSpec *spec) {
    ASSERT(spec != NULL);

    (void)value;
    if (spec->flags & FMT_FLAG_SIGN) {
        return '+';
    }
    if (spec->flags & FMT_FLAG_SPACE) {
        return ' ';
    }
    return '\0';
}

static int32
fmt_ldouble_generate_body(FormatSpec *spec, ldouble value,
                          char *buffer, int32 capacity,
                          FormatBigUInt *integer) {
    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT(integer != NULL);

    (void)value;
    (void)integer;
    return -ENOSYS;
}

#endif

static void
fmt_write_float_sign(FormatSink *sink, FormatSpec *spec,
                     char sign, char *body, int32 body_len) {
    int64 zero_pad;
    int64 inner_len;
    int64 spaces;
    int32 prefix_len;

    ASSERT(sink != NULL);
    ASSERT(spec != NULL);
    ASSERT(body != NULL);
    ASSERT_GE(body_len, 0);

    inner_len = body_len;
    if (sign != '\0') {
        inner_len += 1;
    }

    zero_pad = 0;
    if ((spec->flags & FMT_FLAG_ZERO)
        && !(spec->flags & FMT_FLAG_LEFT)) {
        zero_pad = fmt_pad_len(spec->width, inner_len);
    }

    prefix_len = 0;
    if (fmt_float_is_hex(spec->conversion) && body_len >= 2
        && body[0] == '0' && (body[1] == 'x' || body[1] == 'X')) {
        prefix_len = 2;
    }

    spaces = fmt_pad_len(spec->width, inner_len + zero_pad);
    if ((spec->flags & FMT_FLAG_LEFT) == 0) {
        fmt_sink_write_repeat(sink, ' ', spaces);
    }
    if (sign != '\0') {
        fmt_sink_write_byte(sink, sign);
    }
    if (prefix_len > 0) {
        fmt_sink_write(sink, body, prefix_len);
    }
    fmt_sink_write_repeat(sink, '0', zero_pad);
    fmt_sink_write(sink, body + prefix_len, body_len - prefix_len);
    if (spec->flags & FMT_FLAG_LEFT) {
        fmt_sink_write_repeat(sink, ' ', spaces);
    }
    return;
}

static int32
fmt_float_generate_hex_body(FormatSpec *spec, double value,
                            char *buffer, int32 capacity) {
    char digits[FMT_DOUBLE_HEX_DIGITS];
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
    ASSERT_GT(capacity, 0);
    ASSERT(fmt_float_is_hex(spec->conversion));

    upper = fmt_float_is_upper(spec->conversion);
    memcpy64(&bits, &value, SIZEOF(bits));
    fraction_mask = (UINT64_C(1) << FMT_DOUBLE_FRACTION_BITS) - 1;
    exponent_bits = (bits >> FMT_DOUBLE_FRACTION_BITS) & 0x7ff;
    fraction = bits & fraction_mask;

    if (exponent_bits == 0 && fraction == 0) {
        first_digit = '0';
        exponent = 0;
    } else if (exponent_bits == 0) {
        first_digit = '0';
        exponent = FMT_DOUBLE_SUBNORMAL_EXPONENT;
    } else {
        first_digit = '1';
        exponent = (int32)exponent_bits - FMT_DOUBLE_EXPONENT_BIAS;
    }

    fmt_float_hex_fraction_digits(fraction, digits, upper);
    if (spec->precision < 0) {
        digit_len = fmt_float_hex_trim_digits(digits);
    } else {
        ASSERT(spec->precision <= INT32_MAX);
        if (spec->precision < FMT_DOUBLE_HEX_DIGITS) {
            fmt_float_hex_round(&first_digit, digits, spec->precision, upper);
        }
        digit_len = spec->precision;
    }

    len = 0;
    if (upper) {
        if ((status = fmt_buffer_write(buffer, capacity, len, "0X", 2)) < 0) {
            return status;
        }
    } else {
        if ((status = fmt_buffer_write(buffer, capacity, len, "0x", 2)) < 0) {
            return status;
        }
    }
    len = status;

    if ((status = fmt_buffer_put(buffer, capacity, len, first_digit)) < 0) {
        return status;
    }
    len = status;

    if ((digit_len > 0) || (spec->flags & FMT_FLAG_ALTERNATE)) {
        if ((status = fmt_buffer_put(buffer, capacity, len, '.')) < 0) {
            return status;
        }
        len = status;
    }

    for (int32 i = 0; i < digit_len; i += 1) {
        char digit;

        if (i < FMT_DOUBLE_HEX_DIGITS) {
            digit = digits[i];
        } else {
            digit = '0';
        }
        if ((status = fmt_buffer_put(buffer, capacity, len, digit)) < 0) {
            return status;
        }
        len = status;
    }

    return fmt_float_hex_append_exponent(buffer, capacity, len,
                                         exponent, upper);
}

static int32
fmt_float_generate_body(FormatSpec *spec, double value,
                        char *buffer, int32 capacity);

static int32
fmt_float_parse_exponent(char *body, int32 body_len, int32 *exponent) {
    int32 index;
    int64 value;
    int32 sign;

    ASSERT(body != NULL);
    ASSERT_GE(body_len, 0);
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
    if (index >= body_len || !is_digit(body[index])) {
        return -EINVAL;
    }

    value = 0;
    while (index < body_len) {
        int32 digit;

        if (!is_digit(body[index])) {
            return -EINVAL;
        }
        digit = fmt_digit_value(body[index]);
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
fmt_float_strip_trailing_zeros(char *body, int32 body_len) {
    int32 exponent_index;
    int32 point_index;
    int32 end;

    ASSERT(body != NULL);
    ASSERT_GE(body_len, 0);

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
        memmove64(body + end, body + exponent_index,
                  (body_len - exponent_index));
        end += body_len - exponent_index;
    }

    return end;
}

static int32
fmt_float_generate_general_body(FormatSpec *spec, double value,
                                char *buffer, int32 capacity) {
    FormatSpec work_spec;
    int32 exponent;
    int32 body_len;
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);
    ASSERT(fmt_float_is_general(spec->conversion));
    ASSERT(spec->precision >= 1);

    work_spec = *spec;
    if (fmt_float_is_upper(spec->conversion)) {
        work_spec.conversion = 'E';
    } else {
        work_spec.conversion = 'e';
    }
    work_spec.precision = spec->precision - 1;
    body_len = fmt_float_generate_body(&work_spec, value, buffer, capacity);
    if (body_len < 0) {
        return body_len;
    }

    if ((status = fmt_float_parse_exponent(buffer, body_len, &exponent)) < 0) {
        return status;
    }

    if (exponent >= -4 && exponent < spec->precision) {
        work_spec = *spec;
        if (fmt_float_is_upper(spec->conversion)) {
            work_spec.conversion = 'F';
        } else {
            work_spec.conversion = 'f';
        }
        work_spec.precision = spec->precision - (exponent + 1);
        body_len = fmt_float_generate_body(&work_spec, value, buffer, capacity);
        if (body_len < 0) {
            return body_len;
        }
    }

    if ((spec->flags & FMT_FLAG_ALTERNATE) == 0) {
        body_len = fmt_float_strip_trailing_zeros(buffer, body_len);
    }

    return body_len;
}

static int32
fmt_float_generate_body(FormatSpec *spec, double value,
                        char *buffer, int32 capacity) {
    int32 body_len;

    ASSERT(spec != NULL);
    ASSERT(buffer != NULL);
    ASSERT_GT(capacity, 0);

    if (isnan(value) || isinf(value)) {
        int32 status;

        status = fmt_float_special_body(value, spec, buffer, &body_len);
        if (status < 0) {
            return status;
        }
        return body_len;
    }

    if (fmt_float_is_general(spec->conversion)) {
        return fmt_float_generate_general_body(spec, value, buffer, capacity);
    }
    if (fmt_float_is_hex(spec->conversion)) {
        return fmt_float_generate_hex_body(spec, value, buffer, capacity);
    }

    if (fmt_float_is_fixed(spec->conversion)) {
        body_len = d2fixed_buffered_n(value, (uint32)spec->precision, buffer);
    } else {
        body_len = d2exp_buffered_n(value, (uint32)spec->precision, buffer);
    }
    if (body_len <= 0 || body_len >= capacity) {
        return -EOVERFLOW;
    }

    if (buffer[0] == '-') {
        memmove64(buffer, buffer + 1, (body_len - 1));
        body_len -= 1;
    }
    if (spec->flags & FMT_FLAG_ALTERNATE) {
        body_len = fmt_float_force_decimal_point(buffer, body_len, capacity);
        if (body_len < 0) {
            return body_len;
        }
    }
    if (fmt_float_is_upper(spec->conversion)) {
        fmt_float_uppercase_body(buffer, body_len);
    }

    return body_len;
}

static int32
fmt_estimate_add(int64 *total, int64 len) {
    ASSERT(total != NULL);
    ASSERT_GE(len, 0);

    if (len > (INT32_MAX - *total)) {
        return -EOVERFLOW;
    }
    *total += len;
    if (*total > INT32_MAX) {
        return -EOVERFLOW;
    }
    return 0;
}

static int64
fmt_estimate_apply_width(FormatSpec *spec, int64 len) {
    ASSERT(spec != NULL);
    ASSERT_GE(len, 0);

    return MAX(len, spec->width);
}

static int32
fmt_estimate_spec(FormatSpec *spec, FormatArgs *fmt_args, int64 *estimate) {
    int32 status;

    ASSERT(spec != NULL);
    ASSERT(fmt_args != NULL);
    ASSERT(estimate != NULL);

    if (fmt_is_integer_conversion(spec->conversion)) {
        int32 bits;
        int64 digits;
        int64 prefix_len;

        if ((status = fmt_load_dynamic_width_precision(spec, fmt_args)) < 0) {
            return status;
        }

        if (spec->length == FMT_LENGTH_HH
            || spec->length == FMT_LENGTH_W8) {
            bits = 8;
        } else if (spec->length == FMT_LENGTH_H
                   || spec->length == FMT_LENGTH_W16) {
            bits = 16;
        } else if (spec->length == FMT_LENGTH_LL
                   || spec->length == FMT_LENGTH_W64) {
            bits = 64;
        } else {
            ASSERT(spec->length == FMT_LENGTH_NONE
                   || spec->length == FMT_LENGTH_W32);
            bits = 32;
        }

        prefix_len = 0;
        if (fmt_is_signed_integer_conversion(spec->conversion)) {
            prefix_len = 1;
            if (bits == 8) {
                digits = 3;
            } else if (bits == 16) {
                digits = 5;
            } else if (bits == 64) {
                digits = 19;
            } else {
                digits = 10;
            }
        } else if (spec->conversion == 'u') {
            if (bits == 8) {
                digits = 3;
            } else if (bits == 16) {
                digits = 5;
            } else if (bits == 64) {
                digits = 20;
            } else {
                digits = 10;
            }
        } else if (spec->conversion == 'o') {
            digits = (bits + 2)/3;
            if (spec->flags & FMT_FLAG_ALTERNATE) {
                prefix_len = 1;
            }
        } else if (spec->conversion == 'x' || spec->conversion == 'X') {
            digits = (bits + 3)/4;
            if (spec->flags & FMT_FLAG_ALTERNATE) {
                prefix_len = 2;
            }
        } else {
            ASSERT(spec->conversion == 'b' || spec->conversion == 'B');
            digits = bits;
            if (spec->flags & FMT_FLAG_ALTERNATE) {
                prefix_len = 2;
            }
        }

        if (fmt_has_precision(spec) && spec->precision > digits) {
            digits = spec->precision;
        }
        if (fmt_is_signed_integer_conversion(spec->conversion)) {
            if (spec->length == FMT_LENGTH_LL
                || spec->length == FMT_LENGTH_W64) {
                (void)va_arg(fmt_args->args, int64);
            } else {
                (void)va_arg(fmt_args->args, int32);
            }
        } else if (spec->length == FMT_LENGTH_HH
                   || spec->length == FMT_LENGTH_H
                   || spec->length == FMT_LENGTH_W8
                   || spec->length == FMT_LENGTH_W16) {
            (void)va_arg(fmt_args->args, int32);
        } else if (spec->length == FMT_LENGTH_LL
                   || spec->length == FMT_LENGTH_W64) {
            (void)va_arg(fmt_args->args, uint64);
        } else {
            ASSERT(spec->length == FMT_LENGTH_NONE
                   || spec->length == FMT_LENGTH_W32);
            (void)va_arg(fmt_args->args, uint32);
        }
        *estimate = fmt_estimate_apply_width(spec, prefix_len + digits);
        return 0;
    }

    if (spec->conversion == 'c') {
        if ((status = fmt_load_dynamic_width(spec, fmt_args)) < 0) {
            return status;
        }
        (void)va_arg(fmt_args->args, int32);
        *estimate = fmt_estimate_apply_width(spec, 1);
        return 0;
    }

    if (spec->conversion == 's') {
        char *string;
        bool exact_span;

        if ((status = fmt_load_dynamic_width(spec, fmt_args)) < 0) {
            return status;
        }
        exact_span = false;
        if (spec->precision_kind == FMT_PRECISION_ARG) {
            int32 precision = va_arg(fmt_args->args, int32);

            if (precision < 0) {
                return -EINVAL;
            }
            exact_span = true;
            spec->precision = precision;
            spec->precision_kind = FMT_PRECISION_LITERAL;
        }

        string = va_arg(fmt_args->args, char *);
        if (exact_span) {
            *estimate = spec->precision;
            if (string == NULL && *estimate > 0) {
                return -EINVAL;
            }
        } else if (fmt_has_precision(spec)) {
            *estimate = spec->precision;
        } else if (string == NULL) {
            *estimate = strlen32(fmt_null_string);
        } else {
            *estimate = strlen32(string);
        }
        *estimate = fmt_estimate_apply_width(spec, *estimate);
        return 0;
    }

    if (spec->conversion == 'p') {
        if ((status = fmt_load_dynamic_width(spec, fmt_args)) < 0) {
            return status;
        }
        (void)va_arg(fmt_args->args, void *);
        *estimate = 2 + 2*SIZEOF(uintptr);
        *estimate = fmt_estimate_apply_width(spec, *estimate);
        return 0;
    }

    if (spec->conversion == 'n') {
        void *pointer;

        if (spec->length == FMT_LENGTH_HH
            || spec->length == FMT_LENGTH_W8) {
            pointer = va_arg(fmt_args->args, int8 *);
        } else if (spec->length == FMT_LENGTH_H
                   || spec->length == FMT_LENGTH_W16) {
            pointer = va_arg(fmt_args->args, int16 *);
        } else if (spec->length == FMT_LENGTH_LL
                   || spec->length == FMT_LENGTH_W64) {
            pointer = va_arg(fmt_args->args, int64 *);
        } else {
            ASSERT(spec->length == FMT_LENGTH_NONE
                   || spec->length == FMT_LENGTH_W32);
            pointer = va_arg(fmt_args->args, int32 *);
        }
        if (pointer == NULL) {
            return -EINVAL;
        }
        *estimate = 0;
        return 0;
    }

    if (fmt_is_float_conversion(spec->conversion)) {
        int64 capacity64;
        int64 max_capacity;
        int64 prefix;

        if ((status = fmt_load_float_width_precision(spec, fmt_args)) < 0) {
            return status;
        }
        if (fmt_float_is_general(spec->conversion) && spec->precision == 0) {
            spec->precision = 1;
        }

        if (fmt_float_is_fixed(spec->conversion)) {
            if (spec->length == FMT_LENGTH_BIG_L) {
                prefix = FMT_LDOUBLE_MAX_FIXED_PREFIX;
            } else {
                prefix = FMT_FLOAT_MAX_FIXED_PREFIX;
            }
        } else if (fmt_float_is_general(spec->conversion)) {
            prefix = 32;
        } else if (fmt_float_is_hex(spec->conversion)) {
            prefix = 32;
        } else {
            prefix = FMT_FLOAT_MAX_EXP_PREFIX;
        }

        if (spec->precision < 0) {
            if (spec->length == FMT_LENGTH_BIG_L) {
                capacity64 = prefix + FMT_LDOUBLE_MAX_HEX_DIGITS + 8;
            } else {
                capacity64 = prefix + FMT_DOUBLE_HEX_DIGITS + 8;
            }
        } else {
            capacity64 = prefix + spec->precision + 8;
        }

        if (spec->length == FMT_LENGTH_BIG_L) {
            max_capacity = FMT_LDOUBLE_PRINTF_BUFFER_SIZE;
        } else {
            max_capacity = FMT_DOUBLE_PRINTF_BUFFER_SIZE;
        }
        if (capacity64 > max_capacity) {
            return -EOVERFLOW;
        }

        if (spec->length == FMT_LENGTH_BIG_L) {
            (void)va_arg(fmt_args->args, ldouble);
        } else {
            (void)va_arg(fmt_args->args, double);
        }
        *estimate = fmt_estimate_apply_width(spec, capacity64 + 1);
        return 0;
    }

    ASSERT(spec->conversion == '%');
    *estimate = 1;
    return 0;
}

int32 ATTR_PRINTF(2, 0)
fmt_vsnprintf_estimate_plan(FmtPlan *plan, char *format, va_list args) {
    FormatArgs fmt_args;
    char *literal;
    char *cursor;
    int64 total;
    int32 format_len;
    int32 result;
    int32 status;

    if (plan == NULL) {
        return -EINVAL;
    }
    plan->valid = false;
    if (format == NULL) {
        return -EINVAL;
    }

    format_len = strlen32(format);
    if (format_len >= FMT_PLAN_MAX_FORMAT_LEN) {
        return -EOVERFLOW;
    }

    plan->format = format;
    plan->spec_count = 0;
    literal = plan->format;
    cursor = plan->format;
    while (*cursor != '\0') {
        FmtPlanSpec *plan_spec;
        FormatSpec spec;
        char *percent;

        if (*cursor != '%') {
            cursor += 1;
            continue;
        }
        if (plan->spec_count >= FMT_PLAN_MAX_SPECS) {
            return -EOVERFLOW;
        }

        plan_spec = &plan->specs[plan->spec_count];
        percent = cursor;
        cursor += 1;
        if ((status = fmt_parse_spec(cursor, &cursor, &spec)) < 0) {
            return status;
        }

        ASSERT_BETWEEN(literal - plan->format, 0, UINT8_MAX);
        ASSERT_BETWEEN(percent - literal, 0, UINT8_MAX);
        plan_spec->width = spec.width;
        plan_spec->precision = spec.precision;
        plan_spec->literal_offset = (uint8)(literal - plan->format);
        plan_spec->literal_len = (uint8)(percent - literal);
        plan_spec->flags = (uint8)spec.flags;
        plan_spec->width_kind = (uint8)spec.width_kind;
        plan_spec->precision_kind = (uint8)spec.precision_kind;
        plan_spec->length = (uint8)spec.length;
        plan_spec->conversion = spec.conversion;
        plan->spec_count += 1;
        literal = cursor;
    }

    ASSERT_BETWEEN(literal - plan->format, 0, UINT8_MAX);
    ASSERT_BETWEEN(cursor - literal, 0, UINT8_MAX);
    plan->tail_offset = (uint8)(literal - plan->format);
    plan->tail_len = (uint8)(cursor - literal);

    va_copy(fmt_args.args, args);
    total = 0;
    for (int32 i = 0; i < plan->spec_count; i += 1) {
        FmtPlanSpec *plan_spec = &plan->specs[i];
        FormatSpec spec;
        int64 estimate;

        if ((status = fmt_estimate_add(&total, plan_spec->literal_len)) < 0) {
            result = status;
            goto done;
        }

        fmt_plan_load_spec(&spec, plan_spec);
        if ((status = fmt_estimate_spec(&spec, &fmt_args, &estimate)) < 0) {
            result = status;
            goto done;
        }
        if ((status = fmt_estimate_add(&total, estimate)) < 0) {
            result = status;
            goto done;
        }
    }

    if ((status = fmt_estimate_add(&total, plan->tail_len)) < 0) {
        result = status;
        goto done;
    }
    plan->valid = true;
    result = (int32)total;

done:
    va_end(fmt_args.args);
    return result;
}

int32 ATTR_PRINTF(1, 0)
fmt_vsnprintf_estimate(char *format, va_list args) {
    FormatArgs fmt_args;
    char *literal;
    char *cursor;
    int64 total;
    int32 result;
    int32 status;

    if (format == NULL) {
        return -EINVAL;
    }
    ASSERT_LT(strlen32(format), FMT_MAX_FORMAT_LEN);

    va_copy(fmt_args.args, args);
    total = 0;
    literal = format;
    cursor = format;
    while (*cursor != '\0') {
        FormatSpec spec;
        int64 estimate;

        if (*cursor != '%') {
            cursor += 1;
            continue;
        }

        if ((status = fmt_estimate_add(&total, cursor - literal)) < 0) {
            result = status;
            goto done;
        }

        cursor += 1;
        if ((status = fmt_parse_spec(cursor, &cursor, &spec)) < 0) {
            result = status;
            goto done;
        }
        if ((status = fmt_estimate_spec(&spec, &fmt_args, &estimate)) < 0) {
            result = status;
            goto done;
        }
        if ((status = fmt_estimate_add(&total, estimate)) < 0) {
            result = status;
            goto done;
        }
        literal = cursor;
    }

    if ((status = fmt_estimate_add(&total, cursor - literal)) < 0) {
        result = status;
        goto done;
    }
    result = (int32)total;

done:
    va_end(fmt_args.args);
    return result;
}

int32 ATTR_PRINTF(1, 2)
fmt_snprintf_estimate(char *format, ...) {
    va_list args;
    int32 estimate;

    va_start(args, format);
    estimate = fmt_vsnprintf_estimate(format, args);
    va_end(args);

    return estimate;
}

static int32
fmt_execute_spec(FormatSink *sink, FormatSpec *spec, FormatArgs *fmt_args) {
    int32 status;

    ASSERT(sink != NULL);
    ASSERT(spec != NULL);
    ASSERT(fmt_args != NULL);

    if (fmt_is_integer_conversion(spec->conversion)) {
        FormatIntegerValue value;

        if ((status = fmt_load_dynamic_width_precision(spec, fmt_args)) < 0) {
            return status;
        }

        if (fmt_is_signed_integer_conversion(spec->conversion)) {
            value = fmt_read_signed_integer(spec, fmt_args);
        } else {
            value = fmt_read_unsigned_integer(spec, fmt_args);
        }

        fmt_write_integer(sink, spec, value);
        return sink->status;
    }
    if (spec->conversion == 'c') {
        int32 value;
        char byte;

        if ((status = fmt_load_dynamic_width(spec, fmt_args)) < 0) {
            return status;
        }

        value = va_arg(fmt_args->args, int32);
        byte = (char)(uint8)value;
        fmt_write_padded_bytes(sink, spec, &byte, 1);
        return sink->status;
    }
    if (spec->conversion == 's') {
        int64 len;
        char *string;
        bool exact_span;

        if ((status = fmt_load_dynamic_width(spec, fmt_args)) < 0) {
            return status;
        }

        exact_span = false;
        if (spec->precision_kind == FMT_PRECISION_ARG) {
            int32 precision = va_arg(fmt_args->args, int32);

            if (precision < 0) {
                return -EINVAL;
            }

            exact_span = true;
            spec->precision = precision;
            spec->precision_kind = FMT_PRECISION_LITERAL;
        }

        string = va_arg(fmt_args->args, char *);
        if (exact_span) {
            len = spec->precision;
            if (string == NULL && len > 0) {
                return -EINVAL;
            }
        } else {
            if (string == NULL) {
                string = fmt_null_string;
            }
            if (fmt_has_precision(spec)) {
                len = strnlen32(string, spec->precision);
            } else {
                len = strlen32(string);
            }
        }

        fmt_write_padded_bytes(sink, spec, string, len);
        return sink->status;
    }
    if (spec->conversion == 'p') {
        char digits[64];
        char prefix[] = {'0', 'x'};
        uintptr value;
        int32 digit_len;
        int64 inner_len;
        int64 spaces;
        void *pointer;

        if ((status = fmt_load_dynamic_width(spec, fmt_args)) < 0) {
            return status;
        }

        pointer = va_arg(fmt_args->args, void *);
        value = (uintptr)pointer;
        digit_len = fmt_integer_digits(digits, (uint64)value, 16, false);
        inner_len = 2 + digit_len;
        spaces = fmt_pad_len(spec->width, inner_len);

        if ((spec->flags & FMT_FLAG_LEFT) == 0) {
            fmt_sink_write_repeat(sink, ' ', spaces);
        }
        fmt_sink_write(sink, prefix, 2);
        fmt_sink_write(sink, digits, digit_len);
        if (spec->flags & FMT_FLAG_LEFT) {
            fmt_sink_write_repeat(sink, ' ', spaces);
        }
        return sink->status;
    }
    if (spec->conversion == 'n') {
        if (sink->status < 0) {
            return sink->status;
        }
        return fmt_store_count(spec, fmt_args, sink->total, sink->write_count);
    }
    if (fmt_is_float_conversion(spec->conversion)) {
        if (!fmt_float_is_fixed(spec->conversion)
            && !fmt_float_is_exp(spec->conversion)
            && !fmt_float_is_general(spec->conversion)
            && !fmt_float_is_hex(spec->conversion)) {
            return -ENOSYS;
        }

        if ((status = fmt_load_float_width_precision(spec, fmt_args)) < 0) {
            return status;
        }
        if (fmt_float_is_general(spec->conversion) && spec->precision == 0) {
            spec->precision = 1;
        }

        if (spec->length == FMT_LENGTH_BIG_L) {
            char body[FMT_LDOUBLE_PRINTF_BUFFER_SIZE];
            FormatBigUInt integer;
            ldouble value;
            int32 body_len;

            value = va_arg(fmt_args->args, ldouble);
            body_len = fmt_ldouble_generate_body(spec, value, body,
                                                 SIZEOF(body), &integer);
            if (body_len < 0) {
                return body_len;
            }

            fmt_write_float_sign(sink, spec, fmt_ldouble_sign(value, spec),
                                 body, body_len);
            return sink->status;
        } else {
            char body[FMT_DOUBLE_PRINTF_BUFFER_SIZE];
            double value;
            char sign;
            int32 body_len;

            value = va_arg(fmt_args->args, double);
            body_len = fmt_float_generate_body(spec, value, body, SIZEOF(body));
            if (body_len < 0) {
                return body_len;
            }

            if (signbit(value)) {
                sign = '-';
            } else if (spec->flags & FMT_FLAG_SIGN) {
                sign = '+';
            } else if (spec->flags & FMT_FLAG_SPACE) {
                sign = ' ';
            } else {
                sign = '\0';
            }

            fmt_write_float_sign(sink, spec, sign, body, body_len);
            return sink->status;
        }
    }
    if (spec->conversion == '%') {
        fmt_sink_write_byte(sink, '%');
        return sink->status;
    }
    return -ENOSYS;
}

static int32
fmt_vsnprintf_sink(FormatSink *sink, char *format, va_list args) {
    FormatArgs fmt_args;
    char *literal;
    char *cursor;
    int32 result;
    int32 status;

    ASSERT(sink != NULL);

    if (format == NULL) {
        return -EINVAL;
    }
    ASSERT_LT(strlen32(format), FMT_MAX_FORMAT_LEN);

    va_copy(fmt_args.args, args);
    literal = format;
    cursor = format;
    while (*cursor != '\0') {
        FormatSpec spec;

        if (*cursor != '%') {
            cursor += 1;
            continue;
        }

        fmt_sink_write(sink, literal, cursor - literal);
        if (sink->status < 0) {
            result = fmt_sink_finish(sink);
            goto done;
        }

        cursor += 1;
        if ((status = fmt_parse_spec(cursor, &cursor, &spec)) < 0) {
            result = status;
            goto done;
        }
        if ((status = fmt_execute_spec(sink, &spec, &fmt_args)) < 0) {
            result = status;
            goto done;
        }
        literal = cursor;
    }

    fmt_sink_write(sink, literal, cursor - literal);
    result = fmt_sink_finish(sink);

done:
    va_end(fmt_args.args);
    return result;
}

static int32
fmt_vsnprintf_plan_sink(FormatSink *sink, FmtPlan *plan, va_list args) {
    FormatArgs fmt_args;
    int32 result;
    int32 status;

    ASSERT(sink != NULL);

    if (plan == NULL || !plan->valid) {
        return -EINVAL;
    }

    va_copy(fmt_args.args, args);
    for (int32 i = 0; i < plan->spec_count; i += 1) {
        FmtPlanSpec *plan_spec = &plan->specs[i];
        FormatSpec spec;

        fmt_sink_write(sink, plan->format + plan_spec->literal_offset,
                       plan_spec->literal_len);
        if (sink->status < 0) {
            result = fmt_sink_finish(sink);
            goto done;
        }

        fmt_plan_load_spec(&spec, plan_spec);
        if ((status = fmt_execute_spec(sink, &spec, &fmt_args)) < 0) {
            result = status;
            goto done;
        }
    }

    fmt_sink_write(sink, plan->format + plan->tail_offset, plan->tail_len);
    result = fmt_sink_finish(sink);

done:
    va_end(fmt_args.args);
    return result;
}

int32 ATTR_PRINTF(3, 0)
fmt_vsnprintf(char *buffer, int64 capacity, char *format, va_list args) {
    FormatSink sink;
    int32 status;

    if ((status = fmt_sink_init(&sink, buffer, capacity)) < 0) {
        return status;
    }
    return fmt_vsnprintf_sink(&sink, format, args);
}

int32 ATTR_PRINTF(3, 4)
fmt_snprintf(char *buffer, int64 capacity, char *format, ...) {
    va_list args;
    int32 len;

    va_start(args, format);
    len = fmt_vsnprintf(buffer, capacity, format, args);
    va_end(args);
    return len;
}

int32 ATTR_PRINTF(3, 0)
fmt_vsprintf(char *buffer, int64 capacity, char *format, va_list args) {
    FormatSink sink;
    int32 status;

    if (buffer == NULL) {
        return -EINVAL;
    }
    if (capacity <= 0) {
        return -EINVAL;
    }

    if (DEBUGGING) {
        FormatSink check_sink;
        va_list check_args;
        int32 len;

        if ((status = fmt_sink_init(&check_sink, NULL, 0)) < 0) {
            return status;
        }
        check_sink.write_count = false;
        va_copy(check_args, args);
        len = fmt_vsnprintf_sink(&check_sink, format, check_args);
        va_end(check_args);
        if (len < 0) {
            return len;
        }
        if ((int64)len >= capacity) {
            return -ENOSPC;
        }
    }

    if ((status = fmt_sink_init(&sink, buffer, capacity)) < 0) {
        return status;
    }
    sink.unchecked = true;
    return fmt_vsnprintf_sink(&sink, format, args);
}

int32
fmt_vsnprintf_planned(FmtPlan *plan, char *buffer, int64 capacity, va_list args) {
    FormatSink sink;
    int32 status;

    if (plan == NULL || !plan->valid) {
        return -EINVAL;
    }
    if (buffer == NULL) {
        return -EINVAL;
    }
    if (capacity <= 0) {
        return -EINVAL;
    }

    if (DEBUGGING) {
        FormatSink check_sink;
        va_list check_args;
        int32 len;

        if ((status = fmt_sink_init(&check_sink, NULL, 0)) < 0) {
            return status;
        }
        check_sink.write_count = false;
        va_copy(check_args, args);
        len = fmt_vsnprintf_plan_sink(&check_sink, plan, check_args);
        va_end(check_args);
        if (len < 0) {
            return len;
        }
        if ((int64)len >= capacity) {
            return -ENOSPC;
        }
    }

    if ((status = fmt_sink_init(&sink, buffer, capacity)) < 0) {
        return status;
    }
    sink.unchecked = true;
    return fmt_vsnprintf_plan_sink(&sink, plan, args);
}

int32 ATTR_PRINTF(3, 4)
fmt_sprintf(char *buffer, int64 capacity, char *format, ...) {
    va_list args;
    int32 len;

    va_start(args, format);
    len = fmt_vsprintf(buffer, capacity, format, args);
    va_end(args);

    return len;
}

const FmtLocale fmt_locale_c = {
    .weekday_abbr = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
    },
    .weekday = {
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
        "Saturday",
    },
    .month_abbr = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    },
    .month = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December",
    },
    .am = "AM",
    .pm = "PM",
    .am_lower = "am",
    .pm_lower = "pm",
    .date_time_fmt = "%a %b %e %H:%M:%S %Y",
    .date_fmt = "%m/%d/%y",
    .time_fmt = "%H:%M:%S",
    .time_12_fmt = "%I:%M:%S %p",
};

static void
fmt_strftime_write_number(FormatSink *sink, int64 value,
                          int32 width, char pad) {
    char digits[64];
    uint64 magnitude;
    int32 len;
    bool negative;

    negative = value < 0;
    if (negative) {
        magnitude = (uint64)(-(value + 1)) + 1;
    } else {
        magnitude = (uint64)value;
    }
    len = fmt_integer_digits(digits, magnitude, 10, false);

    if (negative) {
        fmt_sink_write_byte(sink, '-');
        width -= 1;
    }
    if (width > len) {
        fmt_sink_write_repeat(sink, pad, width - len);
    }
    fmt_sink_write(sink, digits, len);
    return;
}

static int32
fmt_strftime_days_in_year(int64 year) {
    if (((year%4) == 0)
        && (((year%100) != 0) || ((year%400) == 0))) {
        return 366;
    }
    return 365;
}

static void
fmt_strftime_iso_week(struct tm *time_info,
                      int64 *iso_year, int32 *iso_week) {
    int64 year = 1900 + (int64)time_info->tm_year;
    int32 iso_wday;
    int32 thursday_yday;

    iso_wday = time_info->tm_wday;
    if (iso_wday == 0) {
        iso_wday = 7;
    }

    thursday_yday = time_info->tm_yday + 4 - iso_wday;
    *iso_year = year;
    if (thursday_yday < 0) {
        *iso_year -= 1;
        thursday_yday += fmt_strftime_days_in_year(*iso_year);
    } else if (thursday_yday >= fmt_strftime_days_in_year(year)) {
        thursday_yday -= fmt_strftime_days_in_year(year);
        *iso_year += 1;
    }

    *iso_week = thursday_yday/7 + 1;
    return;
}

static void
fmt_strftime_write_string(FormatSink *sink, const char *string) {
    ASSERT(string != NULL);

    while (*string != '\0') {
        fmt_sink_write_byte(sink, *string);
        string += 1;
    }
    return;
}

static void
fmt_strftime_write_name(FormatSink *sink, const char *const *names,
                        int32 names_len, int32 index) {
    if ((index < 0) || (index >= names_len)) {
        fmt_sink_write_byte(sink, '?');
        return;
    }

    fmt_strftime_write_string(sink, names[index]);
    return;
}

static void
fmt_strftime_format(FormatSink *sink, const char *format,
                    struct tm *time_info, const FmtLocale *locale) {
    const char *cursor;

    ASSERT(format != NULL);
    ASSERT(locale != NULL);

    cursor = format;
    while (*cursor != '\0') {
        char modifier = '\0';
        char conversion;
        int64 year;

        if (*cursor != '%') {
            fmt_sink_write_byte(sink, *cursor);
            cursor += 1;
            continue;
        }

        cursor += 1;
        if (*cursor == 'E' || *cursor == 'O') {
            modifier = *cursor;
            cursor += 1;
        }
        if (*cursor == '\0') {
            fmt_sink_write_byte(sink, '%');
            if (modifier != '\0') {
                fmt_sink_write_byte(sink, modifier);
            }
            break;
        }

        conversion = *cursor;
        cursor += 1;
        year = 1900 + (int64)time_info->tm_year;

        if ((modifier == 'E')
            && (conversion != 'c')
            && (conversion != 'C')
            && (conversion != 'x')
            && (conversion != 'X')
            && (conversion != 'y')
            && (conversion != 'Y')) {
            fmt_sink_write_byte(sink, '%');
            fmt_sink_write_byte(sink, modifier);
            fmt_sink_write_byte(sink, conversion);
            continue;
        }
        if ((modifier == 'O')
            && (conversion != 'd')
            && (conversion != 'e')
            && (conversion != 'H')
            && (conversion != 'I')
            && (conversion != 'm')
            && (conversion != 'M')
            && (conversion != 'S')
            && (conversion != 'u')
            && (conversion != 'U')
            && (conversion != 'V')
            && (conversion != 'w')
            && (conversion != 'W')
            && (conversion != 'y')) {
            fmt_sink_write_byte(sink, '%');
            fmt_sink_write_byte(sink, modifier);
            fmt_sink_write_byte(sink, conversion);
            continue;
        }

        switch (conversion) {
        case 'a':
            fmt_strftime_write_name(sink, locale->weekday_abbr,
                                    LENGTH(locale->weekday_abbr),
                                    time_info->tm_wday);
            break;
        case 'A':
            fmt_strftime_write_name(sink, locale->weekday,
                                    LENGTH(locale->weekday),
                                    time_info->tm_wday);
            break;
        case 'b':
        case 'h':
            fmt_strftime_write_name(sink, locale->month_abbr,
                                    LENGTH(locale->month_abbr),
                                    time_info->tm_mon);
            break;
        case 'B':
            fmt_strftime_write_name(sink, locale->month,
                                    LENGTH(locale->month),
                                    time_info->tm_mon);
            break;
        case 'c':
            fmt_strftime_format(sink, locale->date_time_fmt, time_info, locale);
            break;
        case 'C': {
            int64 century = year/100;
            int64 year_in_century = year%100;

            if (year_in_century < 0) {
                century -= 1;
            }
            fmt_strftime_write_number(sink, century, 2, '0');
            break;
        }
        case 'd':
            fmt_strftime_write_number(sink, time_info->tm_mday, 2, '0');
            break;
        case 'D':
            fmt_strftime_format(sink, "%m/%d/%y", time_info, locale);
            break;
        case 'e':
            fmt_strftime_write_number(sink, time_info->tm_mday, 2, ' ');
            break;
        case 'F':
            fmt_strftime_format(sink, "%Y-%m-%d", time_info, locale);
            break;
        case 'g':
        case 'G': {
            int64 iso_year;
            int32 iso_week;

            fmt_strftime_iso_week(time_info, &iso_year, &iso_week);
            if (conversion == 'G') {
                fmt_strftime_write_number(sink, iso_year, 4, '0');
            } else {
                int64 year_in_century = iso_year%100;

                if (year_in_century < 0) {
                    year_in_century += 100;
                }
                fmt_strftime_write_number(sink, year_in_century, 2, '0');
            }
            break;
        }
        case 'H':
            fmt_strftime_write_number(sink, time_info->tm_hour, 2, '0');
            break;
        case 'I': {
            int32 hour = time_info->tm_hour%12;

            if (hour == 0) {
                hour = 12;
            }
            fmt_strftime_write_number(sink, hour, 2, '0');
            break;
        }
        case 'j':
            fmt_strftime_write_number(sink, time_info->tm_yday + 1, 3, '0');
            break;
        case 'k':
            fmt_strftime_write_number(sink, time_info->tm_hour, 2, ' ');
            break;
        case 'l': {
            int32 hour = time_info->tm_hour%12;

            if (hour == 0) {
                hour = 12;
            }
            fmt_strftime_write_number(sink, hour, 2, ' ');
            break;
        }
        case 'm':
            fmt_strftime_write_number(sink, time_info->tm_mon + 1, 2, '0');
            break;
        case 'M':
            fmt_strftime_write_number(sink, time_info->tm_min, 2, '0');
            break;
        case 'n':
            fmt_sink_write_byte(sink, '\n');
            break;
        case 'p':
            if (time_info->tm_hour < 12) {
                fmt_strftime_write_string(sink, locale->am);
            } else {
                fmt_strftime_write_string(sink, locale->pm);
            }
            break;
        case 'P':
            if (time_info->tm_hour < 12) {
                fmt_strftime_write_string(sink, locale->am_lower);
            } else {
                fmt_strftime_write_string(sink, locale->pm_lower);
            }
            break;
        case 'r':
            fmt_strftime_format(sink, locale->time_12_fmt, time_info, locale);
            break;
        case 'R':
            fmt_strftime_format(sink, "%H:%M", time_info, locale);
            break;
        case 's': {
            int64 epoch_year = year;
            int64 month = 1 + (int64)time_info->tm_mon;
            int64 era;
            int64 year_of_era;
            int64 day_of_year;
            int64 day_of_era;
            int64 days;
            int64 seconds;

            // Gregorian civil-date conversion. A 400-year era has 146097
            // days; 719468 shifts its March-based epoch to Unix day zero.
            epoch_year -= month <= 2;
            if (epoch_year >= 0) {
                era = epoch_year/400;
            } else {
                era = (epoch_year - 399)/400;
            }
            year_of_era = epoch_year - era*400;
            if (month > 2) {
                month -= 3;
            } else {
                month += 9;
            }
            day_of_year = (153*month + 2)/5 + time_info->tm_mday - 1;
            day_of_era = year_of_era*365 + year_of_era/4 - year_of_era/100
                         + day_of_year;
            days = era*146097 + day_of_era - 719468;

            seconds = days*86400;
            seconds += (int64)time_info->tm_hour*3600;
            seconds += (int64)time_info->tm_min*60;
            seconds += time_info->tm_sec;
#if OS_UNIX
            seconds -= (int64)time_info->tm_gmtoff;
#endif
            fmt_strftime_write_number(sink, seconds, 1, '0');
            break;
        }
        case 'S':
            fmt_strftime_write_number(sink, time_info->tm_sec, 2, '0');
            break;
        case 't':
            fmt_sink_write_byte(sink, '\t');
            break;
        case 'T':
            fmt_strftime_format(sink, "%H:%M:%S", time_info, locale);
            break;
        case 'u': {
            int32 weekday = time_info->tm_wday;

            if (weekday == 0) {
                weekday = 7;
            }
            fmt_strftime_write_number(sink, weekday, 1, '0');
            break;
        }
        case 'U': {
            int32 week;

            week = (time_info->tm_yday + 7 - time_info->tm_wday)/7;
            fmt_strftime_write_number(sink, week, 2, '0');
            break;
        }
        case 'V': {
            int64 iso_year;
            int32 iso_week;

            fmt_strftime_iso_week(time_info, &iso_year, &iso_week);
            fmt_strftime_write_number(sink, iso_week, 2, '0');
            break;
        }
        case 'w':
            fmt_strftime_write_number(sink, time_info->tm_wday, 1, '0');
            break;
        case 'W': {
            int32 monday_weekday = (time_info->tm_wday + 6)%7;
            int32 week;

            week = (time_info->tm_yday + 7 - monday_weekday)/7;
            fmt_strftime_write_number(sink, week, 2, '0');
            break;
        }
        case 'x':
            fmt_strftime_format(sink, locale->date_fmt, time_info, locale);
            break;
        case 'X':
            fmt_strftime_format(sink, locale->time_fmt, time_info, locale);
            break;
        case 'y': {
            int64 year_in_century = year%100;

            if (year_in_century < 0) {
                year_in_century += 100;
            }
            fmt_strftime_write_number(sink, year_in_century, 2, '0');
            break;
        }
        case 'Y':
            fmt_strftime_write_number(sink, year, 4, '0');
            break;
        case 'z':
#if OS_UNIX
            if (time_info->tm_isdst >= 0) {
                int64 offset = (int64)time_info->tm_gmtoff;
                int64 minutes;
                int64 hours;

                if (offset < 0) {
                    fmt_sink_write_byte(sink, '-');
                    offset = -offset;
                } else {
                    fmt_sink_write_byte(sink, '+');
                }
                minutes = (offset/60)%60;
                hours = offset/3600;
                fmt_strftime_write_number(sink, hours, 2, '0');
                fmt_strftime_write_number(sink, minutes, 2, '0');
            }
#endif
            break;
        case 'Z':
#if OS_UNIX
            if (time_info->tm_zone != NULL) {
                fmt_strftime_write_string(sink, time_info->tm_zone);
            }
#endif
            break;
        case '%':
            fmt_sink_write_byte(sink, '%');
            break;
        default:
            fmt_sink_write_byte(sink, '%');
            if (modifier != '\0') {
                fmt_sink_write_byte(sink, modifier);
            }
            fmt_sink_write_byte(sink, conversion);
            break;
        }
    }
    return;
}

int32
fmt_strftime_l(char *buffer, int64 capacity, char *format,
               struct tm *time_info, const FmtLocale *locale) {
    FormatSink sink;
    int32 len;

    ASSERT(locale != NULL);

    if ((format == NULL) || (time_info == NULL) || (locale == NULL)) {
        if ((buffer != NULL) && (capacity > 0)) {
            buffer[0] = '\0';
        }
        return 0;
    }
    if ((capacity < 0) || (capacity > INT32_MAX)) {
        return 0;
    }
    if ((capacity > 0) && (buffer == NULL)) {
        return 0;
    }
    if (fmt_sink_init(&sink, buffer, capacity) < 0) {
        return 0;
    }

    fmt_strftime_format(&sink, format, time_info, locale);
    len = fmt_sink_finish(&sink);
    if ((len < 0) || (len >= capacity)) {
        if (capacity > 0) {
            buffer[0] = '\0';
        }
        return 0;
    }
    return len;
}

int32
fmt_strftime(char *buffer, int64 capacity,
             char *format, struct tm *time_info) {
    return fmt_strftime_l(buffer, capacity, format, time_info, &fmt_locale_c);
}

void
str_float64(String *string, double value) {
    int32 len;

    str_reserve(string, FMT_FLOAT_RYU_BUFFER_SIZE);
    len = fmt_float64_shortest(string->data + string->len,
                               string->cap - string->len, value);
    ASSERT_GE(len, 0);
    string->len += len;
    return;
}

void
str_float64_fixed(String *str, double value, int32 precision) {
    int32 len;

    str_reserve(str, FMT_FLOAT_RYU_BUFFER_SIZE);
    len = fmt_float64_fixed(str->data + str->len, str->cap - str->len,
                            value, precision);
    if (len < 0) {
        error("Invalid float precision %d.\n", precision);
        fatal(EXIT_FAILURE);
    }
    str->len += len;

    return;
}

void ATTR_PRINTF(1, 2)
fmt_printf(char *format, ...) {
    char buffer[4096];
    char *big_buffer = NULL;
    char *pbuffer = buffer;
    int64 capacity = SIZEOF(buffer);
    va_list args;
    va_list args_copy;
    int32 n;

    va_start(args, format);
    va_copy(args_copy, args);
    n = fmt_vsnprintf(pbuffer, capacity, format, args);
    va_end(args);

    if (n < 0) {
        va_end(args_copy);
        error2("Error formatting stdout output (n = %d).\n", n);
        fatal(EXIT_FAILURE);
    }

    if (n >= capacity) {
        int32 retry_n;

        capacity = n + 1;
        big_buffer = malloc2(capacity);
        pbuffer = big_buffer;
        retry_n = fmt_vsnprintf(pbuffer, capacity, format, args_copy);
        va_end(args_copy);
        if ((retry_n < 0) || (retry_n != n)) {
            error2("Error formatting stdout output (n = %d).\n", retry_n);
            fatal(EXIT_FAILURE);
        }
        n = retry_n;
    } else {
        va_end(args_copy);
    }

    fflush(stdout);
    write_all(STDOUT_FILENO, pbuffer, n);
    free2(big_buffer, capacity);
    return;
}

#if TESTING_fmt
#define CBASE_IMPLEMENT
#include "cbase.h"
#include "ryu.h"

static int32
fmt_test_snprintf(char *buffer, int64 capacity, char *format, ...) {
    va_list args;
    int32 len;

    va_start(args, format);
    len = fmt_vsnprintf(buffer, capacity, format, args);
    va_end(args);
    return len;
}

static int32
fmt_test_validate(char *format) {
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
        if ((status = fmt_parse_spec(cursor, &cursor, &spec)) < 0) {
            return status;
        }
    }

    return 0;
}

static FormatSpec
fmt_test_parse_one(char *format) {
    FormatSpec spec;
    char *next;

    ASSERT_EQ(format[0], '%');

    next = NULL;
    ASSERT(!fmt_parse_spec(format + 1, &next, &spec));
    ASSERT_EQ(*next, '\0');
    return spec;
}

static void
test_fmt_parser_valid_specs(void) {
    FormatSpec spec;

    spec = fmt_test_parse_one("%%");
    ASSERT_EQ(spec.conversion, '%');
    ASSERT_ZERO(spec.flags);
    ASSERT(spec.length == FMT_LENGTH_NONE);

    spec = fmt_test_parse_one("%08.3d");
    ASSERT_EQ(spec.conversion, 'd');
    ASSERT(spec.flags == FMT_FLAG_ZERO);
    ASSERT(spec.width_kind == FMT_WIDTH_LITERAL);
    ASSERT_EQ(spec.width, 8);
    ASSERT(spec.precision_kind == FMT_PRECISION_LITERAL);
    ASSERT_EQ(spec.precision, 3);
    ASSERT(spec.length == FMT_LENGTH_NONE);

    spec = fmt_test_parse_one("%*.*f");
    ASSERT_EQ(spec.conversion, 'f');
    ASSERT(spec.width_kind == FMT_WIDTH_ARG);
    ASSERT(spec.precision_kind == FMT_PRECISION_ARG);
    ASSERT(spec.length == FMT_LENGTH_NONE);

    spec = fmt_test_parse_one("%-+ #0w32x");
    ASSERT_EQ(spec.conversion, 'x');
    ASSERT(spec.flags == (FMT_FLAG_LEFT
                          |FMT_FLAG_SIGN
                          |FMT_FLAG_SPACE
                          |FMT_FLAG_ALTERNATE
                          |FMT_FLAG_ZERO));
    ASSERT(spec.length == FMT_LENGTH_W32);

    spec = fmt_test_parse_one("%hhd");
    ASSERT_EQ(spec.conversion, 'd');
    ASSERT(spec.length == FMT_LENGTH_HH);

    spec = fmt_test_parse_one("%llu");
    ASSERT_EQ(spec.conversion, 'u');
    ASSERT(spec.length == FMT_LENGTH_LL);

    spec = fmt_test_parse_one("%w64B");
    ASSERT_EQ(spec.conversion, 'B');
    ASSERT(spec.length == FMT_LENGTH_W64);

    spec = fmt_test_parse_one("%La");
    ASSERT_EQ(spec.conversion, 'a');
    ASSERT(spec.length == FMT_LENGTH_BIG_L);

    spec = fmt_test_parse_one("%w16n");
    ASSERT_EQ(spec.conversion, 'n');
    ASSERT(spec.length == FMT_LENGTH_W16);

    ASSERT(!fmt_test_validate("a %% b %08d %*.*s"));
    return;
}

static void
test_fmt_parser_invalid_specs(void) {
    ASSERT_EQ(fmt_test_validate("%"),      -EINVAL);
    ASSERT_EQ(fmt_test_validate("%2$d"),   -EINVAL);
    ASSERT_EQ(fmt_test_validate("%*2$d"),  -EINVAL);
    ASSERT_EQ(fmt_test_validate("%.*2$s"), -EINVAL);
    ASSERT_EQ(fmt_test_validate("%m"),     -EINVAL);
    ASSERT_EQ(fmt_test_validate("%q"),     -EINVAL);
    ASSERT_EQ(fmt_test_validate("%i"),     -EINVAL);

    ASSERT_EQ(fmt_test_validate("%ld"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%lc"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%.5ls"),  -EINVAL);
    ASSERT_EQ(fmt_test_validate("%zd"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%td"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%jd"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%wfd"),   -EINVAL);
    ASSERT_EQ(fmt_test_validate("%wf32d"), -EINVAL);
    ASSERT_EQ(fmt_test_validate("%w24d"),  -EINVAL);
    ASSERT_EQ(fmt_test_validate("%wd"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%Lx"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%lf"),    -EINVAL);

    ASSERT_EQ(fmt_test_validate("%+s"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%05s"),   -EINVAL);
    ASSERT_EQ(fmt_test_validate("%.2c"),   -EINVAL);
    ASSERT_EQ(fmt_test_validate("%#p"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%0p"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%+p"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%.2p"),   -EINVAL);
    ASSERT_EQ(fmt_test_validate("%10n"),   -EINVAL);
    ASSERT_EQ(fmt_test_validate("%-n"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%+n"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%.0n"),   -EINVAL);
    ASSERT_EQ(fmt_test_validate("%ln"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%5%"),    -EINVAL);
    ASSERT_EQ(fmt_test_validate("%.0%"),   -EINVAL);

    ASSERT_EQ(fmt_test_validate("%2147483648d"), -EOVERFLOW);
    ASSERT_EQ(fmt_test_validate("%.2147483648d"), -EOVERFLOW);

    return;
}

static void
test_fmt_sink_cap(char *format, char *expected) {
    char buffer[128];
    int32 expected_len;

    expected_len = strlen32(expected);
    ASSERT_LT(expected_len + 2, SIZEOF(buffer));

    for (int32 capacity = 0; capacity <= expected_len + 2; capacity += 1) {
        int32 copied;
        int32 len;

        memset64(buffer, 0x7f, SIZEOF(buffer));
        len = fmt_test_snprintf(buffer, capacity, format);
        ASSERT_EQ(len, expected_len);

        if (capacity == 0) {
            ASSERT_EQ(buffer[0], (char)0x7f);
            continue;
        }

        copied = MIN(expected_len, capacity - 1);
        ASSERT_EQ(buffer, copied, expected, copied);
        ASSERT_EQ(buffer[copied], '\0');
        ASSERT_EQ(buffer[capacity], (char)0x7f);
    }

    return;
}

static void
test_fmt_integer_cap(char *expected, char *format, ...) {
    char buffer[256];
    int32 expected_len;

    expected_len = strlen32(expected);
    ASSERT_LT(expected_len + 2, SIZEOF(buffer));

    for (int32 capacity = 0; capacity <= expected_len + 2; capacity += 1) {
        va_list args;
        int32 copied;
        int32 len;

        memset64(buffer, 0x7f, SIZEOF(buffer));
        va_start(args, format);
        len = fmt_vsnprintf(buffer, capacity, format, args);
        va_end(args);
        ASSERT_EQ(len, expected_len);

        if (capacity == 0) {
            ASSERT_EQ(buffer[0], (char)0x7f);
            continue;
        }

        copied = MIN(expected_len, capacity - 1);
        ASSERT_EQ(buffer, copied, expected, copied);
        ASSERT_EQ(buffer[copied], '\0');
        ASSERT_EQ(buffer[capacity], (char)0x7f);
    }

    return;
}

static void
test_fmt_bytes_cap(char *expected, int32 expected_len, char *format, ...) {
    char buffer[256];

    ASSERT_GE(expected_len, 0);
    ASSERT_LT(expected_len + 2, SIZEOF(buffer));

    for (int32 capacity = 0; capacity <= expected_len + 2; capacity += 1) {
        va_list args;
        int32 copied;
        int32 len;

        memset64(buffer, 0x7f, SIZEOF(buffer));
        va_start(args, format);
        len = fmt_vsnprintf(buffer, capacity, format, args);
        va_end(args);
        ASSERT_EQ(len, expected_len);

        if (capacity == 0) {
            ASSERT_EQ(buffer[0], (char)0x7f);
            continue;
        }

        copied = MIN(expected_len, capacity - 1);
        ASSERT_EQ(buffer, copied, expected, copied);
        ASSERT_EQ(buffer[copied], '\0');
        ASSERT_EQ(buffer[capacity], (char)0x7f);
    }

    return;
}

static void
test_fmt_integer_outputs(void) {
    test_fmt_integer_cap("0",           "%d", 0);
    test_fmt_integer_cap("-123",        "%d", -123);
    test_fmt_integer_cap("-2147483648", "%d", INT32_MIN);
    test_fmt_integer_cap("4294967295",  "%u", (uint32)UINT32_MAX);
    test_fmt_integer_cap("12",          "%o", (uint32)10);
    test_fmt_integer_cap("abc",         "%x", (uint32)0xabc);
    test_fmt_integer_cap("ABC",         "%X", (uint32)0xabc);
    test_fmt_integer_cap("1010",        "%b", (uint32)10);
    test_fmt_integer_cap("1010",        "%B", (uint32)10);

    test_fmt_integer_cap("+42",      "%+d",    42);
    test_fmt_integer_cap(" 42",      "% d",    42);
    test_fmt_integer_cap("+42",      "%+ d",   42);
    test_fmt_integer_cap("-0000042", "%08d",   -42);
    test_fmt_integer_cap("42    ",   "%-6d",   42);
    test_fmt_integer_cap("   00042", "%8.5d",  42);
    test_fmt_integer_cap("   00042", "%08.5d", 42);
    test_fmt_integer_cap("00042   ", "%-8.5d", 42);
    test_fmt_integer_cap("",         "%.0d",    0);
    test_fmt_integer_cap("     ",    "%5.0d",   0);

    test_fmt_integer_cap("012",      "%#o",    (uint32)10);
    test_fmt_integer_cap("0",        "%#.0o",  (uint32)0);
    test_fmt_integer_cap("    0",    "%#5.0o", (uint32)0);
    test_fmt_integer_cap("012",      "%#.3o",  (uint32)10);
    test_fmt_integer_cap("00012",    "%#.5o",  (uint32)10);
    test_fmt_integer_cap("0x2a",     "%#x",    (uint32)42);
    test_fmt_integer_cap("0X2A",     "%#X",    (uint32)42);
    test_fmt_integer_cap("0b101",    "%#b",    (uint32)5);
    test_fmt_integer_cap("0B101",    "%#B",    (uint32)5);
    test_fmt_integer_cap(" 0x0a",    "%#5.2x", (uint32)10);
    test_fmt_integer_cap("00000012", "%#08o",  (uint32)10);

    test_fmt_integer_cap("42    ",   "%*d",  -6, 42);
    test_fmt_integer_cap("00042",    "%0*d",  5, 42);
    test_fmt_integer_cap("42",       "%.*d", -1, 42);
    test_fmt_integer_cap("00042",    "%.*d",  5, 42);
    test_fmt_integer_cap("   00042", "%*.*d", 8, 5, 42);

    test_fmt_integer_cap("-1 2a 3", "%d %x %u", -1, (uint32)0x2a, (uint32)3);

    test_fmt_integer_cap("18",                   "%hhd", 0x12);
    test_fmt_integer_cap("44",                   "%hhu", 300);
    test_fmt_integer_cap("-1234",                "%hd",  -1234);
    test_fmt_integer_cap("65535",                "%hu",  65535);
    test_fmt_integer_cap("-9223372036854775808", "%lld", INT64_MIN);
    test_fmt_integer_cap("18446744073709551615", "%llu", UINT64_MAX);

    test_fmt_integer_cap("-128",                 "%w8d",  -128);
    test_fmt_integer_cap("255",                  "%w8u",  255);
    test_fmt_integer_cap("65535",                "%w16u", 65535);
    test_fmt_integer_cap("-2147483648",          "%w32d", INT32_MIN);
    test_fmt_integer_cap("4294967295",           "%w32u", UINT32_MAX);
    test_fmt_integer_cap("-9223372036854775808", "%w64d", INT64_MIN);
    test_fmt_integer_cap("18446744073709551615", "%w64u", UINT64_MAX);

    return;
}

static void
test_fmt_null_string_configuration(void) {
    ASSERT_EQ(fmt_null_string, FMT_NULL_STRING);
    fmt_set_null_string("<nil>");
    ASSERT_EQ(fmt_null_string, "<nil>");
    return;
}

static void
test_fmt_char_string_outputs(void) {
    char nul_char_expected[] = {'\0'};
    char span[] = {'a', '\0', 'b', 'c'};
    char span_expected[] = {'a', '\0', 'b', 'c'};
    char span_width_expected[] = {' ', ' ', 'a', '\0', 'b'};
    char span_left_expected[] = {'a', '\0', 'b', ' ', ' '};
    char plain_precision_expected[] = {'a'};
    char spaces[] = {' ', ' ', ' '};
    char buffer[16];

    test_fmt_bytes_cap(STRLIT("A"), "%c", 'A');
    test_fmt_bytes_cap(STRLIT("  A"), "%3c", 'A');
    test_fmt_bytes_cap(STRLIT("A  "), "%-3c", 'A');
    test_fmt_bytes_cap(nul_char_expected, 1, "%c", 0);

    test_fmt_bytes_cap(STRLIT("abc"), "%s", "abc");
    test_fmt_bytes_cap(STRLIT("  abc"), "%5s", "abc");
    test_fmt_bytes_cap(STRLIT("abc  "), "%-5s", "abc");
    test_fmt_bytes_cap(STRLIT("ab"), "%.2s", "abc");
    test_fmt_bytes_cap(STRLIT("   ab"), "%5.2s", "abc");
    test_fmt_bytes_cap(STRLIT("<nil>"), "%s", (char *)NULL);
    test_fmt_bytes_cap(STRLIT("<n"), "%.2s", (char *)NULL);
    test_fmt_bytes_cap(STRLIT("     <ni"), "%8.3s", (char *)NULL);

    test_fmt_bytes_cap(span_expected, 4, "%.*s", 4, span);
    test_fmt_bytes_cap(span_width_expected, 5, "%5.*s", 3, span);
    test_fmt_bytes_cap(span_left_expected, 5, "%-5.*s", 3, span);
    test_fmt_bytes_cap(plain_precision_expected, 1, "%.4s", span);
    test_fmt_bytes_cap("", 0, "%.*s", 0, (char *)NULL);
    test_fmt_bytes_cap(spaces, 3, "%3.*s", 0, (char *)NULL);

    test_fmt_bytes_cap(STRLIT("x=abc n=7 c=Z"), "x=%s n=%d c=%c", "abc", 7, 'Z');

    memset64(buffer, 0x7f, SIZEOF(buffer));
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "%.*s", -1, "abc"),
              -EINVAL);
    ASSERT_EQ(buffer[0], '\0');
    ASSERT_EQ(buffer[1], (char)0x7f);

    memset64(buffer, 0x7f, SIZEOF(buffer));
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer),
                                "%.*s", 1, (char *)NULL),
              -EINVAL);
    ASSERT_EQ(buffer[0], '\0');
    ASSERT_EQ(buffer[1], (char)0x7f);

    return;
}


static void
test_fmt_pointer_count_outputs(void) {
    char buffer[16];
    void *pointer;
    int8 count8;
    int16 count16;
    int32 count32;
    int64 count64;

    pointer = (void *)(uintptr)0x1234;
    test_fmt_bytes_cap("0x0", 3, "%p", (void *)NULL);
    test_fmt_bytes_cap("   0x0", 6, "%6p", (void *)NULL);
    test_fmt_bytes_cap("0x0   ", 6, "%-6p", (void *)NULL);
    test_fmt_bytes_cap("0x1234", 6, "%p", pointer);
    test_fmt_bytes_cap("p=0x1234.", 9, "p=%p.", pointer);
    test_fmt_bytes_cap("   0x0", 6, "%*p", 6, (void *)NULL);
    test_fmt_bytes_cap("0x0   ", 6, "%*p", -6, (void *)NULL);

    count32 = -1;
    test_fmt_bytes_cap("abcd", 4, "ab%ncd", &count32);
    ASSERT_EQ(count32, 2);

    memset64(buffer, 0x7f, SIZEOF(buffer));
    count32 = -1;
    ASSERT_EQ(fmt_test_snprintf(buffer, 2, "abcd%n", &count32), 4);
    ASSERT_EQ(count32, 4);
    ASSERT_EQ(buffer[0], 'a');
    ASSERT_EQ(buffer[1], '\0');
    ASSERT_EQ(buffer[2], (char)0x7f);

    count8 = -1;
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "abc%hhn", &count8), 3);
    ASSERT_EQ(count8, 3);

    count16 = -1;
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "abc%hn", &count16), 3);
    ASSERT_EQ(count16, 3);

    count64 = -1;
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "abc%lln", &count64),
              3);
    ASSERT_EQ(count64, 3);

    count8 = -1;
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "abc%w8n", &count8), 3);
    ASSERT_EQ(count8, 3);

    count16 = -1;
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "abc%w16n", &count16),
              3);
    ASSERT_EQ(count16, 3);

    count32 = -1;
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "abc%w32n", &count32),
              3);
    ASSERT_EQ(count32, 3);

    count64 = -1;
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "abc%w64n", &count64),
              3);
    ASSERT_EQ(count64, 3);

    count8 = -7;
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer),
                                "%128d%hhn", 0, &count8),
              -EOVERFLOW);
    ASSERT_EQ(count8, -7);

    count16 = -7;
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer),
                                "%32768d%hn", 0, &count16),
              -EOVERFLOW);
    ASSERT_EQ(count16, -7);

    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "abc%n", (int32 *)NULL),
              -EINVAL);

    return;
}

static double
fmt_test_double_from_bits(uint64 bits) {
    double value;

    memcpy64(&value, &bits, SIZEOF(value));
    return value;
}

static double
fmt_test_positive_nan(void) {
    return fmt_test_double_from_bits(UINT64_C(0x7ff8000000000000));
}

static double
fmt_test_negative_nan(void) {
    return fmt_test_double_from_bits(UINT64_C(0xfff8000000000000));
}

static void
test_fmt_printf_float_outputs(void) {
    char buffer[64];
    double pos_nan;
    double neg_nan;
    double true_min;

    pos_nan = fmt_test_positive_nan();
    neg_nan = fmt_test_negative_nan();
    true_min = fmt_test_double_from_bits(UINT64_C(1));

    test_fmt_bytes_cap(STRLIT("1.250000"),   "%f",       1.25);
    test_fmt_bytes_cap(STRLIT("1.25"),       "%.2f",     1.25);
    test_fmt_bytes_cap(STRLIT("1"),          "%.0f",     1.25);
    test_fmt_bytes_cap(STRLIT("1."),         "%#.0f",    1.25);
    test_fmt_bytes_cap(STRLIT("  1.25"),     "%6.2f",    1.25);
    test_fmt_bytes_cap(STRLIT("1.25  "),     "%-6.2f",   1.25);
    test_fmt_bytes_cap(STRLIT("0000001.25"), "%010.2f",  1.25);
    test_fmt_bytes_cap(STRLIT("+000001.25"), "%+010.2f", 1.25);
    test_fmt_bytes_cap(STRLIT("-000001.25"), "%010.2f", -1.25);
    test_fmt_bytes_cap(STRLIT(" 1.25"),      "% .2f",    1.25);
    test_fmt_bytes_cap(STRLIT("-0.000"),     "%.3f",    -0.0);

    test_fmt_bytes_cap(STRLIT("1.250000e+00"), "%e",       1.25);
    test_fmt_bytes_cap(STRLIT("1.25e+00"),     "%.2e",     1.25);
    test_fmt_bytes_cap(STRLIT("1e+00"),        "%.0e",     1.25);
    test_fmt_bytes_cap(STRLIT("1.e+00"),       "%#.0e",    1.25);
    test_fmt_bytes_cap(STRLIT("1.25E+00"),     "%.2E",     1.25);
    test_fmt_bytes_cap(STRLIT("+001.25e+00"),  "%+011.2e", 1.25);

    test_fmt_bytes_cap(STRLIT("inf"),      "%f",   HUGE_VAL);
    test_fmt_bytes_cap(STRLIT("-inf"),     "%f",   -HUGE_VAL);
    test_fmt_bytes_cap(STRLIT("+inf"),     "%+f",  HUGE_VAL);
    test_fmt_bytes_cap(STRLIT(" inf"),     "% f",  HUGE_VAL);
    test_fmt_bytes_cap(STRLIT("00000inf"), "%08f", HUGE_VAL);
    test_fmt_bytes_cap(STRLIT("INF"),      "%F",   HUGE_VAL);
    test_fmt_bytes_cap(STRLIT("INF"),      "%E",   HUGE_VAL);
    test_fmt_bytes_cap(STRLIT("nan"),      "%f",   pos_nan);
    test_fmt_bytes_cap(STRLIT("-nan"),     "%f",   neg_nan);
    test_fmt_bytes_cap(STRLIT("NAN"),      "%F",   pos_nan);
    test_fmt_bytes_cap(STRLIT("-NAN"),     "%F",   neg_nan);

    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "%.*f", -1, 1.25), 8);
    ASSERT_EQ(buffer, "1.250000");
    ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                "%.*f",
                                FMT_DOUBLE_MAX_DECIMAL_PRECISION, true_min),
              FMT_DOUBLE_MAX_DECIMAL_PRECISION + 2);
    ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                "%.*e",
                                FMT_DOUBLE_MAX_DECIMAL_PRECISION, 1.0),
              FMT_DOUBLE_MAX_DECIMAL_PRECISION + 6);
    ASSERT_EQ(fmt_test_snprintf(NULL, 0, "%100000f", 1.0), 100000);
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer),
                                "%.1048576f", 1.0), -ERANGE);
    ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                "%.*e",
                                FMT_DOUBLE_MAX_DECIMAL_PRECISION + 1, 1.0),
              -ERANGE);
    ASSERT_EQ(fmt_test_snprintf(NULL, 0, "%.100000e", 1.0), -ERANGE);

    return;
}

static void
test_fmt_printf_general_outputs(void) {
    char buffer[64];

    test_fmt_bytes_cap(STRLIT("1.25"), "%g", 1.25);
    test_fmt_bytes_cap(STRLIT("123456"), "%g", 123456.0);
    test_fmt_bytes_cap(STRLIT("1.23457e+06"), "%g", 1234567.0);
    test_fmt_bytes_cap(STRLIT("0.0001"), "%g", 0.0001);
    test_fmt_bytes_cap(STRLIT("9.9999e-05"), "%g", 0.000099999);
    test_fmt_bytes_cap(STRLIT("1e-05"), "%g", 0.00001);
    test_fmt_bytes_cap(STRLIT("99999.9"), "%g", 99999.9);
    test_fmt_bytes_cap(STRLIT("1e+05"), "%.5g", 99999.9);
    test_fmt_bytes_cap(STRLIT("1e+05"), "%.4g", 99999.0);
    test_fmt_bytes_cap(STRLIT("0.0001"), "%.4g", 0.000099999);
    test_fmt_bytes_cap(STRLIT("1"), "%.0g", 1.25);
    test_fmt_bytes_cap(STRLIT("1."), "%#.0g", 1.25);
    test_fmt_bytes_cap(STRLIT("0.0000"), "%#.5g", 0.0);
    test_fmt_bytes_cap(STRLIT("1.2500"), "%#.5g", 1.25);
    test_fmt_bytes_cap(STRLIT("100.00"), "%#.5g", 100.0);
    test_fmt_bytes_cap(STRLIT("9.9999e-05"), "%#.5g", 0.000099999);
    test_fmt_bytes_cap(STRLIT("      1.25"), "%10.4g", 1.25);
    test_fmt_bytes_cap(STRLIT("0000001.25"), "%010.4g", 1.25);
    test_fmt_bytes_cap(STRLIT("-000000000"), "%010.4g", -0.0);
    test_fmt_bytes_cap(STRLIT("1.25      "), "%-10.4g", 1.25);
    test_fmt_bytes_cap(STRLIT("1.23457E+06"), "%G", 1234567.0);
    test_fmt_bytes_cap(STRLIT("9.99990E-05"), "%#.6G", 0.000099999);
    test_fmt_bytes_cap(STRLIT("INF"), "%G", HUGE_VAL);
    test_fmt_bytes_cap(STRLIT("NAN"), "%G", fmt_test_positive_nan());
    test_fmt_bytes_cap(STRLIT("-NAN"), "%G", fmt_test_negative_nan());
    test_fmt_bytes_cap(STRLIT("nan"), "%g", fmt_test_positive_nan());
    test_fmt_bytes_cap(STRLIT("-nan"), "%g", fmt_test_negative_nan());

    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "%.*g", -1, 1.25), 4);
    ASSERT_EQ(buffer, "1.25");
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "%.*g", 0, 123.0), 5);
    ASSERT_EQ(buffer, "1e+02");
    ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                "%#.*g",
                                FMT_DOUBLE_MAX_DECIMAL_PRECISION, 1.0),
              FMT_DOUBLE_MAX_DECIMAL_PRECISION + 1);
    ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                "%.*g",
                                FMT_DOUBLE_MAX_DECIMAL_PRECISION + 1, 1.0),
              -ERANGE);

    return;
}


static void
test_fmt_printf_hex_float_outputs(void) {
    char buffer[64];
    double lsb_after_one;
    double true_min;
    double normal_min;
    double largest_subnormal;
    double before_one;

    lsb_after_one = fmt_test_double_from_bits(UINT64_C(0x3ff0000000000001));
    true_min = fmt_test_double_from_bits(UINT64_C(0x0000000000000001));
    normal_min = fmt_test_double_from_bits(UINT64_C(0x0010000000000000));
    largest_subnormal = fmt_test_double_from_bits(UINT64_C(0x000fffffffffffff));
    before_one = fmt_test_double_from_bits(UINT64_C(0x3fefffffffffffff));

    test_fmt_bytes_cap(STRLIT("0x0p+0"), "%a", 0.0);
    test_fmt_bytes_cap(STRLIT("-0x0p+0"), "%a", -0.0);
    test_fmt_bytes_cap(STRLIT("0x1p+0"), "%a", 1.0);
    test_fmt_bytes_cap(STRLIT("0x1.8p+0"), "%a", 1.5);
    test_fmt_bytes_cap(STRLIT("0X1.8P+0"), "%A", 1.5);
    test_fmt_bytes_cap(STRLIT("+0x1.8p+0"), "%+a", 1.5);
    test_fmt_bytes_cap(STRLIT(" 0x1.8p+0"), "% a", 1.5);
    test_fmt_bytes_cap(STRLIT("   0x1.8p+0"), "%11a", 1.5);
    test_fmt_bytes_cap(STRLIT("0x1.8p+0   "), "%-11a", 1.5);
    test_fmt_bytes_cap(STRLIT("0x0000001.8p+0"), "%014a", 1.5);
    test_fmt_bytes_cap(STRLIT("+0x000001.8p+0"), "%+014a", 1.5);

    test_fmt_bytes_cap(STRLIT("0x1.0000000000000p+0"), "%.13a", 1.0);
    test_fmt_bytes_cap(STRLIT("0x1.p+0"), "%#.0a", 1.0);
    test_fmt_bytes_cap(STRLIT("0x1.0p+0"), "%.1a", 1.0);
    test_fmt_bytes_cap(STRLIT("0x2p+0"), "%.0a", 1.5);
    test_fmt_bytes_cap(STRLIT("0x1p+0"), "%.0a", 1.25);
    test_fmt_bytes_cap(STRLIT("0x2p-1"), "%.0a", before_one);
    test_fmt_bytes_cap(STRLIT("0x2.0p-1"), "%.1a", before_one);

    test_fmt_bytes_cap(STRLIT("0x1.0000000000001p+0"), "%a", lsb_after_one);
    test_fmt_bytes_cap(STRLIT("0x0.0000000000001p-1022"), "%a", true_min);
    test_fmt_bytes_cap(STRLIT("0x1p-1022"), "%a", normal_min);
    test_fmt_bytes_cap(STRLIT("0x0.fffffffffffffp-1022"), "%a", largest_subnormal);
    test_fmt_bytes_cap(STRLIT("0x1p-1022"), "%.0a", largest_subnormal);
    test_fmt_bytes_cap(STRLIT("0x1.0p-1022"), "%.1a", largest_subnormal);

    test_fmt_bytes_cap(STRLIT("INF"), "%A", HUGE_VAL);
    test_fmt_bytes_cap(STRLIT("NAN"), "%A", fmt_test_positive_nan());
    test_fmt_bytes_cap(STRLIT("-NAN"), "%A", fmt_test_negative_nan());
    test_fmt_bytes_cap(STRLIT("nan"), "%a", fmt_test_positive_nan());
    test_fmt_bytes_cap(STRLIT("-nan"), "%a", fmt_test_negative_nan());

    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "%.*a", -1, 1.5), 8);
    ASSERT_EQ(buffer, "0x1.8p+0");
    ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                "%.*a",
                                FMT_DOUBLE_MAX_DECIMAL_PRECISION, 1.0),
              FMT_DOUBLE_MAX_DECIMAL_PRECISION + 7);
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "%.1048576a", 1.0),
              -ERANGE);
    ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                "%.*a",
                                FMT_DOUBLE_MAX_DECIMAL_PRECISION + 1, 1.0),
              -ERANGE);

    return;
}


static bool
fmt_test_ldouble_supported(void) {
    FormatBinaryFloat parts;
    int32 status;

    status = fmt_decompose_ldouble(1.0L, &parts);
    if (status == -ENOSYS) {
        return false;
    }
    ASSERT_ZERO(status);
    return true;
}

static void
test_fmt_ldouble_parts(ldouble value, bool negative,
                       int32 bit_len, int32 binary_exponent) {
    FormatBinaryFloat parts;

    ASSERT(!fmt_decompose_ldouble(value, &parts));
    ASSERT(parts.negative == negative);
    ASSERT(parts.zero == (bit_len == 0));
    ASSERT_EQ(parts.precision_bits, LDBL_MANT_DIG);
    ASSERT_EQ(fmt_binary_float_significand_bit_len(&parts), bit_len);
    ASSERT_EQ(parts.binary_exponent, binary_exponent);

    return;
}

static void
test_fmt_ldouble_exact_integer(ldouble value, char *expected) {
    char buffer[128];
    FormatBinaryFloat parts;
    FormatBigUInt integer;
    int32 len;

    ASSERT(!fmt_decompose_ldouble(value, &parts));
    ASSERT(!fmt_binary_float_to_exact_integer(&parts, &integer));
    len = fmt_big_uint_to_decimal(&integer, buffer, SIZEOF(buffer));
    ASSERT_EQ(len, strlen32(expected));
    ASSERT_EQ(buffer, expected);
    return;
}

static void
test_fmt_ldouble_scaled(ldouble value, int32 decimal_places,
                        char *expected, enum FormatRemainderHalf expected_rem) {
    char buffer[128];
    FormatBinaryFloat parts;
    FormatBigUInt integer;
    enum FormatRemainderHalf remainder;
    int32 len;

    ASSERT(!fmt_decompose_ldouble(value, &parts));
    ASSERT(!fmt_binary_float_scaled_decimal(&parts, decimal_places,
                                            &integer, &remainder));
    ASSERT(remainder == expected_rem);
    len = fmt_big_uint_to_decimal(&integer, buffer, SIZEOF(buffer));
    ASSERT_EQ(len, strlen32(expected));
    ASSERT_EQ(buffer, expected);
    return;
}

static void
test_fmt_ldouble_decomposition(void) {
    FormatBinaryFloat parts;
    FormatBigUInt integer;
    ldouble true_min;
    int32 expected_exp;

    if (!fmt_test_ldouble_supported()) {
        return;
    }

    ASSERT_EQ(fmt_decompose_ldouble((ldouble)INFINITY, &parts), -EINVAL);
    ASSERT_EQ(fmt_decompose_ldouble((ldouble)NAN, &parts), -EINVAL);

    test_fmt_ldouble_parts(0.0L, false, 0, 0);
    test_fmt_ldouble_parts(-0.0L, true, 0, 0);
    test_fmt_ldouble_parts(1.0L, false, LDBL_MANT_DIG, 1 - LDBL_MANT_DIG);
    test_fmt_ldouble_parts(-1.0L, true, LDBL_MANT_DIG, 1 - LDBL_MANT_DIG);
    test_fmt_ldouble_parts(0.5L, false, LDBL_MANT_DIG, -LDBL_MANT_DIG);
    test_fmt_ldouble_parts(2.0L, false, LDBL_MANT_DIG, 2 - LDBL_MANT_DIG);

    expected_exp = LDBL_MIN_EXP - LDBL_MANT_DIG;
    test_fmt_ldouble_parts(LDBL_MIN, false, LDBL_MANT_DIG, expected_exp);

    expected_exp = LDBL_MAX_EXP - LDBL_MANT_DIG;
    test_fmt_ldouble_parts(LDBL_MAX, false, LDBL_MANT_DIG, expected_exp);
    ASSERT(!fmt_decompose_ldouble(LDBL_MAX, &parts));
    ASSERT(!fmt_binary_float_to_exact_integer(&parts, &integer));
    ASSERT_EQ(fmt_big_uint_bit_len(&integer), LDBL_MAX_EXP);

    true_min = ldexpl(1.0L, LDBL_MIN_EXP - LDBL_MANT_DIG);
    if (true_min != 0.0L) {
        test_fmt_ldouble_parts(true_min, false, 1,
                               LDBL_MIN_EXP - LDBL_MANT_DIG);
    }

    return;
}

static void
test_fmt_ldouble_decimal_helpers(void) {
    FormatBinaryFloat parts;
    FormatBigUInt integer;

    if (!fmt_test_ldouble_supported()) {
        return;
    }

    test_fmt_ldouble_exact_integer(0.0L, "0");
    test_fmt_ldouble_exact_integer(1.0L, "1");
    test_fmt_ldouble_exact_integer(2.0L, "2");
    test_fmt_ldouble_exact_integer(ldexpl(1.0L, 64), "18446744073709551616");

    ASSERT(!fmt_decompose_ldouble(0.5L, &parts));
    ASSERT_EQ(fmt_binary_float_to_exact_integer(&parts, &integer), -ERANGE);

    test_fmt_ldouble_scaled(0.125L, 3, "125", FMT_REMAINDER_ZERO);
    test_fmt_ldouble_scaled(0.25L, 0, "0", FMT_REMAINDER_LESS_HALF);
    test_fmt_ldouble_scaled(0.5L, 0, "0", FMT_REMAINDER_HALF);
    test_fmt_ldouble_scaled(0.75L, 0, "0", FMT_REMAINDER_MORE_HALF);
    test_fmt_ldouble_scaled(1.25L, 1, "12", FMT_REMAINDER_HALF);
    return;
}

static void
test_fmt_printf_ldouble_outputs(void) {
    char buffer[128];
    ldouble precise;
    ldouble true_min;

    test_fmt_bytes_cap(STRLIT("1.250000"), "%Lf", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("1.25"), "%.2Lf", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("1"), "%.0Lf", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("1."), "%#.0Lf", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("  1.25"), "%6.2Lf", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("1.25  "), "%-6.2Lf", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("0000001.25"), "%010.2Lf", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("+000001.25"), "%+010.2Lf", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("-000001.25"), "%010.2Lf", (ldouble)-1.25);
    test_fmt_bytes_cap(STRLIT("-0.000"), "%.3Lf", (ldouble)-0.0L);
    test_fmt_bytes_cap(STRLIT("0.125"), "%.3Lf", 0.125L);
    test_fmt_bytes_cap(STRLIT("2"), "%.0Lf", 1.5L);
    test_fmt_bytes_cap(STRLIT("2"), "%.0Lf", 2.5L);

    test_fmt_bytes_cap(STRLIT("1.250000e+00"), "%Le", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("1.25e+00"), "%.2Le", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("1e+00"), "%.0Le", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("1.e+00"), "%#.0Le", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("1.25E+00"), "%.2LE", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("+001.25e+00"), "%+011.2Le", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("1.250e-03"), "%.3Le", 0.00125L);
    test_fmt_bytes_cap(STRLIT("1.00e+03"), "%.2Le", 999.9L);

    test_fmt_bytes_cap(STRLIT("inf"), "%Lf", (ldouble)INFINITY);
    test_fmt_bytes_cap(STRLIT("-inf"), "%Lf", (ldouble)-INFINITY);
    test_fmt_bytes_cap(STRLIT("INF"), "%LF", (ldouble)INFINITY);
    test_fmt_bytes_cap(STRLIT("INF"), "%LE", (ldouble)INFINITY);

    if (fmt_test_ldouble_supported() && LDBL_MANT_DIG > DBL_MANT_DIG) {
        precise = ldexpl(1.0L, 63) + 1.0L;
        test_fmt_bytes_cap("9223372036854775809", 19, "%.0Lf", precise);
    }

    test_fmt_bytes_cap(STRLIT("1"), "%Lg", (ldouble)1.0);
    test_fmt_bytes_cap(STRLIT("1.25"), "%Lg", (ldouble)1.25);
    test_fmt_bytes_cap(STRLIT("1.23457e+06"), "%Lg", 1234567.0L);
    test_fmt_bytes_cap(STRLIT("0.0001"), "%Lg", 0.0001L);
    test_fmt_bytes_cap(STRLIT("1e-05"), "%Lg", 0.00001L);
    test_fmt_bytes_cap(STRLIT("1."), "%#.0Lg", 1.25L);
    test_fmt_bytes_cap(STRLIT("1.2500"), "%#.5Lg", 1.25L);
    test_fmt_bytes_cap(STRLIT("      1.25"), "%10.4Lg", 1.25L);
    test_fmt_bytes_cap(STRLIT("0000001.25"), "%010.4Lg", 1.25L);
    test_fmt_bytes_cap(STRLIT("1.250E+00"), "%.3LE", 1.25L);
    test_fmt_bytes_cap(STRLIT("1.25"), "%.3LG", 1.25L);
    test_fmt_bytes_cap(STRLIT("INF"), "%LG", (ldouble)INFINITY);
    test_fmt_bytes_cap(STRLIT("inf"), "%Lg", (ldouble)INFINITY);

    test_fmt_bytes_cap(STRLIT("0x0p+0"), "%La", 0.0L);
    test_fmt_bytes_cap(STRLIT("-0x0p+0"), "%La", (ldouble)-0.0L);
    test_fmt_bytes_cap(STRLIT("0x1p+0"), "%La", 1.0L);
    test_fmt_bytes_cap(STRLIT("0x1.8p+0"), "%La", 1.5L);
    test_fmt_bytes_cap(STRLIT("0X1.8P+0"), "%LA", 1.5L);
    test_fmt_bytes_cap(STRLIT("+0x1.8p+0"), "%+La", 1.5L);
    test_fmt_bytes_cap(STRLIT("0x0000001.8p+0"), "%014La", 1.5L);
    test_fmt_bytes_cap(STRLIT("0x1.0000p+0"), "%.4La", 1.0L);
    test_fmt_bytes_cap(STRLIT("0x1.p+0"), "%#.0La", 1.0L);
    test_fmt_bytes_cap(STRLIT("0x2p+0"), "%.0La", 1.5L);
    test_fmt_bytes_cap(STRLIT("INF"), "%LA", (ldouble)INFINITY);
    test_fmt_bytes_cap(STRLIT("inf"), "%La", (ldouble)INFINITY);

    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer),
                                "%.*Lf", -1, (ldouble)1.25),
              8);
    ASSERT_EQ(buffer, "1.250000");

    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer),
                                "%.*Lg", -1, (ldouble)1.25),
              4);
    ASSERT_EQ(buffer, "1.25");
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer),
                                "%.*La", -1, (ldouble)1.5),
              8);
    ASSERT_EQ(buffer, "0x1.8p+0");

    if (fmt_test_ldouble_supported()) {
        true_min = ldexpl(1.0L, LDBL_MIN_EXP - LDBL_MANT_DIG);
        if (true_min != 0.0L) {
            int32 precision = FMT_LDOUBLE_MAX_DECIMAL_PRECISION;

            ASSERT_EQ(fmt_test_snprintf(NULL, 0, "%.*Lf", precision, true_min),
                      FMT_LDOUBLE_MAX_DECIMAL_PRECISION + 2);
            ASSERT_GT(fmt_test_snprintf(NULL, 0, "%.*Le", precision, true_min),
                      0);
        }
        ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                    "%.*Lf",
                                    FMT_LDOUBLE_MAX_DECIMAL_PRECISION,
                                    LDBL_MAX),
                  FMT_LDOUBLE_MAX_DECIMAL_PRECISION + LDBL_MAX_10_EXP + 2);
        ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                    "%.*Le",
                                    FMT_LDOUBLE_MAX_DECIMAL_PRECISION, 0.0L),
                  FMT_LDOUBLE_MAX_DECIMAL_PRECISION + 6);
        ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                    "%#.*Lg",
                                    FMT_LDOUBLE_MAX_DECIMAL_PRECISION, 0.0L),
                  FMT_LDOUBLE_MAX_DECIMAL_PRECISION + 1);
        ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                    "%.*La",
                                    FMT_LDOUBLE_MAX_DECIMAL_PRECISION, 0.0L),
                  FMT_LDOUBLE_MAX_DECIMAL_PRECISION + 7);
        ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                    "%.*Lf",
                                    FMT_LDOUBLE_MAX_DECIMAL_PRECISION + 1,
                                    1.0L),
                  -ERANGE);
        ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                    "%.*Le",
                                    FMT_LDOUBLE_MAX_DECIMAL_PRECISION + 1,
                                    1.0L),
                  -ERANGE);
        ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                    "%.*Lg",
                                    FMT_LDOUBLE_MAX_DECIMAL_PRECISION + 1,
                                    1.0L),
                  -ERANGE);
        ASSERT_EQ(fmt_test_snprintf(NULL, 0,
                                    "%.*La",
                                    FMT_LDOUBLE_MAX_DECIMAL_PRECISION + 1,
                                    1.0L),
                  -ERANGE);
    }

    return;
}

static int32
fmt_test_public_vsnprintf(char *buffer, int64 capacity, char *format, ...) {
    va_list args;
    int32 len;

    va_start(args, format);
    len = fmt_vsnprintf(buffer, capacity, format, args);
    va_end(args);
    return len;
}

static int32
fmt_test_public_estimate(char *format, ...) {
    va_list args;
    int32 estimate;

    va_start(args, format);
    estimate = fmt_vsnprintf_estimate(format, args);
    va_end(args);

    return estimate;
}

static int32
fmt_test_public_vsprintf(char *buffer, int64 capacity, char *format, ...) {
    va_list args;
    int32 len;

    va_start(args, format);
    len = fmt_vsprintf(buffer, capacity, format, args);
    va_end(args);

    return len;
}

static int32
fmt_test_planned_estimate(FmtPlan *plan, char *format, ...) {
    va_list args;
    int32 estimate;

    va_start(args, format);
    estimate = fmt_vsnprintf_estimate_plan(plan, format, args);
    va_end(args);

    return estimate;
}

static int32
fmt_test_planned_sprintf(FmtPlan *plan, char *buffer, int64 capacity, ...) {
    va_list args;
    int32 len;

    va_start(args, capacity);
    len = fmt_vsnprintf_planned(plan, buffer, capacity, args);
    va_end(args);
    return len;
}

static void
test_fmt_planned_plan(void) {
    char format[] = "x=%d s=%.*s f=%g";
    char span[] = {'a', '\0', 'b', 'c'};
    char buffer[128];
    FmtPlan plan = {0};
    int32 estimate;
    int32 len;
    int32 count;

    estimate = fmt_test_planned_estimate(&plan, format, 7, 4, span, 1.25);
    ASSERT_GT(estimate, 0);
    ASSERT(plan.valid);
    ASSERT(plan.format == format);

    len = fmt_test_planned_sprintf(&plan, buffer, SIZEOF(buffer),
                                   7, 4, span, 1.25);
    ASSERT_EQ(len, 17);
    ASSERT_EQ(buffer, len + 1, "x=7 s=a\0bc f=1.25", 18);

    estimate = fmt_test_planned_estimate(&plan, "%*.*f", 10, 2, 1.25);
    ASSERT_GE_VAR(estimate, 10);
    len = fmt_test_planned_sprintf(&plan, buffer, SIZEOF(buffer), 8, 3, 1.25);
    ASSERT_EQ(len, 8);
    ASSERT_EQ(buffer, "   1.250");
    len = fmt_test_planned_sprintf(&plan, buffer, SIZEOF(buffer), 0, 1, 1.26);
    ASSERT_EQ(len, 3);
    ASSERT_EQ(buffer, "1.3");
    ASSERT_EQ(fmt_test_planned_sprintf(&plan, buffer, SIZEOF(buffer), 0,
                                       FMT_DOUBLE_MAX_DECIMAL_PRECISION + 1,
                                       1.0),
              -ERANGE);

    estimate = fmt_test_planned_estimate(&plan, "[%s]", (char *)NULL);
    ASSERT_EQ(estimate, 7);
    len = fmt_test_planned_sprintf(&plan, buffer, SIZEOF(buffer), (char *)NULL);
    ASSERT_EQ(len, 7);
    ASSERT_EQ(buffer, "[<nil>]");

    count = -1;
    estimate = fmt_test_planned_estimate(&plan, "ab%ncd", &count);
    ASSERT_EQ(estimate, 4);
    ASSERT_EQ(count, -1);
    len = fmt_test_planned_sprintf(&plan, buffer, SIZEOF(buffer), &count);
    ASSERT_EQ(len, 4);
    ASSERT_EQ(count, 2);
    ASSERT_EQ(buffer, "abcd");

    ASSERT_EQ(fmt_test_planned_estimate(&plan, "%"), -EINVAL);
    ASSERT(!plan.valid);
    ASSERT_EQ(fmt_test_planned_sprintf(&plan, buffer, SIZEOF(buffer)), -EINVAL);
    ASSERT_EQ(fmt_test_planned_estimate(NULL, "%d", 1), -EINVAL);

    {
        char max_format[FMT_PLAN_MAX_FORMAT_LEN];

        for (int32 i = 0; i < FMT_PLAN_MAX_SPECS; i += 1) {
            max_format[2*i] = '%';
            max_format[2*i + 1] = '%';
        }
        max_format[2*FMT_PLAN_MAX_SPECS] = 'x';
        max_format[2*FMT_PLAN_MAX_SPECS + 1] = '\0';

        estimate = fmt_test_planned_estimate(&plan, max_format);
        ASSERT_EQ(estimate, FMT_PLAN_MAX_SPECS + 1);
        ASSERT(plan.valid);
        ASSERT(plan.format == max_format);
        ASSERT_EQ(plan.spec_count, FMT_PLAN_MAX_SPECS);
    }

    {
        char too_long[FMT_PLAN_MAX_FORMAT_LEN + 1];

        for (int32 i = 0; i < FMT_PLAN_MAX_FORMAT_LEN; i += 1) {
            too_long[i] = 'x';
        }
        too_long[FMT_PLAN_MAX_FORMAT_LEN] = '\0';

        ASSERT_EQ(fmt_test_planned_estimate(&plan, too_long), -EOVERFLOW);
        ASSERT(!plan.valid);
    }

    return;
}

static void
test_fmt_strftime(void) {
    char buffer[512];
    char tiny[4];
    struct tm time_info = {0};
    int32 len;

    time_info.tm_year = 122;
    time_info.tm_mon = 0;
    time_info.tm_mday = 2;
    time_info.tm_hour = 3;
    time_info.tm_min = 4;
    time_info.tm_sec = 5;
    time_info.tm_wday = 0;
    time_info.tm_yday = 1;
    time_info.tm_isdst = 0;
#if OS_UNIX
    time_info.tm_gmtoff = 0;
    time_info.tm_zone = "UTC";
#endif

    len = fmt_strftime(buffer, SIZEOF(buffer),
                       "%a|%A|%b|%B|%c|%C|%d|%D|%e|%F|%g|%G|%h|%H|%I|%j|"
                       "%k|%l|%m|%M|%p|%P|%r|%R|%s|%S|%T|%u|%U|%V|%w|%W|"
                       "%x|%X|%y|%Y|%%", &time_info);
    ASSERT_EQ(buffer,
              "Sun|Sunday|Jan|January|Sun Jan  2 03:04:05 2022|20|02|"
              "01/02/22| 2|2022-01-02|21|2021|Jan|03|03|002| 3| 3|01|"
              "04|AM|am|03:04:05 AM|03:04|1641092645|05|03:04:05|7|01|"
              "52|0|00|01/02/22|03:04:05|22|2022|%");
    ASSERT_EQ(len, strlen32(buffer));

    len = fmt_strftime(buffer, SIZEOF(buffer), "%n%t", &time_info);
    ASSERT_EQ(len, 2);
    ASSERT_EQ(buffer, 3, "\n\t", 3);

    len = fmt_strftime(buffer, SIZEOF(buffer),
                       "%Ec|%EC|%Ex|%EX|%Ey|%EY|%Od|%Oe|%OH|%OI|%Om|%OM|"
                       "%OS|%Ou|%OU|%OV|%Ow|%OW|%Oy",
                       &time_info);
    ASSERT_EQ(buffer,
              "Sun Jan  2 03:04:05 2022|20|01/02/22|03:04:05|22|2022|02|"
              " 2|03|03|01|04|05|7|01|52|0|00|22");
    ASSERT_EQ(len, strlen32(buffer));

    len = fmt_strftime(buffer, SIZEOF(buffer), "%Ea|%Ob|%Q|%", &time_info);
    ASSERT_EQ(buffer, "%Ea|%Ob|%Q|%");
    ASSERT_EQ(len, 12);

    time_info.tm_year = 121;
    time_info.tm_mon = 0;
    time_info.tm_mday = 1;
    time_info.tm_wday = 5;
    time_info.tm_yday = 0;
    len = fmt_strftime(buffer, SIZEOF(buffer), "%G-%V-%g", &time_info);
    ASSERT_EQ(len, 10);
    ASSERT_EQ(buffer, "2020-53-20");

    time_info.tm_year = 118;
    time_info.tm_mon = 11;
    time_info.tm_mday = 31;
    time_info.tm_wday = 1;
    time_info.tm_yday = 364;
    len = fmt_strftime(buffer, SIZEOF(buffer), "%G-%V-%g", &time_info);
    ASSERT_EQ(len, 10);
    ASSERT_EQ(buffer, "2019-01-19");

    time_info.tm_year = 122;
    time_info.tm_mon = 0;
    time_info.tm_mday = 2;
    time_info.tm_hour = 3;
    time_info.tm_min = 4;
    time_info.tm_sec = 5;
    time_info.tm_wday = 0;
    time_info.tm_yday = 1;
    time_info.tm_isdst = 0;
#if OS_UNIX
    time_info.tm_gmtoff = -3*3600 - 30*60;
    time_info.tm_zone = "TEST";
    len = fmt_strftime(buffer, SIZEOF(buffer), "%z|%Z|%s", &time_info);
    ASSERT_EQ(buffer, "-0330|TEST|1641105245");
    ASSERT_EQ(len, strlen32(buffer));
#endif

    ASSERT_EQ(fmt_strftime(buffer, 5, "%Y", &time_info), 4);
    ASSERT_EQ(buffer, "2022");
    ASSERT_ZERO(fmt_strftime(tiny, SIZEOF(tiny), "%Y", &time_info));
    ASSERT_EQ(tiny, "");
    ASSERT_ZERO(fmt_strftime(NULL, 0, "%Y", &time_info));
    ASSERT_ZERO(fmt_strftime(buffer, SIZEOF(buffer), "", &time_info));
    ASSERT_EQ(buffer, "");
    ASSERT_ZERO(fmt_strftime(buffer, SIZEOF(buffer), NULL, &time_info));
    ASSERT_EQ(buffer, "");
    ASSERT_ZERO(fmt_strftime(buffer, SIZEOF(buffer), "%Y", NULL));
    ASSERT_EQ(buffer, "");

    return;
}

static void
test_fmt_strftime_locale(void) {
    static const FmtLocale locale = {
        .weekday_abbr = {
            "d0", "d1", "d2", "d3", "d4", "d5", "d6",
        },
        .weekday = {
            "day0", "day1", "day2", "day3", "day4", "day5", "day6",
        },
        .month_abbr = {
            "m01", "m02", "m03", "m04", "m05", "m06",
            "m07", "m08", "m09", "m10", "m11", "m12",
        },
        .month = {
            "month01", "month02", "month03", "month04", "month05",
            "month06", "month07", "month08", "month09", "month10",
            "month11", "month12",
        },
        .am = "before-noon",
        .pm = "after-noon",
        .am_lower = "before-noon-lower",
        .pm_lower = "after-noon-lower",
        .date_time_fmt = "%A/%B/%e/%Y/%X",
        .date_fmt = "%Y.%m.%d",
        .time_fmt = "%Hh%Mm%Ss",
        .time_12_fmt = "%Ih%Mm%Ss %p",
    };
    char buffer[512];
    struct tm time_info = {0};
    int32 len;

    time_info.tm_year = 122;
    time_info.tm_mon = 0;
    time_info.tm_mday = 2;
    time_info.tm_hour = 15;
    time_info.tm_min = 4;
    time_info.tm_sec = 5;
    time_info.tm_wday = 0;
    time_info.tm_yday = 1;
    time_info.tm_isdst = 0;

    len = fmt_strftime_l(buffer, SIZEOF(buffer),
                         "%a|%A|%b|%B|%p|%P|%c|%x|%X|%r",
                         &time_info, &locale);
    ASSERT_EQ(buffer,
              "d0|day0|m01|month01|after-noon|after-noon-lower|"
              "day0/month01/ 2/2022/15h04m05s|2022.01.02|15h04m05s|"
              "03h04m05s after-noon");
    ASSERT_EQ(len, strlen32(buffer));

    len = fmt_strftime_l(buffer, SIZEOF(buffer),
                         "%Ec|%Ex|%EX",
                         &time_info, &locale);
    ASSERT_EQ(buffer, "day0/month01/ 2/2022/15h04m05s|2022.01.02|15h04m05s");
    ASSERT_EQ(len, strlen32(buffer));

    len = fmt_strftime_l(buffer, SIZEOF(buffer),
                         "%A|%B|%p|%P|%x|%X",
                         &time_info, &fmt_locale_c);
    ASSERT_EQ(buffer, "Sunday|January|PM|pm|01/02/22|15:04:05");
    ASSERT_EQ(len, strlen32(buffer));

    return;
}

static void
test_fmt_public_api(void) {
    char buffer[32];
    char tiny[4];
    int32 len;
    int32 count;

    len = fmt_snprintf(buffer, SIZEOF(buffer), "public:%d:%s", 42, "ok");
    ASSERT_EQ(len, 12);
    ASSERT_EQ(buffer, len + 1, "public:42:ok", 13);

    ASSERT_LE_VAR(SIZEOF(buffer), fmt_snprintf_estimate("%g", 1.0));
    len = fmt_sprintf(buffer, SIZEOF(buffer), "%g", 1.0);
    ASSERT_EQ(len, 1);
    ASSERT_EQ(buffer, "1");

    len = fmt_snprintf(tiny, SIZEOF(tiny), "abcdef");
    ASSERT_EQ(len, 6);
    ASSERT_EQ(tiny, SIZEOF(tiny), "abc", 4);

    len = fmt_snprintf(NULL, 0, "abcdef");
    ASSERT_EQ(len, 6);

    len = fmt_test_public_vsnprintf(buffer, SIZEOF(buffer),
                                    "%s:%.*s", NULL, 3, "a\0b");
    ASSERT_EQ(len, 9);
    ASSERT_EQ(buffer, len + 1, "<nil>:a\0b", 10);

    count = -1;
    len = fmt_snprintf(tiny, SIZEOF(tiny), "abcd%n", &count);
    ASSERT_EQ(len, 4);
    ASSERT_EQ(count, 4);
    ASSERT_EQ(tiny, SIZEOF(tiny), "abc", 4);

    len = fmt_sprintf(buffer, SIZEOF(buffer), "fast:%d:%s", 42, "ok");
    ASSERT_EQ(len, 10);
    ASSERT_EQ(buffer, len + 1, "fast:42:ok", 11);

    len = fmt_test_public_vsprintf(buffer, SIZEOF(buffer),
                                   "%s:%.*s", NULL, 3, "a\0b");
    ASSERT_EQ(len, 9);
    ASSERT_EQ(buffer, len + 1, "<nil>:a\0b", 10);

    count = -1;
    len = fmt_sprintf(buffer, SIZEOF(buffer), "abcd%n", &count);
    ASSERT_EQ(len, 4);
    ASSERT_EQ(count, 4);
    ASSERT_EQ(buffer, "abcd");

    ASSERT_EQ(fmt_sprintf(tiny, SIZEOF(tiny), "abcdef"), -ENOSPC);
    ASSERT_EQ(fmt_test_public_vsprintf(tiny, SIZEOF(tiny), "abcdef"), -ENOSPC);

    {
        char max_format[FMT_MAX_FORMAT_LEN];
        char max_output[FMT_MAX_FORMAT_LEN];

        for (int32 i = 0; i < FMT_MAX_FORMAT_LEN - 1; i += 1) {
            max_format[i] = 'x';
        }
        max_format[FMT_MAX_FORMAT_LEN - 1] = '\0';

        len = fmt_test_public_vsnprintf(max_output, SIZEOF(max_output),
                                        max_format);
        ASSERT_EQ(len, FMT_MAX_FORMAT_LEN - 1);
        ASSERT_EQ(max_output, len + 1, max_format, FMT_MAX_FORMAT_LEN);
        ASSERT_EQ(fmt_test_public_estimate(max_format), FMT_MAX_FORMAT_LEN - 1);
    }

    return;
}

static void
test_fmt_estimate(void) {
    char span[] = {'a', '\0', 'b', 'c'};
    char buffer[64];
    int32 estimate;
    int32 exact;
    int32 count;

    ASSERT_EQ(fmt_test_public_estimate("abc"), 3);
    ASSERT_EQ(fmt_snprintf_estimate("%d", 0), 11);
    ASSERT_EQ(fmt_snprintf_estimate("%lld", (int64)0), 20);
    ASSERT_EQ(fmt_snprintf_estimate("%#b", 0), 34);
    ASSERT_EQ(fmt_snprintf_estimate("%100d", 0), 100);
    ASSERT_EQ(fmt_snprintf_estimate("%p", (void *)NULL),
              (2 + 2*SIZEOF(uintptr)));

    ASSERT_EQ(fmt_snprintf_estimate("%s", "abc"), 3);
    ASSERT_EQ(fmt_snprintf_estimate("%s", (char *)NULL), 5);
    ASSERT_EQ(fmt_snprintf_estimate("%.10s", "abc"), 10);
    ASSERT_EQ(fmt_snprintf_estimate("%.*s", 4, span), 4);
    ASSERT_EQ(fmt_snprintf_estimate("%5.*s", 3, span), 5);
    ASSERT_EQ(fmt_snprintf_estimate("%.*s", -1, "abc"), -EINVAL);
    ASSERT_EQ(fmt_snprintf_estimate("%.*s", 1, (char *)NULL), -EINVAL);

    ASSERT_EQ(fmt_snprintf_estimate("%f", 1.0),
              FMT_FLOAT_MAX_FIXED_PREFIX + 6 + 8 + 1);
    ASSERT_EQ(fmt_snprintf_estimate("%20.2f", 1.0),
              FMT_FLOAT_MAX_FIXED_PREFIX + 2 + 8 + 1);
    ASSERT_EQ(fmt_snprintf_estimate("%.*e",
                                    FMT_DOUBLE_MAX_DECIMAL_PRECISION + 1, 1.0),
              -ERANGE);

    count = -1;
    ASSERT_EQ(fmt_snprintf_estimate("ab%ncd", &count), 4);
    ASSERT_EQ(count, -1);
    ASSERT_EQ(fmt_snprintf_estimate("abc%n", (int32 *)NULL), -EINVAL);

    estimate = fmt_snprintf_estimate("x=%d s=%.*s f=%g", 7, 4, span, 1.25);
    exact = fmt_test_snprintf(buffer, SIZEOF(buffer),
                              "x=%d s=%.*s f=%g", 7, 4, span, 1.25);
    ASSERT_GE_VAR(estimate, exact);

    return;
}

static void
test_fmt_sink_validation(void) {
    char buffer[8];
    FormatSink sink;

    ASSERT_EQ(fmt_test_snprintf(NULL, 0, "abc"), 3);
    ASSERT_EQ(fmt_test_snprintf(NULL, 1, "abc"), -EINVAL);
    ASSERT_EQ(fmt_test_snprintf(buffer, -1, "abc"), -EINVAL);
    ASSERT_EQ(fmt_test_snprintf(buffer, (int64)INT32_MAX + 1, "abc"),
              -EOVERFLOW);
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), NULL), -EINVAL);
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "%*d", INT32_MIN, 0),
              -EOVERFLOW);

    memset64(buffer, 0x7f, SIZEOF(buffer));
    memset64(buffer, 0x7f, SIZEOF(buffer));
    ASSERT_EQ(fmt_test_snprintf(buffer, SIZEOF(buffer), "%"), -EINVAL);
    ASSERT_EQ(buffer[0], '\0');
    ASSERT_EQ(buffer[1], (char)0x7f);

    ASSERT(!fmt_sink_init(&sink, buffer, SIZEOF(buffer)));
    fmt_sink_add_total(&sink, INT32_MAX);
    fmt_sink_add_total(&sink, 1);
    ASSERT_EQ(fmt_sink_finish(&sink), -EOVERFLOW);
    ASSERT_EQ(fmt_sink_init(&sink, buffer, (int64)INT32_MAX + 1), -EOVERFLOW);

    return;
}

static void
test_fmt_float32_shortest(float value, char *expected) {
    char buffer[FMT_FLOAT_RYU_BUFFER_SIZE];
    int32 len = fmt_float32_shortest(buffer, SIZEOF(buffer), value);
    ASSERT_EQ(buffer, len, expected);
    return;
}

static void
test_fmt_float64_shortest(double value, char *expected) {
    char buffer[FMT_FLOAT_RYU_BUFFER_SIZE];
    int32 len = fmt_float64_shortest(buffer, SIZEOF(buffer), value);
    ASSERT_EQ(buffer, len, expected);
    return;
}

static void
test_fmt_float64_fixed(double value, int32 precision, char *expected) {
    char buffer[FMT_FLOAT_RYU_BUFFER_SIZE];
    int32 len = fmt_float64_fixed(buffer, SIZEOF(buffer), value, precision);
    ASSERT_EQ(buffer, len, expected);
    return;
}

static void
test_fmt_float64_exp(double val, int32 precision, char *expected) {
    char buffer[FMT_FLOAT_RYU_BUFFER_SIZE];
    int32 len = fmt_float64_exp(buffer, SIZEOF(buffer), val, precision);
    ASSERT_EQ(buffer, len, expected);
    return;
}

static uint32
test_fmt_float32_bits(float value) {
    uint32 bits;
    memcpy64(&bits, &value, SIZEOF(bits));
    return bits;
}

static uint64
test_fmt_float64_bits(double value) {
    uint64 bits;
    memcpy64(&bits, &value, SIZEOF(bits));
    return bits;
}

static void
test_fmt_float32_round_trip(float value) {
    char buffer[FMT_FLOAT_RYU_BUFFER_SIZE];
    char *end;
    float parsed;
    int32 len;

    len = fmt_float32_shortest(buffer, SIZEOF(buffer), value);
    ASSERT_GT(len, 0);

    end = NULL;
    parsed = strtof(buffer, &end);
    ASSERT(end == buffer + len);
    ASSERT_EQ(test_fmt_float32_bits(parsed), test_fmt_float32_bits(value));

    return;
}

static void
test_fmt_float64_round_trip(double value) {
    char buffer[FMT_FLOAT_RYU_BUFFER_SIZE];
    char *end;
    double parsed;
    int32 len;

    len = fmt_float64_shortest(buffer, SIZEOF(buffer), value);
    ASSERT_GT(len, 0);

    end = NULL;
    parsed = strtod(buffer, &end);
    ASSERT(end == buffer + len);
    ASSERT_EQ(test_fmt_float64_bits(parsed), test_fmt_float64_bits(value));

    return;
}

int
main(void) {
    char buffer[16];

    test_fmt_null_string_configuration();
    test_fmt_sink_cap("", "");
    test_fmt_sink_cap("abc", "abc");
    test_fmt_sink_cap("%%", "%");
    test_fmt_sink_cap("a%%b%%c", "a%b%c");
    test_fmt_sink_cap("abc%%def", "abc%def");
    test_fmt_parser_valid_specs();
    test_fmt_parser_invalid_specs();
    test_fmt_integer_outputs();
    test_fmt_char_string_outputs();
    test_fmt_pointer_count_outputs();
    test_fmt_printf_float_outputs();
    test_fmt_printf_general_outputs();
    test_fmt_printf_hex_float_outputs();
    test_fmt_printf_ldouble_outputs();
    test_fmt_ldouble_decomposition();
    test_fmt_ldouble_decimal_helpers();
    test_fmt_strftime();
    test_fmt_strftime_locale();
    test_fmt_public_api();
    test_fmt_planned_plan();
    test_fmt_estimate();
    test_fmt_sink_validation();

    test_fmt_float64_shortest(0.0, "0E0");
    test_fmt_float64_shortest(-0.0, "-0E0");
    test_fmt_float64_shortest(1.0, "1E0");
    test_fmt_float64_shortest(0.1, "1E-1");
    test_fmt_float64_shortest(1234567.89, "1.23456789E6");
    test_fmt_float64_shortest(1e-7, "1E-7");

    test_fmt_float32_shortest(0.0f, "0E0");
    test_fmt_float32_shortest(-0.0f, "-0E0");
    test_fmt_float32_shortest(1.0f, "1E0");
    test_fmt_float32_shortest(0.1f, "1E-1");
    test_fmt_float32_shortest(1e-7f, "1E-7");

    test_fmt_float64_fixed(1.25, 2, "1.25");
    test_fmt_float64_fixed(1.2, 4, "1.2000");
    test_fmt_float64_fixed(-0.0, 3, "-0.000");

    test_fmt_float64_exp(1234.0, 2, "1.23e+03");
    test_fmt_float64_exp(0.00123, 3, "1.230e-03");

    {
        String builder = {0};

        STR_APPEND(&builder, "x=");
        str_float64(&builder, 0.1);
        STR_APPEND(&builder, " y=");
        str_float64_fixed(&builder, 1.25, 2);
        ASSERT_EQ(builder.data, "x=1E-1 y=1.25");
        str_free(&builder);
    }

    ASSERT_EQ(fmt_float64_shortest(NULL, 64, 1.0), -EINVAL);
    ASSERT_EQ(fmt_float64_shortest(buffer, 0, 1.0), -EINVAL);
    ASSERT_EQ(fmt_float64_fixed(buffer, SIZEOF(buffer), 1.0, -1), -EINVAL);
    ASSERT_EQ(fmt_float64_fixed(buffer, 4, 1.25, 2), -ENOSPC);
    ASSERT_EQ(fmt_float64_fixed(buffer, SIZEOF(buffer), 1.0,
                                FMT_DOUBLE_MAX_DECIMAL_PRECISION + 1),
              -ERANGE);

    test_fmt_float64_round_trip(0.1);
    test_fmt_float32_round_trip(0.1f);

    exit(EXIT_SUCCESS);
}
#endif

#endif /* FMT_C */
