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

## Alternative usage: compile cbase as a separate object
```sh
cc -std=c11 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -c cbase.c -o cbase.o 
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
