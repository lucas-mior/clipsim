#!/bin/sh -e

# shellcheck disable=SC2086

alias trace_on='set -x'
alias trace_off='{ set +x; } 2>/dev/null'

. ./targets

dir="$(realpath "$(dirname "$0")")"

target="${1:-build}"

if ! grep -q "$target" ./targets; then
    echo "usage: $(basename "$0") <targets>"
    cat targets
    exit 1
fi

cross="$2"

printf "\n${0} ${RED}${1} ${2}$RES\n"
PREFIX="${PREFIX:-/usr/local}"
DESTDIR="${DESTDIR:-/}"

main="main.c"
program="clipsim"
exe="bin/$program"
mkdir -p "$(dirname "$exe")"

CFLAGS="$CFLAGS -std=c99"
CFLAGS="$CFLAGS -Wextra -Wall"
CFLAGS="$CFLAGS -Wno-unknown-warning-option"
CFLAGS="$CFLAGS -Wno-unused-macros -Wno-unused-function"
CFLAGS="$CFLAGS -Wno-constant-logical-operand"
CFLAGS="$CFLAGS -Wno-unknown-pragmas"
CFLAGS="$CFLAGS -Wfatal-errors"
CPPFLAGS="$CPPFLAGS -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600"

LDFLAGS="$LDFLAGS $(pkg-config x11 --libs)"
LDFLAGS="$LDFLAGS $(pkg-config xfixes --libs)"
LDFLAGS="$LDFLAGS $(pkg-config xi --libs)"
LDFLAGS="$LDFLAGS $(pkg-config libmagic --libs)"

OS=$(uname -a)

CC=${CC:-cc}

case "$target" in
"debug")
    CFLAGS="$CFLAGS -Wno-declaration-after-statement -g -fsanitize=undefined"
    CPPFLAGS="$CPPFLAGS $GNUSOURCE -DDEBUGGING=1"
    ;;
"valgrind") 
    CFLAGS="$CFLAGS -g -O0 -ftree-vectorize"
    CPPFLAGS="$CPPFLAGS $GNUSOURCE -DDEBUGGING=1"
    ;;
"check") 
    CC=gcc
    CFLAGS="$CFLAGS $GNUSOURCE -fanalyzer"
    ;;
"build") 
    CFLAGS="$CFLAGS $GNUSOURCE -O2 -flto -march=native -ftree-vectorize"
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

if [ "$target" != "test" ] && [ "$CC" = "clang" ]; then
    CFLAGS="$CFLAGS -Weverything"
    CFLAGS="$CFLAGS -Wno-unsafe-buffer-usage"
    CFLAGS="$CFLAGS -Wno-format-nonliteral"
    CFLAGS="$CFLAGS -Wno-disabled-macro-expansion"
    CFLAGS="$CFLAGS -Wno-c++-keyword"
    CFLAGS="$CFLAGS -Wno-implicit-void-ptr-cast"
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
"test_all")
    ;;
*)
    trace_on
    ctags --kinds-C=+l+d ./*.h ./*.c 2> /dev/null || true
    vtags.sed tags > .tags.vim       2> /dev/null || true
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
