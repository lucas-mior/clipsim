#if !defined(WARNINGS_H)
#define WARNINGS_H

#include "platform_detection.h"

#if !defined(DEBUGGING)
#define DEBUGGING 0
#endif

#if !defined(TESTING)
#define TESTING 0
#endif

#define DIAGNOSTIC_PRAGMA2(X) _Pragma(#X)
#define DIAGNOSTIC_PRAGMA(X) DIAGNOSTIC_PRAGMA2(X)

#if DEBUGGING && !TESTING
  #if CC_GCC
    #define DIAGNOSTIC(W) DIAGNOSTIC_PRAGMA(GCC diagnostic error W)
  #elif CC_CLANG
    #define DIAGNOSTIC(W) DIAGNOSTIC_PRAGMA(clang diagnostic error W)
  #else
    #define DIAGNOSTIC(W)
  #endif
#else
  #if CC_GCC
    #define DIAGNOSTIC(W) DIAGNOSTIC_PRAGMA(GCC diagnostic warning W)
  #elif CC_CLANG
    #define DIAGNOSTIC(W) DIAGNOSTIC_PRAGMA(clang diagnostic warning W)
  #else
    #define DIAGNOSTIC(W)
  #endif
#endif

#if CBASE_CRT_MSVC
  #pragma warning(push, 4)
#endif

