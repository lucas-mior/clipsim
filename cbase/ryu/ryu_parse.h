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

// This is an experimental implementation of parsing strings to 64-bit floats.
// Decimal conversion uses a Ryu-like algorithm and currently supports up to 17
// non-zero digits. Hexadecimal floating-point input uses the C %a/%A syntax.
// Not all strtod formats are supported. Use at your own risk.

enum Status {
  SUCCESS,
  INPUT_TOO_SHORT,
  INPUT_TOO_LONG,
  MALFORMED_INPUT
};

// Parses a double prefix. Decimal input and hexadecimal %a/%A input are
// supported, along with inf, infinity, and nan. A leading + or - is accepted.
// Hexadecimal input requires a 0x/0X prefix and p/P binary exponent. On
// success, returns the number of input bytes consumed and stores the converted
// value in *result. A negative return
// value is an error: -INPUT_TOO_SHORT, -INPUT_TOO_LONG, or -MALFORMED_INPUT.
//
// s2d_n reads at most len bytes and does not require a nul terminator. Parsing
// stops before the first byte that is not part of a valid floating-point
// token. s2d parses a nul-terminated string with the same prefix semantics.
int32 s2d_n(const char *buffer, int32 len, double *result);
int32 s2d(const char *buffer, double *result);

enum Status s2f_n(const char * buffer, const int len, float * result);
enum Status s2f(const char * buffer, float * result);

#ifdef __cplusplus
}
#endif

#endif // RYU_PARSE_H
