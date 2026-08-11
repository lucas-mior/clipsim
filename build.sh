#!/bin/sh -e

# shellcheck disable=SC2086

dir=$(dirname "$(readlink -f "$0")")
# shellcheck source=/dev/null
. "$dir/cbase/common.sh"

cd "$dir" || exit

cbase="cbase"
script=$(basename "$0")

if [ -f ./targets ]; then
    . ./targets
else
    targets=$(cat <<'EOF_TARGETS'
build
debug
fast_feedback
install
uninstall
test
check
assembly
valgrind
callgrind
test_all
cross x86_64-linux
cross aarch64-linux
cross x86_64-macos
cross aarch64-macos
cross x86_64-windows-gnu
EOF_TARGETS
)
fi

target="${1:-debug}"
target_line=$target
if [ "$target" = "cross" ] && [ -n "${2:-}" ]; then
    target_line="$target $2"
fi

if ! target_supported "$targets" "$target_line" \
        && ! target_supported "$targets" "$target"; then
    echo "usage: $script <targets>"
    printf '%s\n' "$targets"
    exit 1
fi

cross="$2"

printf "\n${script} ${RED}${1:-} ${2:-}$RES\n"
PREFIX="${PREFIX:-/usr/local}"
DESTDIR="${DESTDIR:-/}"

program=$(get_program "$0")
exe="bin/$program"
mkdir -p "$(dirname "$exe")"

CC=$(get_compiler "$target")

CPPFLAGS="$CPPFLAGS -I$dir/$cbase"

CFLAGS="$CFLAGS -std=c11"
CFLAGS="$CFLAGS -Wfatal-errors"
CFLAGS="$CFLAGS -Wextra -Wall"
CFLAGS="$CFLAGS -Werror=all -Werror=extra"
CFLAGS="$CFLAGS -Werror"  # Only uncomment occasionally, keep this line

if [ "$CC" = "clang" ]; then
    CFLAGS="$CFLAGS -Weverything"
    CFLAGS="$CFLAGS -Wno-assign-enum"
    CFLAGS="$CFLAGS -Wno-c++-keyword"
    CFLAGS="$CFLAGS -Wno-cast-qual"
    CFLAGS="$CFLAGS -Wno-constant-logical-operand"
    CFLAGS="$CFLAGS -Wno-covered-switch-default"
    CFLAGS="$CFLAGS -Wno-disabled-macro-expansion"
    CFLAGS="$CFLAGS -Wno-float-equal"
    CFLAGS="$CFLAGS -Wno-format-nonliteral"
    CFLAGS="$CFLAGS -Wno-implicit-int-enum-cast"
    CFLAGS="$CFLAGS -Wno-implicit-void-ptr-cast"
    CFLAGS="$CFLAGS -Wno-pre-c11-compat"
    CFLAGS="$CFLAGS -Wno-unknown-pragmas"
    CFLAGS="$CFLAGS -Wno-unsafe-buffer-usage"
    CFLAGS="$CFLAGS -Wno-unused-macros"
fi

LDFLAGS="$LDFLAGS $(pkg-config x11 --libs)"
LDFLAGS="$LDFLAGS $(pkg-config xfixes --libs)"
LDFLAGS="$LDFLAGS $(pkg-config xi --libs)"
LDFLAGS="$LDFLAGS $(pkg-config libmagic --libs)"
LDFLAGS="$LDFLAGS -lm"

case "$target" in
fast_feedback)
    ;;
test)
    CFLAGS="$CFLAGS -Wno-declaration-after-statement"
    CFLAGS="$CFLAGS -g3 -DDEBUGGING=1 -fsanitize=undefined"
    ;;
debug)
    CFLAGS="$CFLAGS -Wno-declaration-after-statement -g -fsanitize=undefined"
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1"
    ;;
valgrind)
    CFLAGS="$CFLAGS -g -Og -ftree-vectorize"
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1"
    ;;
callgrind)
    CFLAGS="$CFLAGS -g3 -O2 -ftree-vectorize"
    CPPFLAGS="$CPPFLAGS"
    ;;
