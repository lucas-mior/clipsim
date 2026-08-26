# C coding guidelines for 64-bit PC programming
Use this together with c-format.md

## Note
When a rule says "prefer", avoid changing already-readable code unless the
specific form is listed as bad. Exceptions listed under a "prefer" rule should
be preserved.

For formatting-only style rules, see `c-format.md`.

## Naming style

- Use `CamelCase` for types.
- Use `snake_case` for variables and functions.
- Use `CAPITAL_SNAKE_CASE` for macros.
  * Exception: `error` macro.
- Variable names should be descriptive, but descriptiveness must be
  proportional to scope. Do not give `too_much_descriptive_name` to a variable
  that is only used inside a short loop.
- Use `i`, `j`, and `k` for `for` loop counters.
- Don't use `error` for names of variables, it is already used to print error
  messages. Use something like `${some_prefix}_error`.

## for loops
- Use the pattern `for (int32 i = 0; i < N; i += 1)` for loops.
  * Use `for (uint32 i = 0; i < N; i += 1)` if N is an enum value.

## Types

- `const` keyword:
  * Never use it in function declarations nor definitions
  * You can use it for static constant global data
  * You can use it for casts needed to interface with the C standard library or
    other stupid libraries.
- Avoid using `size_t`, `ptrdiff_t`, and `ssize_t`.
  * Prefer `int64` or `int32` depending on context.
  * The only reason to use `size_t`, `ptrdiff_t`, and `ssize_t` is for casting
    so that we conform to C standard library functions that expect those types.
- Never use `_t` integer types.
  * Include `primitives.h` and use the better aliases typedefed there.
- Avoid using legacy C integers: `short`, `int`, `long`, and `long long`.
  * Prefer using `int16`, `int32`, and `int64` instead.
    + See `cbase/primitives.h` to see how they are defined.
- Boolean flags that are not used in hot loops or hot structs should use the
  `bool` type and the `true` and `false` keywords to communicate intent.
- If a boolean variable is used in a very hot loop, `int32` may be preferable.
- Do not confuse booleans with bits in bit flags. Use `#define BITFLAGS 1` in
  `xenums.c` for up to 32 related flags that occupy 1 bit each.
- When casting to a smaller integer type, or when casting double to integer, or
  double to float, and we are not sure if it fits, check first using the `MAXOF`
  macro:
  ```c
  int32 function_that_returns_int32(void *param) {
      int64 value = function_that_returns_int64(param);

      // Note: MAXOF uses _Generic, it does not really runs the function below.
      if (value >= MAXOF(function_that_returns_int32())) {
          error("Too large value\n");
          fatal(EXIT_FAILURE);
      }

      // Note: (int32) cast is hardcoded,
      // but we will be warned by the compiler if the return type ever changes
      return (int32)value;
  }
  ```
- unsigned integers: avoid, prefer signed integers
  * use unsigned integers for bit flags and other bit-wise operated values.
  * use unsigned for converting integers to and from enums.
  * use unsigned for intentional wrapping, like when needed while hashing
  * when interfacing a stupid library that receives unsigned integers where a
    signed integer is used on our side of the code, write a wrapper. The
    wrapper checks if the signed value is less than zero before converting.
  * when interfacing a stupid library that returns unsigned integers where a
    signed integer is used on our side of the code, write a wrapper. The
    wrapper checks if the received value fits in the positive range of our
    signed integer type. Use `MAXOF()` macro defined in cbase/.
  * For all other uses like arithmetic, indices, sizes, offsets, etc, use
    signed integers.

## Expressions and control flow

- Always add an explicit `return;` at the end of void functions, unless the path
  is unreachable (as an example, it may call `fatal` or `exit`).
- Never use the ternary operator:
  ```c
  something = condition ? whentrue : whenfalse;
  ```
  Use normal `if`/`else` blocks instead.
- Avoid creating functions that exit the program. Use `fatal(EXIT_FAILURE)` at
  the call site.

## `sizeof`

The `SIZEOF` macro casts the result of `sizeof` to `int64`. Signed sizes are way
better than unsigned.
Use the `SIZEOF` macro like this:

```c
int64 alloc_size = len*SIZEOF(*pointer);
```

Avoid using `SIZEOF(TypePointedTo)`. Prefer to use `SIZEOF(*pointer)`.

For `typeof`, also use parenthesis (always):
```c
typeof var   // bad
typeof(var)  // good
```

