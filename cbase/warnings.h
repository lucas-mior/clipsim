#if !defined(WARNINGS_H)
#define WARNINGS_H

#include "platform_detection.h"

#if !defined(DEBUGGING)
#define DEBUGGING 0
#endif

#if !defined(TESTING)
#define TESTING 0
#endif

#define CBASE_DIAGNOSTIC_PRAGMA2(X) _Pragma(#X)
#define CBASE_DIAGNOSTIC_PRAGMA(X) CBASE_DIAGNOSTIC_PRAGMA2(X)

#if DEBUGGING == 1
#define CBASE_GCC_DIAGNOSTIC_WARNING(W) \
    CBASE_DIAGNOSTIC_PRAGMA(GCC diagnostic error W)
#define CBASE_CLANG_DIAGNOSTIC_WARNING(W) \
    CBASE_DIAGNOSTIC_PRAGMA(clang diagnostic error W)
#else
#define CBASE_GCC_DIAGNOSTIC_WARNING(W) \
    CBASE_DIAGNOSTIC_PRAGMA(GCC diagnostic warning W)
#define CBASE_CLANG_DIAGNOSTIC_WARNING(W) \
    CBASE_DIAGNOSTIC_PRAGMA(clang diagnostic warning W)
#endif

#if CBASE_CRT_MSVC
  #pragma warning(push, 4)
#endif

#if CC_GCC
  // -Wall
  CBASE_GCC_DIAGNOSTIC_WARNING("-Waddress")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Warray-bounds")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wbool-compare")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wbool-operation")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wchar-subscripts")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wcomment")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wformat")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wformat-overflow")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wformat-truncation")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wint-in-bool-context")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wlogical-not-parentheses")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wmain")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wmaybe-uninitialized")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wmemset-elt-size")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wmemset-transposed-args")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wmisleading-indentation")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wmissing-attributes")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wmissing-braces")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wmultistatement-macros")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wnonnull")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wnonnull-compare")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wparentheses")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wpointer-sign")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wrestrict")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wreturn-type")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wsequence-point")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wsizeof-pointer-div")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wsizeof-pointer-memaccess")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wstrict-aliasing")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wstrict-overflow")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wstringop-overflow")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wstringop-truncation")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wswitch")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wswitch-bool")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wtautological-compare")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wtrigraphs")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wuninitialized")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wunknown-pragmas")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wunused-function")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wunused-label")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wunused-value")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wunused-variable")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wvolatile-register-var")

  // -Wextra
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wclobbered")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wcast-function-type")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wempty-body")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wignored-qualifiers")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wimplicit-fallthrough")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wmissing-field-initializers")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wmissing-parameter-type")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wold-style-declaration")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Woverride-init")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wsign-compare")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wstring-compare")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wtype-limits")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wshift-negative-value")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wunused-parameter")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wunused-but-set-parameter")

  // Enabled by default or not enabled -Wall or -Wextra
  CBASE_GCC_DIAGNOSTIC_WARNING("-WNSObject-attribute")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Waddress-of-packed-member")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Waggressive-loop-optimizations")

  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-allocation-size")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-deref-before-check")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-double-fclose")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-double-free")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-exposure-through-output-file")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-exposure-through-uninit-copy")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-fd-access-mode-mismatch")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-fd-double-close")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-fd-leak")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-fd-phase-mismatch")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-fd-type-mismatch")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-fd-use-after-close")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-fd-use-without-check")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-file-leak")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-free-of-non-heap")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-imprecise-fp-arithmetic")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-infinite-loop")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-infinite-recursion")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-jump-through-null")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-malloc-leak")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-mismatching-deallocation")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-null-argument")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-null-dereference")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-out-of-bounds")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-overlapping-buffers")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-possible-null-argument")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-possible-null-dereference")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-putenv-of-auto-var")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-shift-count-negative")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-shift-count-overflow")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-stale-setjmp-buffer")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-tainted-allocation-size")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-tainted-array-index")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-tainted-assertion")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-tainted-divisor")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-tainted-offset")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-tainted-size")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-throw-of-unexpected-type")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-undefined-behavior-ptrdiff")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-undefined-behavior-strtok")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-unsafe-call-within-signal-handler")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-use-after-free")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-use-of-pointer-in-stale-stack-frame")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-use-of-uninitialized-value")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-va-arg-type-mismatch")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-va-list-exhausted")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-va-list-leak")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-va-list-use-after-va-end")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-write-to-const")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wanalyzer-write-to-string-literal")

  CBASE_GCC_DIAGNOSTIC_WARNING("-Wattribute-alias")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wattribute-warning")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wattributes")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wbidi-chars")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wbuiltin-declaration-mismatch")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wbuiltin-macro-redefined")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wcannot-profile")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wcompare-distinct-pointer-types")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wcomplain-wrong-lang")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wcoverage-invalid-line-number")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wcoverage-mismatch")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wcoverage-too-many-conditions")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wcoverage-too-many-paths")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wcpp")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wdeclaration-missing-parameter-type")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wdeprecated")
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wdeprecated-openmp")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wdesignated-init")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wdiscarded-array-qualifiers")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wdiscarded-qualifiers")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wdiv-by-zero")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wendif-labels")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wfree-nonheap-object")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Whardened")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wif-not-aligned")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wignored-attributes")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wincompatible-pointer-types")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wint-conversion")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wint-to-pointer-cast")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Winvalid-memory-model")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wlto-type-mismatch")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wmissing-profile")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wmusttail-local-addr")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wnormalized")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wodr")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wopenmp")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Woverflow")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Woverride-init-side-effects")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wpointer-compare")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wpointer-to-int-cast")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wpragma-once-outside-header")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wpragmas")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wprio-ctor-dtor")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wpsabi")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wreturn-local-addr")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wreturn-mismatch")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wscalar-storage-order")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wshift-count-negative")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wshift-count-overflow")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wsizeof-array-argument")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wstringop-overread")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wswitch-outside-range")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wswitch-unreachable")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wsync-nand")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wtsan")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wunicode")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wunused-result")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wvarargs")
  CBASE_GCC_DIAGNOSTIC_WARNING("-Wxor-used-as-pow")