check)
    CC=gcc CFLAGS="-fanalyzer -fdiagnostics-color=never" "$0" build
    CFLAGS="--analyze -Xanalyzer -analyzer-output=text"
    CFLAGS="$CFLAGS -Xanalyzer -analyzer-werror"
    CFLAGS="$CFLAGS -Xanalyzer -analyzer-opt-analyze-headers"
    CFLAGS="$CFLAGS -Wno-unused-command-line-argument"
    CFLAGS="$CFLAGS -fno-color-diagnostics"
    CC=clang CFLAGS="$CFLAGS" "$0" build
    exit
    ;;
build)
    CFLAGS="$CFLAGS -O2 -flto -march=native -ftree-vectorize"
    ;;
*)
    CFLAGS="$CFLAGS -O2"
    ;;
esac

if [ "$target" = "cross" ]; then
    CC="zig cc"
    CFLAGS="$CFLAGS -target $cross"

    case $cross in
    x86_64-macos|aarch64-macos)
        CFLAGS="$CFLAGS -fno-lto"
        LDFLAGS="$LDFLAGS -lpthread"
        ;;
    *windows*)
        exe="bin/$program.exe"
        ;;
    *)
        LDFLAGS="$LDFLAGS -lpthread"
        ;;
    esac
else
    LDFLAGS="$LDFLAGS -lpthread"
fi

case "$target" in
uninstall)
    trace_on
    rm -f ${DESTDIR}${PREFIX}/bin/${program}
    rm -f ${DESTDIR}${PREFIX}/man/man1/${program}.1
    rm -f ${DESTDIR}${PREFIX}/share/fish/vendor_completions.d/${program}.fish
    rm -f ${DESTDIR}${PREFIX}/share/bash-completion/completions/${program}
    rm -f ${DESTDIR}${PREFIX}/share/zsh/site-functions/_${program}
    rm -f ${DESTDIR}${PREFIX}/share/licenses/${program}/LICENSE
    exit
    ;;
install)
    if [ ! -f bin/$program ]; then
        $0 build
    fi
    trace_on
    install -Dm755 bin/${program}              ${DESTDIR}${PREFIX}/bin/${program}
    install -Dm644 ${program}.1                ${DESTDIR}${PREFIX}/man/man1/${program}.1
    install -Dm644 completions/${program}.fish ${DESTDIR}${PREFIX}/share/fish/vendor_completions.d/${program}.fish
    install -Dm644 completions/${program}.bash ${DESTDIR}${PREFIX}/share/bash-completion/completions/${program}
    install -Dm644 completions/${program}.zsh  ${DESTDIR}${PREFIX}/share/zsh/site-functions/_${program}
    install -Dm644 LICENSE                     ${DESTDIR}${PREFIX}/share/licenses/${program}/LICENSE
    exit
    ;;
assembly)
    trace_on
    $CC $CPPFLAGS $CFLAGS -S -o ${program}_$CC.S main.c $LDFLAGS
    exit
    ;;
test)
    TEST_EXCLUDE_PATTERN='(^|/)tests/' \
        test "$2"
    tests/test.bash
    exit
    ;;
test_all)
    ;;
fast_feedback)
    trace_on
    $CC $CPPFLAGS $CFLAGS main.c -o "$exe" $LDFLAGS && "$exe"
    trace_off
    ;;
*)
    trace_on
    build_tags

    $CC $CPPFLAGS $CFLAGS -o ${exe} main.c $LDFLAGS
    trace_off
    ;;
esac

case "$target" in
valgrind)
    vg_flags="--error-exitcode=1 --errors-for-leak-kinds=all"
    vg_flags="$vg_flags --leak-check=full --show-leak-kinds=all"

    trace_on
    valgrind $vg_flags -s --tool=memcheck $dir/bin/clipsim -d
    trace_off
    exit
    ;;
callgrind)
    trace_on
    out="callgrind-$(date +%s).callgrind"
    valgrind --tool=callgrind --callgrind-out-file="$out" bin/clipsim --daemon
    kcachegrind "$out"
    trace_off
    exit
    ;;
esac

trace_off
if [ "$target" = "test_all" ]; then
    printf '%s\n' "$targets" | while IFS= read -r target; do
        echo "$target" | grep -Eq "^(# |$)" && continue
        if echo "$target" | grep "cross"; then
            $0 $target
            continue
        fi
        for compiler in gcc tcc clang "zig cc" ; do
            CC=$compiler $0 $target || exit
        done
    done
fi