## Memory allocation
- Avoid `malloc`, `calloc`, `realloc`, and `free`.
  * Use wrappers from `cbase/memory.c` that track allocations in debug builds
    instead:
    + `malloc2(size)`
    + `free2(pointer, size)`
    + `realloc2(pointer, old_array_capacity, new_array_capacity, obj_size)`
    + `malloc2(size) + memset64(pointer, 0, size)` instead of `calloc`, or
    + `realloc2(size) + memset64(pointer, 0, size)` instead of `calloc`.
  * The wrappers above never fail: if out of memory, they exit the program. No
    need to check if they succeded or not.
  * `free2` already checks if the passed pointer is NULL. Never check if the
    pointer is NULL before calling `free2`.
- Choose what is best in each situation:
  * Use the `arena.c` bump allocator for groups of allocations with the same
    lifetime, either of equal or different sizes.
  * Use traditional `malloc2`, `realloc2`, and `free2` when more appropriate.
  * Use the stack for small capped allocations.
- Never use VLAs.
- Some guidelines to avoid memory errors (use after free, double free, invalid
  free, leak):
  * Use the arena allocator from `cbase/arena.c` as much as possible. 
    Only use `malloc2`, `realloc2`, and `free2` if you
    really need the flexibility, for instance:
    + if you need to grow the allocation
    + if you are inside a callback that would be infeasible to pass an arena
      pointer
    + possible other reasons
  * For tracking ownership, you can use handlers to a specific module/type of
    the program that is responsible for allocating and freeing its objects. A
    good example is `cbase/hash.c`
  * For dynamic arrays, use the generic arrays defined in `cbase/array.c`, if
    possible.

### exiting the program
- To exit from within `main()`, always use `exit()`, never `return`.
- Fatal errors: use `fatal(EXIT_FAILURE)`
- Clean exit: use `exit(EXIT_SUCCESS)`

## Enums, structs, and unions
- Enums that don't need the `_str` and `_parse` functions, and arent bit flags,
  don't need xenums.c. Define the enum manually.
- But do use include-based `xenums.c` for creating enums if it is a bit flag
  enum, or if we need the `_str` or the `_parse` functions.
  * It will give automatic bit flags if needed with `#define ENUM_BITFLAGS 1`.
  * It will give automatic `_str` and `_parse` functions.
  * It will give automatic `_LAST` value. Use it for iterating on the enum:
    ```c
    for (uint32 x = 0; x < MY_ENUM_LAST; x += 1) {
        printf("x = %s.\n", MY_ENUM_str(x));
    }
    ```
- The moment that you find that an existing enum ends up needing `_str`, or
  `_parse`, then it is time to define it using `xenums.c`.
- Always typedef structs:
  ```c
  typedef struct MyStruct {
      int32 number;
  } MyStruct;
  ```
- Never typedef enums and unions.
- Never typedef pointers.

## Function declarations
Most functions don't need to have an extra declaration, only the definition will
suffice. Properly order the functions in a file so that extra declarations
aren't needed. For functions that do need an extra pre declaration, put the
declaration in a project-wide header file or in the header file associated with
the C file itself. Avoid declaring functions defined in a C file, in another C
file. If a C function for whatever reason needs to define a function that is
only used for tests in itself and tests in another file, put it inside an #if
TESTING block, to avoid unused function warnings when compiling the main
program.
- Never use `#if TESTING_module` block in `.h` files.
- Never use more than one `#if TESTING_module` block in a C file.
- Never use more than one `#if TESTING` block in a C file and use it only if
  absolutely needed as explained above.

## Struct declarations

Try to organize big structs in logical groups or types with empty lines:

```c
// good
typedef struct MyStruct {
    char *main;
    char *other;
    char *what;

    int32 main_len;
    int32 other_len;
    int32 what_len;
} MyStruct;

// bad (this is uglier and wastes space because of padding)
typedef struct MyStruct {
    char *main;
    int32 main_len;
    char *other;
    int32 other_len;
    char *what;
    int32 what_len;
} MyStruct;
```

## Strings and their lengths

In general, we must always know the lengths of our strings.

In general, pass `char *string` and `int32 string_len` around. Also use this
convention in struct definitions.

That means to also avoid calling `strlen32`:

- `strlen32` shall be viewed as an interface for code that we do not control or
  for C string literals.
- `STRLIT_LEN("literal")` is also to be avoided. Only use it if you need to pass
  the length of a string literal, but not the string literal itself (which would
  be very weird). It is also in the `STRLIT` definition.
  * Never to stupid shit like: `my_function("literal", STRLIT_LEN("literal")`
    + Instead, to `my_function(STRLIT("literal"))`
