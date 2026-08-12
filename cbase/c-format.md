# C formatting style for 64-bit PC programming
## Note
When a rule says "prefer", avoid changing already-readable code unless the
specific form is listed as bad. Exceptions listed under a "prefer" rule should
be preserved.

For non-formatting coding guidelines, see `c-guidelines.md`.

## Whitespace and indentation
- For indentation, never use tabs.
  * Use spaces for indentation.
  * Use 4 spaces for each indentation level.
- Trim trailing whitespace.
- Keep line length at maximum 80 characters.

## Operators
- Space is forbidden around unary plus and minus (`a = -1;`)
- Space is mandatory around binary plus and minus (`a + b` and `a - b`).
  * But not when using `+=` or `-=`:
    ```c
    i += 1; // good

    i+=1;   // bad
    i+= 1;  // bad
    i +=1;  // bad
    ```
- Space or newline (if at end of line) is mandatory after comma.
- Space is forbidden around `*` used for multiplication.
- Space is optional around `/` used for division (keep as is).
- Space is optional around bitwise or (`|`)
- Space is mandatory around bitwise and (`&`)
- Space is forbidden after bitwise not (`~`)
- Space is mandatory before bitwise not (`~`)
- Space is mandatory around bitshifts (`<<` and `>>`)
- Space is forbidden around dot and arrow (`.` and `->`) for acessing struct and
  union fields.
- Space is forbidden after dot used for initializing struct fields (but space
  before the dot is fine).
- Space is mandatory around boolean or and boolean and (`&&`, and `||`).
- Space is forbidden around boolean negation (`!`).
- Space is mandatory around all comparison operators.
- In general, prefer `+= 1` instead of `++`.
- In general, prefer `-= 1` instead of `--`.

## Casts

- Do not add space for casts:
  ```c
  int32 x = (int32)y;  // good
  int32 x = (int32) y; // bad
  ```

## Pointers

- For pointers, use space after the type and no space before the name:
  ```c
  char *string;
  ```

## Function calls

- Never use space before parenthesis of function calls.

## Identation
When breaking long lines that are long expressions, try to make them readable
and aligned, see below for good examples:

```c
enum MyFlags flags = MY_FLAG_EXAMPLE1
                     |MY_FLAG_EXAMPLE2
                     |MY_FLAG_EXAMPLE3
                     |MY_FLAG_EXAMPLE4
                     |MY_FLAG_EXAMPLE5;

if ((flags & MY_FLAG_EXAMPLE1)
    && (some_other_condition || other_condition_yet)) {
    // do stuff
}
```

## Switch formatting

`case` must align with `switch`.

```c
switch (value) {
case 0:
    handle_zero();
    break;
default:
    handle_default();
    break;
}
```

## Declaration and initialization formatting

- Never declare more than one variable per line:
  ```c
  // bad
  int32 x, y;

  // good
  int32 x;
  int32 y;
  ```
- Always use trailing commas for arrays (except when initializing to zero):
  ```c
  int32 array[] = {
      1,
      2,
      3,
  };

  int32 array2[10] = {0};
  ```

## Curly braces

- Always use curly braces for all control-flow blocks:
  ```c
  // good
  if (condition) {
      do_only_one_thing();
  }

  // bad
  if (condition)
      do_only_one_thing();
  ```

## Parentheses

Complex `if` conditions must have parentheses around each subexpression:

```c
// bad
if (x < 0 && y > 1) {
}

// good
if ((x < 0) && (y > 1)) {
}
```

But do not add parentheses for the not operator if precedence is not confusing:

```c
// bad
if ((!condition1) || (!condition2)) {
}

// good
if (!condition1 || !condition2) {
}

// bad
if ((!condition1) || (x > 0)) {
}

// good
if (!condition1 || (x > 0)) {
}

// bad
if (!condition1 || confusing_precedence || (x > 0)) {
}

// good
if (!(condition1) || confusing_precedence || (x > 0)) {
}
```

Don't use extra parenthesis if the condition is a simple variable:
```c
// bad
if ((condition1) || (condition2)) {
}

// good
if (condition1 || condition2) {
}
```