#if CC_GCC
  #define IGNORE(W) DIAGNOSTIC_PRAGMA(GCC diagnostic ignored W)
  #pragma GCC diagnostic warning "-Wpragmas"
  #pragma GCC diagnostic warning "-Wunknown-pragmas"

  // -Wall
  DIAGNOSTIC("-Waddress")
  DIAGNOSTIC("-Warray-bounds")
  DIAGNOSTIC("-Wbool-compare")
  DIAGNOSTIC("-Wbool-operation")
  DIAGNOSTIC("-Wchar-subscripts")
  DIAGNOSTIC("-Wcomment")
  DIAGNOSTIC("-Wformat")
  DIAGNOSTIC("-Wformat-overflow")
  DIAGNOSTIC("-Wformat-truncation")
  DIAGNOSTIC("-Wint-in-bool-context")
  DIAGNOSTIC("-Wlogical-not-parentheses")
  DIAGNOSTIC("-Wmain")
  DIAGNOSTIC("-Wmaybe-uninitialized")
  DIAGNOSTIC("-Wmemset-elt-size")
  DIAGNOSTIC("-Wmemset-transposed-args")
  DIAGNOSTIC("-Wmisleading-indentation")
  DIAGNOSTIC("-Wmissing-attributes")
  DIAGNOSTIC("-Wmissing-braces")
  DIAGNOSTIC("-Wmultistatement-macros")
  DIAGNOSTIC("-Wnonnull")
  DIAGNOSTIC("-Wnonnull-compare")
  DIAGNOSTIC("-Wparentheses")
  DIAGNOSTIC("-Wpointer-sign")
  DIAGNOSTIC("-Wrestrict")
  DIAGNOSTIC("-Wreturn-type")
  DIAGNOSTIC("-Wsequence-point")
  DIAGNOSTIC("-Wsizeof-pointer-div")
  DIAGNOSTIC("-Wsizeof-pointer-memaccess")
  DIAGNOSTIC("-Wstrict-aliasing")
  DIAGNOSTIC("-Wstrict-overflow")
  DIAGNOSTIC("-Wstringop-overflow")
  DIAGNOSTIC("-Wstringop-truncation")
  DIAGNOSTIC("-Wswitch")
  DIAGNOSTIC("-Wswitch-bool")
  DIAGNOSTIC("-Wtautological-compare")
  DIAGNOSTIC("-Wtrigraphs")
  DIAGNOSTIC("-Wuninitialized")
  DIAGNOSTIC("-Wunused-function")
  DIAGNOSTIC("-Wunused-label")
  DIAGNOSTIC("-Wunused-value")
  DIAGNOSTIC("-Wunused-variable")
  DIAGNOSTIC("-Wvolatile-register-var")

  // -Wextra
  DIAGNOSTIC("-Wclobbered")
  DIAGNOSTIC("-Wcast-function-type")
  DIAGNOSTIC("-Wempty-body")
  DIAGNOSTIC("-Wignored-qualifiers")
  DIAGNOSTIC("-Wimplicit-fallthrough")
  DIAGNOSTIC("-Wmissing-field-initializers")
  DIAGNOSTIC("-Wmissing-parameter-type")
  DIAGNOSTIC("-Wold-style-declaration")
  DIAGNOSTIC("-Woverride-init")
  DIAGNOSTIC("-Wsign-compare")
  DIAGNOSTIC("-Wstring-compare")
  DIAGNOSTIC("-Wtype-limits")
  DIAGNOSTIC("-Wshift-negative-value")
  DIAGNOSTIC("-Wunused-parameter")
  DIAGNOSTIC("-Wunused-but-set-parameter")

  // Enabled by default or not enabled -Wall or -Wextra
  DIAGNOSTIC("-WNSObject-attribute")
  DIAGNOSTIC("-Waddress-of-packed-member")
  DIAGNOSTIC("-Waggressive-loop-optimizations")

  DIAGNOSTIC("-Wanalyzer-allocation-size")
  DIAGNOSTIC("-Wanalyzer-deref-before-check")
  DIAGNOSTIC("-Wanalyzer-double-fclose")
  DIAGNOSTIC("-Wanalyzer-double-free")
  DIAGNOSTIC("-Wanalyzer-exposure-through-output-file")
  DIAGNOSTIC("-Wanalyzer-exposure-through-uninit-copy")
  DIAGNOSTIC("-Wanalyzer-fd-access-mode-mismatch")
  DIAGNOSTIC("-Wanalyzer-fd-double-close")
  DIAGNOSTIC("-Wanalyzer-fd-leak")
  DIAGNOSTIC("-Wanalyzer-fd-phase-mismatch")
  DIAGNOSTIC("-Wanalyzer-fd-type-mismatch")
  DIAGNOSTIC("-Wanalyzer-fd-use-after-close")
  DIAGNOSTIC("-Wanalyzer-fd-use-without-check")
  DIAGNOSTIC("-Wanalyzer-file-leak")
  DIAGNOSTIC("-Wanalyzer-free-of-non-heap")
  DIAGNOSTIC("-Wanalyzer-imprecise-fp-arithmetic")
  DIAGNOSTIC("-Wanalyzer-infinite-loop")
  DIAGNOSTIC("-Wanalyzer-infinite-recursion")
  DIAGNOSTIC("-Wanalyzer-jump-through-null")
  DIAGNOSTIC("-Wanalyzer-malloc-leak")
  DIAGNOSTIC("-Wanalyzer-mismatching-deallocation")
  DIAGNOSTIC("-Wanalyzer-null-argument")
  DIAGNOSTIC("-Wanalyzer-null-dereference")
  DIAGNOSTIC("-Wanalyzer-out-of-bounds")
  DIAGNOSTIC("-Wanalyzer-overlapping-buffers")
  DIAGNOSTIC("-Wanalyzer-possible-null-argument")
  DIAGNOSTIC("-Wanalyzer-possible-null-dereference")
  DIAGNOSTIC("-Wanalyzer-putenv-of-auto-var")
  DIAGNOSTIC("-Wanalyzer-shift-count-negative")
  DIAGNOSTIC("-Wanalyzer-shift-count-overflow")
  DIAGNOSTIC("-Wanalyzer-stale-setjmp-buffer")
  DIAGNOSTIC("-Wanalyzer-tainted-allocation-size")
  DIAGNOSTIC("-Wanalyzer-tainted-array-index")
  DIAGNOSTIC("-Wanalyzer-tainted-assertion")
  DIAGNOSTIC("-Wanalyzer-tainted-divisor")
  DIAGNOSTIC("-Wanalyzer-tainted-offset")
  DIAGNOSTIC("-Wanalyzer-tainted-size")
  DIAGNOSTIC("-Wanalyzer-throw-of-unexpected-type")
  DIAGNOSTIC("-Wanalyzer-undefined-behavior-ptrdiff")
  DIAGNOSTIC("-Wanalyzer-undefined-behavior-strtok")
  DIAGNOSTIC("-Wanalyzer-unsafe-call-within-signal-handler")
  DIAGNOSTIC("-Wanalyzer-use-after-free")
  DIAGNOSTIC("-Wanalyzer-use-of-pointer-in-stale-stack-frame")
  DIAGNOSTIC("-Wanalyzer-use-of-uninitialized-value")
  DIAGNOSTIC("-Wanalyzer-va-arg-type-mismatch")
  DIAGNOSTIC("-Wanalyzer-va-list-exhausted")
  DIAGNOSTIC("-Wanalyzer-va-list-leak")
  DIAGNOSTIC("-Wanalyzer-va-list-use-after-va-end")
  DIAGNOSTIC("-Wanalyzer-write-to-const")
  DIAGNOSTIC("-Wanalyzer-write-to-string-literal")

  DIAGNOSTIC("-Wattribute-alias")
  DIAGNOSTIC("-Wattribute-warning")
  DIAGNOSTIC("-Wattributes")
  DIAGNOSTIC("-Wbidi-chars")
  DIAGNOSTIC("-Wbuiltin-declaration-mismatch")
  DIAGNOSTIC("-Wbuiltin-macro-redefined")
  DIAGNOSTIC("-Wcannot-profile")
  DIAGNOSTIC("-Wcompare-distinct-pointer-types")
  DIAGNOSTIC("-Wcomplain-wrong-lang")
  DIAGNOSTIC("-Wcoverage-invalid-line-number")
  DIAGNOSTIC("-Wcoverage-mismatch")
  DIAGNOSTIC("-Wcoverage-too-many-conditions")
  DIAGNOSTIC("-Wcoverage-too-many-paths")
  DIAGNOSTIC("-Wcpp")
  DIAGNOSTIC("-Wdeclaration-missing-parameter-type")

  IGNORE("-Wdeprecated")
  IGNORE("-Wdeprecated-declarations")

  DIAGNOSTIC("-Wdeprecated-openmp")
  DIAGNOSTIC("-Wdesignated-init")
  DIAGNOSTIC("-Wdiscarded-array-qualifiers")
  DIAGNOSTIC("-Wdiscarded-qualifiers")
  DIAGNOSTIC("-Wdiv-by-zero")
  DIAGNOSTIC("-Wendif-labels")
  DIAGNOSTIC("-Wfree-nonheap-object")
  DIAGNOSTIC("-Whardened")
  DIAGNOSTIC("-Wif-not-aligned")
  DIAGNOSTIC("-Wignored-attributes")
  DIAGNOSTIC("-Wincompatible-pointer-types")
  DIAGNOSTIC("-Wint-conversion")
  DIAGNOSTIC("-Wint-to-pointer-cast")
  DIAGNOSTIC("-Winvalid-memory-model")
  DIAGNOSTIC("-Wlto-type-mismatch")
  DIAGNOSTIC("-Wmissing-profile")
  DIAGNOSTIC("-Wmusttail-local-addr")
  DIAGNOSTIC("-Wnormalized")
  DIAGNOSTIC("-Wodr")
  DIAGNOSTIC("-Wopenmp")
  DIAGNOSTIC("-Woverflow")
  DIAGNOSTIC("-Woverride-init-side-effects")
  DIAGNOSTIC("-Wpointer-compare")
  DIAGNOSTIC("-Wpointer-to-int-cast")
  DIAGNOSTIC("-Wpragma-once-outside-header")

  DIAGNOSTIC("-Wprio-ctor-dtor")
  DIAGNOSTIC("-Wpsabi")
  DIAGNOSTIC("-Wreturn-local-addr")
  DIAGNOSTIC("-Wreturn-mismatch")
  DIAGNOSTIC("-Wscalar-storage-order")
  DIAGNOSTIC("-Wshift-count-negative")
  DIAGNOSTIC("-Wshift-count-overflow")
  DIAGNOSTIC("-Wsizeof-array-argument")
  DIAGNOSTIC("-Wstringop-overread")
  DIAGNOSTIC("-Wswitch-outside-range")
  DIAGNOSTIC("-Wswitch-unreachable")
  DIAGNOSTIC("-Wsync-nand")
  DIAGNOSTIC("-Wtsan")
  DIAGNOSTIC("-Wunicode")
  DIAGNOSTIC("-Wunused-result")
  DIAGNOSTIC("-Wvarargs")
  DIAGNOSTIC("-Wxor-used-as-pow")
