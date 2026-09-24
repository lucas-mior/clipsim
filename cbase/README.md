# cbase
C library for include basic functionallity and wrapping stuff from libc.
Alternative description:
"stuff that a sane programming language would provide by default".

## Usage (except include-based files)
- users of cbase shall `#include "cbase.h"` before ANY OTHER INCLUDES.
- one of the files shall `#define CBASE_IMPLEMENT 1` before including `cbase.h`

## Usage for include-based files like hash.c and xenums.c
- the user must first have `#included "cbase.h" before continuing
- then, the user must `#define` the relevant macros for the "template"
- then, the user must `#include "hash.c"

## Architecture and development
`cbase.h` must only be included by user code OR by files in cbase/ that
`cbase.h` itself does not depends on.
In other words, files that get only included when `CBASE_IMPLEMENT` is defined,
are ok to include `cbase.h` themselves. However, files that `cbase.h` must
always include like `memory.h`, or `assertion.c`, must NOT include `cbase.h`, or
the build will break. Those files shall include the follwing basic headers
instead as needed:

- `platform_detection.h`
- `libc.h`
- `primitives.h`
- `base_macros.h`

Also, sometimes we need to use a separate translation unit for wrapping a stupid
library that does not use proper name space convention. In this case, *do not*
include "cbase.h" from the wrapper. You may include the files above, and fix any
name collision that ends up happening. For instance, harfbuzz library defined a
macro called SIZEOF. (Really? They really couldn't have used HB_SIZEOF, I
guess).

However, when testing files like assertion.c, it is ok and necessary to `#define
CBASE_IMPLEMENT 1` and `#include "cbase.h"` so that the test compilation unit
works. But this must be done inside `#if TESTING_` block.

## Sadness
Code compiling utf8.c depends on `-D_XOPEN_SOURCE=700` because of `wcwidth`.
cbase in general depends on `-D_DEFAULT_SOURCE`.

## Formatter
`fmt_snprintf` and `fmt_vsnprintf` are cbase's deterministic printf-style
formatters. They return the number of bytes that would have been written,
excluding the terminating `'\0'`, and write a terminating `'\0'` whenever the
capacity is positive. `buffer == NULL` is valid only when `capacity == 0`.
Negative returns are errno-style failures such as `-EINVAL`, `-EOVERFLOW`,
`-ENOSYS`, and `-EILSEQ`.

The formatter intentionally uses cbase semantics instead of libc locale or libc
extension semantics:

- locale is ignored completely; floating point numbers always use `.` as the
  decimal separator.
- positional arguments are not supported.
- GNU, glibc, and compiler-specific extensions such as `%m` are not supported.
- the integer conversions are `%d`, `%i`, `%u`, `%o`, `%x`, `%X`, `%b`, and
  `%B`.
- the supported integer length modifiers are no modifier, `hh`, `h`, `ll`,
  `w8`, `w16`, `w32`, and `w64`.
- integer `l`, `j`, `z`, `t`, and `wfN` length modifiers are not supported.
  Use `ll` for cbase `int64`/`uint64`, or `w8`/`w16`/`w32`/`w64` when the
  width must be explicit.
- `%p` is deterministic and formats as lowercase `0x...`; a null pointer is
  `0x0`.
- `%n` stores the logical byte count that would have been produced, not the
  number of bytes physically copied into the destination buffer. It supports the
  same length modifiers as cbase integers.
- `%s` formats a NULL `char *` as `(null)`.
- `%.*s` is a cbase byte-span formatter. It consumes an `int32` byte length and
  a `char *`, writes exactly that many bytes, and ignores embedded `'\0'`
  bytes. `NULL` is accepted only when the length is zero.
- `%.Ns` remains an ordinary nul-terminated string precision and stops at the
  first `'\0'`.
- `%lc` and `%ls` are deterministic Unicode-to-UTF-8 conversions. They do not
  call locale-dependent conversion routines. Invalid Unicode input fails with
  `-EILSEQ`; `%ls` precision is a byte limit and never splits a UTF-8 sequence.
- `%f`, `%F`, `%e`, `%E`, `%g`, `%G`, `%a`, and `%A` are supported for
  `double`.
- `%Lf`, `%LF`, `%Le`, `%LE`, `%Lg`, `%LG`, `%La`, and `%LA` are supported for
  the common `long double` ABIs decoded by cbase. This is platform specific;
  unknown `long double` representations fail with `-ENOSYS` instead of being
  silently narrowed to `double`.
- `%a` and `%La` are tied to the binary representation and are generated from
  the decoded floating-point bits.

The variadic `fmt_snprintf` declaration uses the compiler's printf format
attribute. That still catches ordinary type mistakes, but the compiler only
checks the printf grammar it knows. Warnings for newer conversions such as
`%b`, `%B`, or `wN` therefore depend on compiler support for those spellings.

`snprintf2`, `SNPRINTF`, and `sb_printf` are not implemented in terms of this
formatter yet.

## Alternative usage: compile cbase as a separate object
```sh
cc -std=c11 -c cbase.c -o cbase.o
cc -std=c11 your_main.c cbase.o
```

## Infrastructure
Every .c file in cbase/ must have block for avoid unused function warnings when
not testing that specific file. This allows to still see which functions are not
being tested, without warnings if a specific project does not use all the
functions of cbase/.

```c
#if 0 == TESTING_memory
static inline void
memory_functions_sink(void) {
    (void)memory_check;
    (void)realloc4;
    (void)free2_;
    (void)realloc_flex_debug;
    return;
}
#endif
```
