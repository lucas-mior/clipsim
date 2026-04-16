#!/bin/sh

# shellcheck disable=SC2086

alias trace_on='set -x'
alias trace_off='{ set +x; } 2>/dev/null'

. ./targets

dir=$(dirname "$(readlink -f "$0")")
cbase="cbase"
CPPFLAGS="$CPPFLAGS -I "$dir/$cbase""

target="${1:-build}"

if ! grep -q "$target" ./targets; then
    echo "usage: $(basename "$0") <targets>"
    cat targets
    exit 1
fi

optional_stuff_for_development() {
    if command -v ctags; then
        ctags -o tags --kinds-C=+l+d ./*.h ./*.c
        if command -v vtags.sed; then
            vtags.sed tags | sort | uniq > .tags.vim
        fi
    fi

    return
}

cross="$2"

printf "\n${0} ${RED}${1} ${2}$RES\n"
PREFIX="${PREFIX:-/usr/local}"
DESTDIR="${DESTDIR:-/}"

main="main.c"
program="clipsim"
exe="bin/$program"
mkdir -p "$(dirname "$exe")"

CFLAGS="$CFLAGS -std=c11"
CFLAGS="$CFLAGS -Wextra -Wall"
CFLAGS="$CFLAGS -Wno-unknown-warning-option"
CFLAGS="$CFLAGS -Wno-unused-macros"
CFLAGS="$CFLAGS -Wno-unused-function"
CFLAGS="$CFLAGS -Wno-gnu-union-cast"
CFLAGS="$CFLAGS -Wno-constant-logical-operand"
CFLAGS="$CFLAGS -Wno-unknown-pragmas"
CFLAGS="$CFLAGS -Wfatal-errors"
CFLAGS="$CFLAGS -Wno-float-equal"
CFLAGS="$CFLAGS -Wno-cast-qual"
CPPFLAGS="$CPPFLAGS -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600"

LDFLAGS="$LDFLAGS $(pkg-config x11 --libs)"
LDFLAGS="$LDFLAGS $(pkg-config xfixes --libs)"
LDFLAGS="$LDFLAGS $(pkg-config xi --libs)"
LDFLAGS="$LDFLAGS $(pkg-config libmagic --libs)"

CC=${CC:-cc}

case "$target" in
"fast_feedback")
    CC=clang
    CFLAGS="$CFLAGS -Werror"
    ;;
"test")
    CFLAGS="$CFLAGS -g3 $GNUSOURCE -DDEBUGGING=1 -fsanitize=undefined -Wno-address"
    ;;
"debug")
    CFLAGS="$CFLAGS -Wno-declaration-after-statement -g -fsanitize=undefined"
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1"
    exe="${exe}_debug"
    ;;
"valgrind")
    CFLAGS="$CFLAGS -g -O0 -ftree-vectorize"
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1"
    ;;
"check")
    CC=gcc
    CFLAGS="$CFLAGS -fanalyzer"
    ;;
"build")
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
    "x86_64-macos"|"aarch64-macos")
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
fi

case "$target" in
"uninstall")
    trace_on
    rm -f ${DESTDIR}${PREFIX}/bin/${program}
    rm -f ${DESTDIR}${PREFIX}/man/man1/${program}.1
    rm -f ${DESTDIR}${PREFIX}/share/fish/vendor_completions.d/${program}.fish
    rm -f ${DESTDIR}${PREFIX}/share/bash-completion/completions/${program}
    rm -f ${DESTDIR}${PREFIX}/share/zsh/site-functions/_${program}
    rm -f ${DESTDIR}${PREFIX}/share/licenses/${program}/LICENSE
    exit
    ;;
"install")
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
"assembly")
    trace_on
    $CC $CPPFLAGS $CFLAGS -S -o ${program}_$CC.S "$main" $LDFLAGS
    exit
    ;;
"test")
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
            if with_other "$CC" "$cmdline_no_cc"; then
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
                        $test_exe 2>&1 | xsel -b
                    exit 1
                fi
            else
                exit 1
            fi
        fi
        trace_off
    done
    exit
    ;;
"test_all")
    ;;
"fast_feedback")
    trace_on
    $CC $CPPFLAGS $CFLAGS main.c -o "$exe" $LDFLAGS && "$exe"
    trace_off
    ;;
*)
    trace_on
    optional_stuff_for_development
    $CC $CPPFLAGS $CFLAGS -o ${exe} "$main" $LDFLAGS
    trace_off
    ;;
esac

case "$target" in
"valgrind")
    vg_flags="--error-exitcode=1 --errors-for-leak-kinds=all"
    vg_flags="$vg_flags --leak-check=full --show-leak-kinds=all"

    trace_on
    valgrind $vg_flags -s --tool=memcheck $dir/bin/clipsim -d
    trace_off
    exit
    ;;
"check")
    CC=gcc CFLAGS="-fanalyzer" ./build.sh
    scan-build --view -analyze-headers --status-bugs ./build.sh
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