#endif

#if CC_CLANG
  DIAGNOSTIC("-Weverything")
  #define IGNORE(W) DIAGNOSTIC_PRAGMA(GCC diagnostic ignored W)

  #pragma clang diagnostic warning "-Wunknown-warning-option"

  IGNORE("-Wassign-enum")
  IGNORE("-Wc++-keyword")
  IGNORE("-Wc++98-compat")
  IGNORE("-Wcast-function-type-strict")
  IGNORE("-Wcast-qual")
  IGNORE("-Wchar-subscripts")
  IGNORE("-Wconstant-logical-operand")
  IGNORE("-Wcovered-switch-default")
  IGNORE("-Wdisabled-macro-expansion")
  IGNORE("-Wfloat-equal")
  IGNORE("-Wformat-nonliteral")
  IGNORE("-Wimplicit-int-enum-cast")
  IGNORE("-Wimplicit-void-ptr-cast")
  IGNORE("-Wnrvo")
  IGNORE("-Wpadded")
  IGNORE("-Wpre-c11-compat")
  IGNORE("-Wtentative-definition-compat")
  IGNORE("-Wunsafe-buffer-usage")
  IGNORE("-Wunused-macros")
  IGNORE("-Wused-but-marked-unused")
  IGNORE("-Wdeprecated-declarations")

  #if OS_MAC
    #pragma clang diagnostic warning "-Wallocator-wrappers"
  #endif
#endif

#if DEBUGGING || TESTING
  #if CC_GCC || CC_CLANG
    #pragma GCC diagnostic ignored "-Wunused-function"
  #endif
#endif

#undef IGNORE
#undef DIAGNOSTIC

#endif /* WARNINGS_H */