- `STRLIT("literal")` can be used to pass the string literal and its length
  in an "don't repeat yourself" way, that also does not depend on the compiler
  to optimize the `strlen32`, since it uses `sizeof` to get the length of the
  literal.

Exceptions to this rule are:

- Macros `ENDS_WITH` and `BEGINS_WITH`: they use a macro trick to allow passing
  only the string, or also passing the string length. See `cbase.h`.
- Functions that in general only operate on short literals. In this case it is
  ok to let the function call `strlen32` inside.
- `StrBuilder`: use this struct and its functions to build long, dynamic
  strings. Do not use it where a single
  `SNPRINTF(stack_array, "format_%s_string", args);` would be enough.
  * Use `SB_APPEND` for appending literals or strings of known length, and
    `sb_printf` for formatting.
  * `SNPRINTF` and `snprintf2` return the number of bytes written (excluding the
    terminating null byte. No need to call `strlen32` on the buffer:
    ```c
    // bad
    static void
    function(int32 m) {
        int32 n;
        char buffer[256];

        SNPRINTF(buffer, "%d", m);
        n = strlen32(buffer);
    }

    // good
    static void
    function(int32 m) {
        int32 n;
        char buffer[256];

        n = SNPRINTF(buffer, "%d", m);
    }
    ```
- If the string is a literal passed just for printing purposes, very common in
  test suites, then it is okay to simply pass the `char *` without the length.

Considering all that, most `string.h` functions from the C standard library are
to be avoided. `strcpy`, `strcat`, `strstr`, and `strtok` are always the wrong
choice once you have the habit of always knowing the length of your strings.
Prefer `memcpy64`, `memmem64`, or custom functions that operate on string with
known length.

## Comparing strings:
In general, avoid `strcmp()`, use the alternatives below instead:
- For strings that are both null terminated and we don't know the length of
  either one:
  * use `strequal(s1, s2)`
- For strings that we know the length of the one (it might be null terminated
  but not necessarly), but we don't know the length of the other (the other must
  be null terminated):
  * use `STREQUAL(s1, s1_len, s2)`
- For strings that we know the length of both (they might be null terminated,
  but not necessarly):
  * use `STREQUAL(s1, s1_len, s2, s2_len)`
- Use `strcmp()` when comparing strings for sorting purposes.

## Checking if a string begins or ends with something:
Don't use `strncmp` to check if a string begins or ends with something.
Instead use:
- `BEGINS_WITH(string, string_len, "prefix_as_literal")` or
  `BEGINS_WITH(string, string_len, prefix, prefix_len)`
- `ENDS_WITH(string, string_len, "suffix_as_literal")` or
  `ENDS_WITH(string, string_len, suffix, suffix_len)`

## Standard-library wrappers

Use the project's own `int64`/`int32` wrappers for useful standard-library
functions. Those are defined in cbase/.

## Error messages

- Use the `error` or `error2` macro.
  * `error` includes `__FILE__`, `__LINE__`, and `__func__`,
    so avoid using it for error messages composed of multiple calls.
  * `error2` is simply a wrapper for `fprintf(stderr,`, so avoid using it if
    more context about the bug would be useful.

## Fatal errors

- Print the error message using `error()` and then exit using
  `fatal(EXIT_FAILURE)`.
- Avoid creating functions that exit the program. Use `fatal(EXIT_FAILURE)`
  where the failure is detected:
  ```c
  // bad
  if (some_error_condition) {
      my_custom_error_reporting_function_that_also_exits();
  }

  // good
  if (some_error_condition) {
      my_custom_error_reporting_function();
      fatal(EXIT_FAILURE);
  }
  ```

## Error handling

Avoid initializing if you are not sure that the result is valid. Examples:

```c
// bad
void my_function(char *path) {
    FILE *file = XFOPEN(path, "w");  // might fail and we only check it later
    int32 x = 0;
    int32 y = 0;

    // Note how the error checking is far away from where the error happened.
    if (!file) {
        fatal(EXIT_FAILURE);
    }
}

// better
void my_function(char *path) {
    FILE *file;
    int32 x = 0;
    int32 y = 0;

    file = XFOPEN(path, "r");
    if (file == NULL) {
        fatal(EXIT_FAILURE);
    }
}

// even better
void my_function(char *path) {
    FILE *file;
    int32 x = 0;
    int32 y = 0;

    if ((file = XFOPEN(path, "r")) == NULL) {
        fatal(EXIT_FAILURE);
    }
}
```