#endif

#if CC_CLANG
  CBASE_CLANG_DIAGNOSTIC_WARNING("-Weverything")

  #pragma clang diagnostic ignored "-Wassign-enum"
  #pragma clang diagnostic ignored "-Wc++-keyword"
  #pragma clang diagnostic ignored "-Wc++98-compat"
  #pragma clang diagnostic ignored "-Wcast-function-type-strict"
  #pragma clang diagnostic ignored "-Wcast-qual"
  #pragma clang diagnostic ignored "-Wchar-subscripts"
  #pragma clang diagnostic ignored "-Wconstant-logical-operand"
  #pragma clang diagnostic ignored "-Wcovered-switch-default"
  #pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
  #pragma clang diagnostic ignored "-Wfloat-equal"
  #pragma clang diagnostic ignored "-Wformat-nonliteral"
  #pragma clang diagnostic ignored "-Wimplicit-int-enum-cast"
  #pragma clang diagnostic ignored "-Wimplicit-void-ptr-cast"
  #pragma clang diagnostic ignored "-Wnrvo"
  #pragma clang diagnostic ignored "-Wpadded"
  #pragma clang diagnostic ignored "-Wpre-c11-compat"
  #pragma clang diagnostic ignored "-Wtentative-definition-compat"
  #pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
  #pragma clang diagnostic ignored "-Wunused-macros"
  #pragma clang diagnostic ignored "-Wused-but-marked-unused"
  #pragma clang diagnostic ignored "-Wdeprecated-declarations"

  #pragma clang diagnostic warning "-Wunknown-warning-option"
#endif

#if DEBUGGING || TESTING
  #if CC_GCC || CC_CLANG
    #pragma GCC diagnostic ignored "-Wunused-function"
  #endif
#endif

#endif /* WARNINGS_H */
