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

- Space is mandatory around `+` and `-`.
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
- Space is mandatory around all boolean operators (`!`, `&&`, and `||`).
- Space is mandatory around all comparison operators.
- In general, prefer `+= 1` instead of `++`.
  * Exception: dynamic arrays: use `array[len++] = value` instead of
    `array[len] = value; len += 1`.
- In general, prefer `-= 1` instead of `--`.
  * Exception: dynamic arrays: use `array[len--] = value` instead of
    `array[len] = value; len -= 1`.

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
- Always use trailing commas for arrays:
  ```c
  int32 array[] = {
      1,
      2,
      3,
  };
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

If the function header is long, and needs more than one line, put an extra new
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

In standalone declarations, if one is needed at all, put all in one line. Break
long lines so the 80-character limit rule is followed.

```c
static int32 function(int32 arg);
```

## Mechanical rewrite rules

Do not rewrite post-increment or post-decrement when it is part of an array
append or fill expression:

```c
array[len++] = value;  // keep as is
argv[i++] = "arg";     // keep as is
items[count++] = item; // keep as is
```

## Preprocessor directives
Don't use `#ifdef` and `#ifndef`, use `#if defined()` and `if !defined()`
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
Use line breaks indentation for struct initialization:

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
```

Also indent structs inside structs.
```c
typedef Struct MyStruct {
    char *string;
    struct {
        char *inner_string;
    } inner;
} MyStruct;

// good
MyStruct my_struct = {
    .string = "string",
    .inner = {
        .inner_string = "inner_string";
    },
};

```