In general, use the assignment inside the if condition, if directly related:
```c
// bad
void function(void) {
    int32 *result = get_result();

    if (result) {
        // do something with result
    }
}

// good
void function(void) {
    int32 *result;

    if ((result = get_result())) {
        // do something with result
    }
}

// also good
void function(void) {
    int32 *result;

    if ((result = get_result()) == NULL) {
        // early return
        return;
    }

    // do something with result
}
```

But do initialize if the initialization is clear:
```c
// bad
for (int32 i = 0; i < LENGTH(some_array); i += 1) {
    char *string;
    
    string = some_array[i];
}

// good
for (int32 i = 0; i < LENGTH(some_array); i += 1) {
    char *string = some_array[i];
}
```

Never check against `-1` for functions that return -1 or negative integer on
error. Check `< 0` instead, since it works for the case where the negative
result encodes an error number. The only exception is the `switch (fork())`
pattern, but using `fork()` directly should be avoided anyway (use the
`cbase/command.c` API instead which wraps `fork()`.

### Control flow and error handling
- Avoid `goto`. Use it only for common cleanup logic.
- Sometimes a useful pattern to avoid using goto:
  ```c
  do {
      if ((a = may_fail()) < 0) {
          break;
      }
      if ((b = may_fail2()) < 0) {
          break;
      }
      do_something(a, b);
  } while (0);
  ```

### Return value for errors
- Only model failures that callers can reasonably handle. Programmer errors
  should be asserted with `cbase/assertions.h`. Unrecoverable application errors
  may print a diagnostic and exit. Out of memory is currently unrecoverable and
  should always exit; see `cbase/memory.h` and `cbase/memory.c`.

- Functions that cannot fail should not return an error value.

- Functions that return an index, count, length, file descriptor, or another
  non-negative integer result may return a negative value to report failure.
  These functions must use a signed return type.
  * Check for errors in those functions by doing:
    ```c
    if ((idx = my_function()) < 0) {
        // error condition
    }
    ```

- Functions that return a pointer may return `NULL` when the natural result is
  optional or absent, such as lookup/find functions. Do not use `NULL` for
  general failure when the caller may need to know why the operation failed.

- Use `bool` only for predicates. Names like `is_*`, `has_*`, `can_*`,
  `contains_*`, and `matches_*` should return boolean answers. Failure to answer
  the question should be impossible or asserted.

- Use signed integer status for fallible actions. Names like `read_*`,
  `write_*`, `parse_*`, `rename_*`, `init_*`, `open_*`, `build_*`,
  `resolve_*`, and `normalize_*` should return a signed status/result. `0`
  means success when there is no payload; non-negative values may be successful
  counts, indices, lengths, or other results; negative values are errors.

- Convert libc `errno` to the project convention at the boundary. For example,
  after a failing libc call, capture `errno` immediately and return `-errno`
  or the equivalent project error code.

- Project/domain errors should also be negative values. Define positive error
  constants and return their negation, so callers can pass `-err` to an error
  string function.

- Use out-parameters when the natural return value is data. For operations that
  need to return a pointer, string, struct, or multiple outputs, return the
  status/result code and write the data through out-parameters.

- Do not return `bool` from fallible action functions just because the caller
  currently ignores the reason for failure. Return a signed status/result and let
  the caller ignore the specific negative value if it does not care.

## More on if expressions
Don't use logical negation and equality for bounds checking:

```c
// bad
if (!number_of_stuff) {
    // there isn't any stuff
}

// good
if (number_of_stuff <= 0) {
    // there isn't any stuff
}

// bad
if (number_of_stuff == capacity) {
    // is at maximum capacity
}

// good
if (number_of_stuff >= capacity) {
    // is at maximum capacity
}
```

Only check if something is different than zero, if zero is more meaningfull
than simply "zero items" (i.e. zero is a valid value in all circumstances for
that variable).

```c
// bad
if (nitems != 0) {
    // there are items
}

// good
if (nitems) {
    // there are items
}

// bad
while (nitems-- != 0) {
    // there are items
}

// good
while (nitems--) {
    // there are items
}

// bad
if (point.x) {
    // point is not at x == 0
}

// good
if (point.x != 0) {
    // point is not at x == 0
}
```

Don't use logical negation to check if a pointer is null:
```c
// bad
if (!pointer) {
    // pointer is NULL
}

// good
if (pointer == NULL) {
    // pointer is NULL
}
```

Don't use `!= NULL` to check if a pointer is not null:
```c
// bad
if (pointer != NULL) {
    // pointer is not NULL
}

// good
if (pointer) {
    // pointer is not NULL
}
```

## Assertions
- Use the assertions defined in `cbase/assertions.c`.
  - `ASSERT(expression)`
  - `ASSERT_NULL(pointer)`
  - `ASSERT_ZERO(integer expression)`
  - `ASSERT_POSITIVE(integer expression)`
  - `ASSERT_NEGATIVE(integer expression)`
  - `ASSERT_NON_POSITIVE(integer expression)`
  - `ASSERT_NON_NEGATIVE(integer expression)`
  - `ASSERT_EQUAL(number or string or pointer 1, number or string or pointer 2)`
  - `ASSERT_EQUAL(string, string_len, other_string)`
  - `ASSERT_LESS(number expr 1, number expr 2)`
  - `ASSERT_MORE(number expr 1, number expr 2)`
  - `ASSERT_LESS_EQUAL(number expr 1, number expr 2)`
  - `ASSERT_MORE_EQUAL(number expr 1, number expr 2)`
  - `ASSERT_CONTAINS(haystack, haystack_len, needle)`
  - `ASSERT_NOT_CONTAINS(haystack, haystack_len, needle)`
  - `ASSERT_FILE_CONTAINS(haystack, haystack_len, needle)`
  * They all use `__builtin_unreachable` if the condition fails if not
    debugging. Don't use them for non-debugging assertions.
    + Assertions that must happen in non-debugging builds, must be explicit code
      with a clear error message, not an assertion macro.
- Do not use `ASSERT_EQUAL` for enums.
  * Use `ASSERT(enumvalue1 == enumvalue2)` instead, so that the compiler does
    not complain.

## Modules

- In general, use unity builds for programs.
  * Use `#include "file.c"` to include other files.
  * Define all functions as `static`.

## Magic numbers and magic literals

- In general, avoid numbers that have no explanation. Calculate them explicitly
  or use a macro or constant.

Sometimes we want to have a string associated with a test or data. If possible,
wrap the test case or function invocation inside a macro that uses the
`#identifier` operation to transform an identifier into a string literal. This
keeps the string explaining the test in sync with the test itself. Example:

```c
#define XX(FUNCTION) {#FUNCTION, FUNCTION}
static struct TestCase tests[] = {
    XX(test_fixture_metadata_matches_generated_output),
    XX(test_generated_headers_compile_for_valid_fixtures),
    XX(test_compile_and_run_generated_valid_system),
};
#undef XX
```

## Switch and fork style

Always add explicit `break`, unless the case exits the program.

Use the following pattern:

```c
pid_t child;  // only if the pid of the child is actually needed.

switch (child = fork()) {
case -1:
    error("Error forking: %s.\n", strerror(errno));
    fatal(EXIT_FAILURE);
case 0:
    // code for child
    break;
default:
    // code for parent
    // (only related stuff,
    //  like handling file descriptors, or waiting for the child).
    break;
}
```

## Declaration and initialization
- Only initialize if the value is correct:
  * Initializing to zero "just to be safe", because you don't know if and when
    the variable will be used, is a fundamental error. The program must be
    strucutured so that variables are initialized consiously, not because we are
    scared of what might happen if reading a undefined value.
- To initialize a stack struct to zero, use `MyStruct my_struct = {0};`
  * Don't use `memset(&my_struct, 0, SIZEOF(my_struct));`
- To initialize a stack array to zero, use `MyType array[ARRAY_SIZE] = {0};`
  * Don't use `memset(&array, 0, SIZEOF(array));`
- To initialize a heap struct to zero, use `*my_struct = (MyStruct){0};`
  * Don't use `memset(my_struct, 0, SIZEOF(*my_struct));`
- To initialize a heap array to zero, use `memset64`.
- Try to keep the scope of variables reduced. Use of artificial blocks is
  sometimes useful:
  ```c
  {
      // this var only exists here and can't leak where it is not needed
      int32 x;
  }
  ```
- Prefer to declare variable at the top of blocks
  * Exception: for loop counters (`for (int32 i = 0; i < N; i += 1)`)
  * Another exception: generated code / meta programming.
  * Don't mix declarations and code (`-Wdeclaration-after-statement`)
  * Sometimes it is good practice is to create artificial blocks to reduce the
    scope of variables. Only do it for reasonably long functions, for short
    functions, declare variables at the top of the function and call it a day.
  * If a variable is only used inside a while/for loop, declare inside the
    while/for block:
    ```c
    // bad
    static void
    function(int32 a, int32 b) {
        int32 x;

        while (a != b) {
            x = a;
            // some code
        }
    }

    // good
    static void
    function(int32 a, int32 b) {
        while (a != b) {
            int32 x = a;
            // some code
        }
    }
    ```

- Variable that have a "default return" value, or a "stub" value, shall be
  initialized with the declaration:
  ```c
  // bad
  static bool function(void *parameters) {
      bool result;

      result = false;

      // do stuff that might change result

      return result;
  }

  // good
  static bool function(void *parameters) {
      bool result = false;

      // do stuff that might change result

      return result;
  }
  ```
  * But initializing an "ORing" or "summing" variable is better done before the
    "ORing"/"summing" loop:
    ```c
    // bad (this obscures the fact that 0 is not a dummy return:
    //      it is a valid temporary state of the variable)
    static uint32
    function(void *params) {
        uint32 mask = 0;
        double other_var;

        for (uint32 i = 0; i < N; i += 1) {
            mask |= some_function(params, i);
        }
        return mask;
    }

    // better (this makes it clear how the variable works)
    // prefer this only when there are many variables, which can confuse the
    // reader. When there are fewer variables, use the // even better version
    static uint32
    function(void *params) {
        uint32 mask;
        double other_var;

        mask = 0;
        for (uint32 i = 0; i < N; i += 1) {
            mask |= some_function(params, i);
        }
        return mask;
    }

    // even better (now the important variable is initialized near the loop,
                    while also keeping conciseness)
    static uint32
    function(void *params) {
        double other_var;
        uint32 mask = 0;

        for (uint32 i = 0; i < N; i += 1) {
            mask |= some_function(params, i);
        }
        return mask;
    }
    ```
- Variables that are aliases/copies, shall be declared and initialized in the
  same line:
  ```c
  // bad
  void
  my_function(ProgramOptions *options) {
      char *name;

      name = options->name;
  }

  // good
  void
  my_function(ProgramOptions *options) {
      char *name = options->name;
  }
  ```

## Utilities
Use utilities available in `cbase/`. There are functions for command building
and executing, file reading and writing, string comparison, path manipulation,
memory, etc.

Before implementing a new common utility function, check whether this
functionality is already in `cbase/`. If not, implement it in the most
appropriate cbase/ C file.

## Functions
- Avoid defining functions that are called only once in the same file.
  * Simply write the code for the function inlined.
- Everytime you call a function, look at the implementation of the function to
  check the types and the semantics of the function. Don't call functions that
  you don't know how they work. 
  * The exceptions are functions defined in external
    libraries (libc and other libraries not implemented by us). In this case,
    check the documentation of the function before calling it.
- Try to give code some empty lines here and there.
  * For instance, a good practice is to add a blank line after the variable
    declarations of a block.
  * Try to group related statements together.
  * Another good practice is group assignments to the same struct together:
  ```c
  char *some_string = "abc";

  my_struct->field = 0;
  my_struct->other = 1;
  my_struct->n = 2;

  do_function(my_struct);
  ```

## Algorithms reutilization
Don't reimplement root finding, minimization and integrating algorithms.
When you need one of those, make use of the algorithms implemented in
`cy_roots.c`, `cy_minimize.c` and `cy_methods.c`.

## Testing
Every .c file (except the main program) must have a testing block:
```c
#if TESTING_file_prefix`
#define CBASE_IMPLEMENT 1
#include "cbase.h"

#include "other_needed_file.c"

int main(void) {
    // tests most of the file
}
#endif /* TESTING_file_prefix */
```

## File organization
- Add include guards for every C file, except the ones uses as include templates
  like `cbase/hash.c` and `cbase/xenums.c`.
- The first file to be included is always `cbase.h`, or the build will break.
  * Exceptions: Some files in `cbase/` are not allowed to include `cbase.h`
    because `cbase.h` itself depends on them.

## libc functions
In general, avoid using libc functions.  In particular, never use any function
listed in `functions_never_use.txt`; Functions listed in
`functions_allowed_cbase_only.txt` may be used (only to write a wrapper for it)
in `cbase/`, but not in any project main code (use the wrapper instead).

## Missing cases

If these guides miss some case, decide based on the files in `cbase/`. If you
still do not know, use common sense.
