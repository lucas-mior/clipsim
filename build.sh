#!/bin/sh

# shellcheck disable=SC2086

target="${1:-build}"
PREFIX="${PREFIX:-/usr/local}"
DESTDIR="${DESTDIR:-/}"

src="main.c"
headers="clipsim.h"
program="clipsim"

CC=${CC:-cc}

# pkg-config libs
LDLIBS="$(pkg-config x11 --libs) $(pkg-config xfixes --libs) $(pkg-config xi --libs) $(pkg-config libmagic --libs) -lpthread"

# base flags
CFLAGS="$CFLAGS -std=c99 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600"
CFLAGS="$CFLAGS -Wall -Wextra"
CFLAGS="$CFLAGS -Wno-unused-function -Wno-sign-conversion"
CFLAGS="$CFLAGS -Wno-implicit-int-conversion -Wno-constant-logical-operand"

# clang-specific flags
if [ "$CC" = "clang" ]; then
    CFLAGS="$CFLAGS -Weverything -Wno-unsafe-buffer-usage"
    CFLAGS="$CFLAGS -Wno-format-nonliteral -Wno-declaration-after-statement"
    CFLAGS="$CFLAGS -Wno-format-zero-length"
fi

echo "target=$target"

case "$target" in
    "debug")
        CFLAGS="$CFLAGS -g -fsanitize=undefined -DCLIPSIM_DEBUG"
        CFLAGS="$CFLAGS -Wno-format-zero-length"
        ;;
    "release"|"build")
        CFLAGS="$CFLAGS -O2 -flto"
        ;;
    "clang")
        CC=clang
        CFLAGS="$CFLAGS -Weverything -Wno-unsafe-buffer-usage"
        CFLAGS="$CFLAGS -Wno-format-nonliteral -Wno-declaration-after-statement"
        CFLAGS="$CFLAGS -Wno-format-zero-length"
        CFLAGS="$CFLAGS -O2 -flto"
        ;;
esac

case "$target" in
    "uninstall")
        set -x
        rm -f ${DESTDIR}${PREFIX}/bin/${program}
        rm -f ${DESTDIR}${PREFIX}/share/man/man1/${program}.1
        rm -f ${DESTDIR}${PREFIX}/share/fish/vendor_completions.d/${program}.fish
        rm -f ${DESTDIR}${PREFIX}/share/bash-completion/completions/${program}
        rm -f ${DESTDIR}${PREFIX}/share/zsh/site-functions/_${program}
        rm -f ${DESTDIR}${PREFIX}/share/licenses/${program}/LICENSE
        ;;
    "install")
        [ ! -f "$program" ] && "$0" build
        set -x
        install -Dm755 ${program}                  ${DESTDIR}${PREFIX}/bin/${program}
        install -Dm644 ${program}.1                ${DESTDIR}${PREFIX}/man/man1/${program}.1
        install -Dm644 completions/${program}.fish ${DESTDIR}${PREFIX}/share/fish/vendor_completions.d/${program}.fish
        install -Dm644 completions/${program}.bash ${DESTDIR}${PREFIX}/share/bash-completion/completions/${program}
        install -Dm644 completions/${program}.zsh  ${DESTDIR}${PREFIX}/share/zsh/site-functions/_${program}
        install -Dm644 LICENSE                     ${DESTDIR}${PREFIX}/share/licenses/${program}/LICENSE
        ;;
    "clean")
        set -x
        rm -f *.o *~ ${program}
        ;;
    "build"|"debug"|"release"|"clang")
        ctags --kinds-C=+l ./*.h ./*.c 2>/dev/null || true
        vtags.sed tags > .tags.vim 2>/dev/null || true
        set -x
        $CC $CFLAGS $LDFLAGS -o ${program} ${src} ${LDLIBS}
        ;;
    *)
        echo "usage: $0 [ build | debug | release | clang | install | uninstall | clean ]"
        ;;
esac