## Function declaration and definition

In the definition, put a newline before the function name and parameters:

```c
static int32
function(int32 arg) {
    return 0;
}
```

If the function header is long, and needs more than one line,
break the line but keep the identation, if it fits in 80 columns:
```c
// good
static void
function_with_long_name_and_multiple_arguments(void *pointer,
                                               char *argument_long_name) {
    return;
}

// bad
static void
function_with_long_name_and_multiple_arguments(
    void *pointer,
    char *argument_long_name
) {
    return;
}
```

If the function header is long, and the identation makes it not fit in 80
columns, break, put an extra new
line before the closing parenthesis:
```c
// bad
static void
function_with_long_name_and_multiple_arguments(void *pointer,
    char *argument_long_name_very_long) {
    return;
}

// good
static void
function_with_long_name_and_multiple_arguments(
    void *pointer,
    char *argument_long_name_very_long
) {
    return;
}
```

For function with many arguments, it is a good idea to group by types/intent of
the parameter, even if it adds more lines:
```c
// bad
static void
function_with_long_name_and_multiple_arguments(MyStruct *handle, int32 x,
                                               int32 y, double a, double b);

// good
static void
function_with_long_name_and_multiple_arguments(MyStruct *handle,
                                               int32 x, int32 y,
                                               double a, double b);
```

In function calls/definitions/headers, try to make the `_len` of a variable in
the same line of the object it refers to:
```c
// bad
static void
function_with_long_name_and_multiple_arguments_x(MyStruct *handle, char *string,
                                                 int32 string_len);
function_with_long_name_and_multiple_arguments_x(handle, string_name,
                                                 string_name_len);

// good
static void
function_with_long_name_and_multiple_arguments(MyStruct *handle,
                                               char *string, int32 string_len);
function_with_long_name_and_multiple_arguments(handle,
                                               string_name, string_name_len);
```

In the pattern above, if it is not possible to put string and string_len side by
side withtout going over 80 columns, put them in separate lines.

In standalone declarations, if one is needed at all, put all in one line. Break
long lines so the 80-character limit rule is followed.

Prefer to break after an argument than before the equal sign, specially if the
first argument fits in the first line:
```c
// bad
    array->items
        = realloc2(array->items, old_cap, new_cap, SIZEOF(*array->items));
// good
    array->items = realloc2(array->items,
                            old_cap, new_cap, SIZEOF(*array->items));

// bad
    array->items = realloc2(extremelly_long_argument_that_does_not_fit_in_this_line,
                            old_cap, new_cap, SIZEOF(*array->items));
// good
    array->items
        = realloc2(extremelly_long_argument_that_does_not_fit_in_this_line,
                   old_cap, new_cap, SIZEOF(*array->items));
```

```c
static int32 function(int32 arg);
```

## Preprocessor directives
Don't use `#ifdef` and `#ifndef`, use `#if defined()` and `#if !defined()`
instead. Prefer explicitly setting the macro to 0 or 1 and checking its value
directly instead of checking if it is defined:
```c
#if MY_MACRO_CONDITION
// stuff
#endif
```

Break lines longer than 80 characters with the backslash. Example:
```c
#define MY_VERY_EXTREMELY_LONG_MACRO_NAME \
  12.4779847021478714732904782347234237409192312
```

## Initialization
Use line breaks indentation for struct initialization,
except when initializing to zero:

```c
typedef struct MyStruct {
    char *string;
    char *other;
} MyStruct;

// bad
MyStruct my_struct = { .string = "string", };

// good
MyStruct my_struct = {
    .string = "string",
};

// good
MyStruct my_struct = {
    .string = "string",
    .other = "other",
};

// bad
MyStruct my_struct = { .string = "string", .other = "other", };

// bad
MyStruct my_struct = {
    0
};

// good
MyStruct my_struct = {0};
```

Also indent structs inside structs.
```c
typedef struct MyStruct {
    char *string;
    struct {
        char *inner_string;
    } inner;
} MyStruct;

// good
MyStruct my_struct = {
    .string = "string",
    .inner = {
        .inner_string = "inner_string",
    },
};

```
