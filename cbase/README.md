# cbase
C library for include basic functionallity and wrapping stuff from libc.
Alternative description:
"stuff that a sane programming language would provide by default".

## Usage (except include-based files)
- users of cbase shall `#include "cbase.h"`.
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
always include like `memory.h`, or `assert.c`, must NOT include `cbase.h`, or
the build will break. Those files shall include the follwing basic headers
instead as needed:

- `platform_detection.h`
- `libc.h`
- `primitives.h`
- `base_macros.h`

However, when testing files like assert.c, it is ok and necessary to `#define
CBASE_IMPLEMENT 1` and `#include "cbase.h"` so that the test compilation unit
works. But this must be done inside `#if TESTING_` block.

## Sadness
Code compiling utf8.c depends on `-D_XOPEN_SOURCE=700` because of `wcwidth`.
cbase in general depends on `-D_DEFAULT_SOURCE`.

## Alternative usage: compile cbase as a separate object
NOTE: it does not work yet, because all cbase functions are declared static.

```sh
gcc -DCBASE_IMPLEMENT=1 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -x c -c cbase.h -o cbase.o 
gcc cbase_main_separate_object.c cbase.o
```

## Infrastructure
Every C file in cbase/ must have block for avoid unused function warnings when
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
