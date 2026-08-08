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

main="main.c"
program=$(get_program "$0")
exe="bin/$program"
mkdir -p "$(dirname "$exe")"

case "$target" in
debug|test)
    CC="${CC:-tcc}"
    ;;
fast_feedback)
    CC="${CC:-clang}"
    ;;
*)
    CC="${CC:-cc}"
    ;;
esac

if ! command -v "$CC" > /dev/null 2>&1; then
    CC=cc
fi

CPPFLAGS="$CPPFLAGS -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700"
CPPFLAGS="$CPPFLAGS -I$dir/$cbase"

CFLAGS="$CFLAGS -std=c11"
CFLAGS="$CFLAGS -Wfatal-errors"
CFLAGS="$CFLAGS -Wextra -Wall"
CFLAGS="$CFLAGS -Werror=all -Werror=extra"
CFLAGS="$CFLAGS -Werror"  # Only uncomment occasionally, keep this line
CFLAGS="$CFLAGS -Wno-cast-qual"
CFLAGS="$CFLAGS -Wno-constant-logical-operand"
CFLAGS="$CFLAGS -Wno-float-equal"
CFLAGS="$CFLAGS -Wno-gnu-union-cast"
CFLAGS="$CFLAGS -Wno-unknown-pragmas"
CFLAGS="$CFLAGS -Wno-unknown-warning-option"
CFLAGS="$CFLAGS -Wno-unused-macros"

if [ "$CC" = "clang" ]; then
    CFLAGS="$CFLAGS -Weverything"
    CFLAGS="$CFLAGS -Wno-unsafe-buffer-usage"
    CFLAGS="$CFLAGS -Wno-format-nonliteral"
    CFLAGS="$CFLAGS -Wno-disabled-macro-expansion"
    CFLAGS="$CFLAGS -Wno-c++-keyword"
    CFLAGS="$CFLAGS -Wno-covered-switch-default"
    CFLAGS="$CFLAGS -Wno-implicit-void-ptr-cast"
    CFLAGS="$CFLAGS -Wno-format-pedantic"
    CFLAGS="$CFLAGS -Wno-pre-c11-compat"
    CFLAGS="$CFLAGS -Wno-implicit-int-enum-cast"
    CFLAGS="$CFLAGS -Wno-assign-enum"
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
    CFLAGS="$CFLAGS -g3 $GNUSOURCE -DDEBUGGING=1 -fsanitize=undefined"
    ;;
debug)
    CFLAGS="$CFLAGS -Wno-declaration-after-statement -g -fsanitize=undefined"
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1"
    ;;
valgrind)
    CFLAGS="$CFLAGS -g -O0 -ftree-vectorize"
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
    $CC $CPPFLAGS $CFLAGS -S -o ${program}_$CC.S "$main" $LDFLAGS
    exit
    ;;
test)
    find . -iname "*.c" | sort | while read -r src; do
        trace_off
        name=$(basename "$src")

        if [ -n "$2" ] && [ "$name" != "$2" ]; then
            continue
        fi
        if [ "$name" = "$main" ]; then
            continue
        fi
        if echo "$src" | grep -q "stc/"; then
            continue
        fi
        if echo "$src" | grep -q "tests/"; then
            continue
        fi
        name=$(echo "$name" | sed 's/\.c//')
        test_exe="/tmp/${name}_test"

        printf "\nTesting ${RED}${src}${RES} ...\n"

        flags="$(awk '/\/\/ flags:/ { $1=$2=""; print $0 }' "$src")"
        if [ "$name" = "windows_functions" ]; then
            if ! zig version; then
                continue
            fi
            cmdline="zig cc $CPPFLAGS $CFLAGS"
            cmdline=$(option_remove "$cmdline" "-D_GNU_SOURCE")
            cmdline="$cmdline -target x86_64-windows-gnu"
            cmdline="$cmdline -Wno-unused-variable -DTESTING_$name=1 -DTESTING=1"
            cmdline="$cmdline $flags -o $test_exe $src"
            CC="zig cc"
        else
            cmdline="$CC $CPPFLAGS $CFLAGS"
            cmdline="$cmdline -Wno-unused-variable -DTESTING_$name=1 -DTESTING=1 $LDFLAGS"
            cmdline="$cmdline $flags -o $test_exe $src"
        fi

        if [ "$CC" = "chibicc" ] || [ "$CC"  = "cproc" ]; then
            cmdline_no_cc=$(option_remove "$cmdline" "$CC")
            trace_on
            if COMPILE_WITH_OTHER_EXTRA_DEFS="-D__attribute=__attribute__" \
                    compile_with_other "$CC" "$cmdline_no_cc"; then
                /tmp/${name}_test
            else
                exit 1
            fi
        else
            trace_on
            if $cmdline; then
                if ! $test_exe; then
                    gdb --quiet \
                        -ex run -ex backtrace -ex quit \
                        $test_exe 2>&1 | tee /dev/tty | xsel -i -b
                    exit 1
                fi
            else
                exit 1
            fi
        fi
        trace_off
    done
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

    $CC $CPPFLAGS $CFLAGS -o ${exe} "$main" $LDFLAGS
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
