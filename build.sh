#!/bin/sh -e

# shellcheck disable=SC2086

dir=$(dirname "$(readlink -f "$0")")
cd "$dir" || exit

# shellcheck source=./cbase/common.sh
. "./cbase/common.sh"

script=$(basename "$0")

common_build_parse_args "$@"

case "$mode" in
build|callgrind|check|cross|debug|debug-fast|fast_feedback)
    ;;
install|test|test_all|uninstall|valgrind)
    ;;
*)
    common_build_unknown_mode
    ;;
esac
cross="$target"

common_build_print_invocation "$script"
PREFIX="${PREFIX:-/usr/local}"
DESTDIR="${DESTDIR:-/}"

project=$(common_get_program "$0")
exe="bin/$project"
mkdir -p "$(dirname "$exe")"

CC=$(common_get_compiler "$mode")

CPPFLAGS="$CPPFLAGS -Icbase"

CFLAGS="$CFLAGS -std=c11"
CFLAGS="$CFLAGS -Wfatal-errors"

CPPFLAGS="$CPPFLAGS $(pkg-config x11 --cflags)"
CPPFLAGS="$CPPFLAGS $(pkg-config xfixes --cflags)"
CPPFLAGS="$CPPFLAGS $(pkg-config xi --cflags)"
CPPFLAGS="$CPPFLAGS $(pkg-config libmagic --cflags)"

LDFLAGS="$LDFLAGS $(pkg-config x11 --libs)"
LDFLAGS="$LDFLAGS $(pkg-config xfixes --libs)"
LDFLAGS="$LDFLAGS $(pkg-config xi --libs)"
LDFLAGS="$LDFLAGS $(pkg-config libmagic --libs)"
LDFLAGS="$LDFLAGS -lm"

case "$mode" in
fast_feedback)
    ;;
test)
    CFLAGS="$CFLAGS -g3 -DDEBUGGING=1"
    ;;
debug)
    CFLAGS="$CFLAGS -g3"
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1"
    ;;
debug-fast)
    CFLAGS="$CFLAGS -g2 -O2 -flto -march=native -ftree-vectorize"
    CFLAGS="$CFLAGS -fsanitize=undefined"
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1"
    ;;
valgrind)
    CFLAGS="$CFLAGS -g3 -Og -ftree-vectorize"
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1"
    ;;
callgrind)
    CFLAGS="$CFLAGS -g3 -O2 -ftree-vectorize"
    CPPFLAGS="$CPPFLAGS"
    ;;
check)
    common_build_run_analyzers build
    ;;
build)
    CFLAGS="$CFLAGS -O2 -flto -march=native -ftree-vectorize"
    ;;
cross)
    common_build_cross_all
    CFLAGS="$CFLAGS -O2"
    ;;
esac

if [ "$mode" = "cross" ]; then
    CC="zig cc"
    CFLAGS="$CFLAGS -target $cross"

    case $cross in
    x86_64-macos|aarch64-macos)
        CFLAGS="$CFLAGS -fno-lto"
        LDFLAGS="$LDFLAGS -lpthread"
        ;;
    *windows*)
        exe="bin/$project.exe"
        ;;
    *)
        LDFLAGS="$LDFLAGS -lpthread"
        ;;
    esac
else
    LDFLAGS="$LDFLAGS -lpthread"
fi

case "$mode" in
uninstall)
    trace_on
    rm -f ${DESTDIR}${PREFIX}/bin/${project}
    rm -f ${DESTDIR}${PREFIX}/man/man1/${project}.1
    rm -f ${DESTDIR}${PREFIX}/share/fish/vendor_completions.d/${project}.fish
    rm -f ${DESTDIR}${PREFIX}/share/bash-completion/completions/${project}
    rm -f ${DESTDIR}${PREFIX}/share/zsh/site-functions/_${project}
    rm -f ${DESTDIR}${PREFIX}/share/licenses/${project}/LICENSE
    exit
    ;;
install)
    if [ ! -f bin/$project ]; then
        $0 build
    fi
    trace_on
    install -Dm755 bin/${project}              ${DESTDIR}${PREFIX}/bin/${project}
    install -Dm644 ${project}.1                ${DESTDIR}${PREFIX}/man/man1/${project}.1
    install -Dm644 completions/${project}.fish ${DESTDIR}${PREFIX}/share/fish/vendor_completions.d/${project}.fish
    install -Dm644 completions/${project}.bash ${DESTDIR}${PREFIX}/share/bash-completion/completions/${project}
    install -Dm644 completions/${project}.zsh  ${DESTDIR}${PREFIX}/share/zsh/site-functions/_${project}
    install -Dm644 LICENSE                     ${DESTDIR}${PREFIX}/share/licenses/${project}/LICENSE
    exit
    ;;
test)
    TEST_EXCLUDE_PATTERN='(^|/)tests/|cbase' \
        common_test "$target"
    if command -v bash >/dev/null 2>&1; then
        tests/test.bash
    fi
    exit
    ;;
test_all)
    ;;
fast_feedback)
    trace_on
    $CC $CPPFLAGS $CFLAGS main.c -o "$exe" $LDFLAGS && "$exe"
    trace_off
    ;;
build|callgrind|cross|debug|debug-fast|valgrind)
    common_build_tags

    trace_on
    $CC $CPPFLAGS $CFLAGS -o ${exe} main.c $LDFLAGS
    trace_off
    ;;
esac

case "$mode" in
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
if [ "$mode" = "test_all" ]; then
    common_build_test_all "debug build test" gcc tcc clang "zig cc"
fi
