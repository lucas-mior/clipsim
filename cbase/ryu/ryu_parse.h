// Copyright 2019 Ulf Adams
//
// The contents of this file may be used under the terms of the Apache License,
// Version 2.0.
//
//    (See accompanying file LICENSE-Apache or copy at
//     http://www.apache.org/licenses/LICENSE-2.0)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.
#ifndef RYU_PARSE_H
#define RYU_PARSE_H

#include "../primitives.h"

#ifdef __cplusplus
extern "C" {
#endif

// Parses decimal strings to binary64 with correct round-to-nearest-even
// conversion. Long decimal inputs use fixed-size integer arithmetic rather
// than an arbitrarily growing integer or a libc decimal parser. The entire
// input can be scanned, but only 800 significant digits and a sticky bit are
// retained: this exceeds the precision of every binary64 rounding midpoint.
// Hexadecimal floating-point input uses C %a/%A syntax. Not all strtod
// formats are supported.

enum Status {
  SUCCESS,
  INPUT_TOO_SHORT,
  INPUT_TOO_LONG,
  MALFORMED_INPUT,
  FLOAT_UNDERFLOW
};

// Parses a double prefix. Decimal input and hexadecimal %a/%A input are
// supported, along with inf, infinity, and nan. A leading + or - is accepted.
// Hexadecimal input requires a 0x/0X prefix and p/P binary exponent. On
// success, returns the number of input bytes consumed and stores the converted
// value in *result. A negative return value is an error: -INPUT_TOO_SHORT,
// -INPUT_TOO_LONG, -MALFORMED_INPUT, or -FLOAT_UNDERFLOW. Numeric overflow
// is a successful conversion: it stores signed infinity and returns the number
// of bytes consumed. Explicit inf/infinity and nan tokens are also successful.
// Numeric underflow is reported for every nonzero input below the normal double
// range, including exact subnormals, and stores the resulting signed zero or
// subnormal value.
//
// s2d_n reads at most len bytes and does not require a nul terminator. Parsing
// stops before the first byte that is not part of a valid floating-point
// token. s2d parses a nul-terminated string with the same prefix semantics.
int32 s2d_n(const char *buffer, int32 len, double *result);
int32 s2d(const char *buffer, double *result);

// Like s2d_n, but optimized for a readable, nul-terminated buffer. Ordinary
// decimal input is scanned without checking a length on each character.
// The caller must ensure a terminating nul byte is accessible. This function
// has the same prefix, rounding, and error semantics as s2d_n.
int32 s2d_fast(const char *buffer, double *result);

enum Status s2f_n(const char * buffer, const int len, float * result);
enum Status s2f(const char * buffer, float * result);

#ifdef __cplusplus
}
#endif

#endif // RYU_PARSE_H
