PREFIX ?= /usr/local

src = ipc.c util.c clipboard.c history.c content.c send_signal.c main.c
headers = clipsim.h

ldlibs = $(LDLIBS) -lX11 -lXfixes -lmagic

all: release

.PHONY: all clean install uninstall
.SUFFIXES:
.SUFFIXES: .c .o

clang: CC=clang
clang: CFLAGS += -Weverything -Wno-unsafe-buffer-usage
clang: CFLAGS += -Wno-format-nonliteral -Wno-declaration-after-statement
clang: CFLAGS += -Wno-format-zero-length
clang: clean
clang: clipsim

CFLAGS += -std=c99 -D_DEFAULT_SOURCE
CFLAGS += -Wall -Wextra

release: CFLAGS += -O2 -flto
release: clipsim

debug: CFLAGS += -g
debug: CFLAGS += -DCLIPSIM_DEBUG -fsanitize=undefined
debug: CFLAGS += -Wno-format-zero-length
debug: clean
debug: clipsim

clipsim: $(src) $(headers) Makefile
	ctags --kinds-C=+l *.h *.c
	vtags.sed tags > .tags.vim
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(src) $(ldlibs)

install: all
	install -Dm755 clipsim                  ${DESTDIR}${PREFIX}/bin/clipsim
	install -Dm644 clipsim.1                ${DESTDIR}${PREFIX}/man/man1/clipsim.1
	install -Dm644 completions/clipsim.fish ${DESTDIR}${PREFIX}/share/fish/vendor_completions.d/clipsim.fish
	install -Dm644 completions/clipsim.bash ${DESTDIR}${PREFIX}/share/bash-completion/completions/clipsim
	install -Dm644 completions/clipsim.zsh  ${DESTDIR}${PREFIX}/share/zsh/site-functions/_clipsim
	install -Dm644 LICENSE                  ${DESTDIR}${PREFIX}/share/licenses/${pkgname}/LICENSE
uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/clipsim
	rm -f ${DESTDIR}${PREFIX}/share/man/man1/clipsim.1
	rm -f ${DESTDIR}${PREFIX}/share/fish/vendor_completions.d/clipsim.fish
	rm -f ${DESTDIR}${PREFIX}/share/bash-completion/completions/clipsim
	rm -f ${DESTDIR}${PREFIX}/share/zsh/site-functions/_clipsim
	rm -f ${DESTDIR}${PREFIX}/share/licenses/${pkgname}/LICENSE

clean:
	rm -f *.o *~ clipsim
