# Ryu formatting and performance exceptions

The Ryu sources follow `cbase/c-format.md` and the non-semantic conventions
in `cbase/c-guidelines.md`. Public API names, numeric algorithms, lookup-table
contents, and the original upstream copyright/license notices are preserved.

The following are intentional exceptions to general implementation preferences:

- `?:` is permitted in Ryu, including the conversion hot paths.
- Unsigned arithmetic, intentional wraparound, casts, and fixed-width types are
  retained where they are part of the bit-exact floating-point algorithms.
- `__uint128_t`, MSVC intrinsics, and the 64-bit fallback are retained behind
  their existing compile-time feature selections. The `uint128` typedef is an
  arithmetic implementation detail, not a binary128 floating-point type.
- `sizeof` on lookup arrays and raw bit representations remains where the
  expression is compile-time constant or needed for representation checks.
- `static inline` arithmetic helpers, optimized division, compact lookup
  arrays, and algorithm-specific control flow are not rewritten into generic
  cbase wrappers just to satisfy a preference.
- Externally visible Ryu names and function signatures are kept for API/ABI
  compatibility. `floating_decimal_128` is an upstream public struct tag.
- The implementation section of `cbase/ryu.h` deliberately lives outside its
  declaration include guard: this is required by the existing include-based
  implementation pattern.
- Libc bit-copy, diagnostics, and allocation operations internal to the
  vendorized library are not indiscriminately replaced; doing so is outside
  this formatting audit and requires individual behavior/performance review.

## Verification

Run `./build.sh test ryu` with GCC and Clang. Useful alternate configurations:

- `TEST_CFLAGS='-DRYU_ONLY_64_BIT_OPS=1'`
- `TEST_CFLAGS='-DRYU_OPTIMIZE_SIZE=1'`
- `TEST_CFLAGS='-DRYU_FLOAT_FULL_TABLE=1'`
- `TEST_CFLAGS='-DRYU_32_BIT_PLATFORM=1'`
- `TEST_CFLAGS='-DRYU_AVOID_UINT128=1'`

Compile `cbase/ryu/generic_128.c` separately, including with `-DRYU_DEBUG`.
The `RYU_BENCHMARK=1` test option measures conversion paths. For before/after
comparisons, compile both revisions with identical compiler/optimization flags
and compare multiple runs in the same environment; small fluctuations are not
regressions on their own. Lookup-table formatting must leave compiled table
bytes unchanged.

## Forbidden-pattern checks

`cbase/functions_forbidden.sh` includes Ryu's `.c` and `.h` files in normal
checks, even with `TEST_MAXDEPTH` set. Selecting `TEST_FILTER=ryu` also
checks Ryu's implementation `.c` files rather than only `cbase/ryu.c`.
Explicit `TEST_EXCLUDE_PATTERN` exclusions still apply.

`csanecheck` enforces Ryu's 80-column limit, whitespace,
preprocessor guard/comment conventions, and its existing shared formatting
rules. Ternary operators are expressly permitted in Ryu, but the general C
rule against them still applies to ordinary non-macro source code elsewhere.
Performance-sensitive arithmetic and representation choices above are not
banned by this style checker.

Run `./build.sh test csanecheck` to run the checker regression tests.
The removed Perl regression suite and its cache-specific tests are no longer
available.
