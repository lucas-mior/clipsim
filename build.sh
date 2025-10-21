#!/bin/sh

# shellcheck disable=SC2086

target="${1:-build}"
PREFIX="${PREFIX:-/usr/local}"
DESTDIR="${DESTDIR:-/}"

src="main.c"
program="clipsim"

CC=${CC:-cc}

LDLIBS="$LDLIBS $(pkg-config x11 --libs)"
LDLIBS="$LDLIBS $(pkg-config xfixes --libs)"
LDLIBS="$LDLIBS $(pkg-config xi --libs)"
LDLIBS="$LDLIBS $(pkg-config libmagic --libs)"
LDLIBS="$LDLIBS -lpthread"

CFLAGS="$CFLAGS -std=c99 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600"
CFLAGS="$CFLAGS -Wall -Wextra"
CFLAGS="$CFLAGS -Wno-unused-function -Wno-constant-logical-operand"

if [ "$CC" = "clang" ]; then
    CFLAGS="$CFLAGS -Weverything"
    CFLAGS="$CFLAGS -Wno-unsafe-buffer-usage"
    CFLAGS="$CFLAGS -Wno-format-nonliteral"
    CFLAGS="$CFLAGS -Wno-format-zero-length"
    CFLAGS="$CFLAGS -Wno-implicit-void-ptr-cast"
    CFLAGS="$CFLAGS -Wno-declaration-after-statement"
fi

echo "target=$target"

case "$target" in
    "debug")
        CFLAGS="$CFLAGS -g -fsanitize=undefined -DDEBUGGING=1"
        CFLAGS="$CFLAGS -Wno-format-zero-length"
        ;;
    "release"|"build")
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
    "build"|"debug"|"release")
        ctags --kinds-C=+l ./*.h ./*.c 2>/dev/null || true
        vtags.sed tags > .tags.vim 2>/dev/null || true
        set -x
        $CC $CFLAGS $LDFLAGS -o ${program} ${src} ${LDLIBS}
        ;;
    *)
        echo "usage: $0 [ build | debug | release | clang | install | uninstall | clean ]"
        ;;
esac
