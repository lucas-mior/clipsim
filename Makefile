PREFIX ?= /usr/local

objs = ipc.o util.o clipboard.o history.o content.o send_signal.o main.o

ldlibs = $(LDLIBS) -lX11 -lXfixes -lmagic

all: release

.PHONY: all clean install uninstall
.SUFFIXES:
.SUFFIXES: .c .o

CC=cc
ifeq ($(CC),clang)
	CFLAGS += -Weverything -Wno-unsafe-buffer-usage -Wno-format-nonliteral
else
	CFLAGS += -Wextra -Wall
endif


CFLAGS += -std=c99 -D_DEFAULT_SOURCE
CFLAGS += -Wno-declaration-after-statement

release: CFLAGS += -O2
release: clipsim

debug: CFLAGS += -g
debug: CFLAGS += -DCLIPSIM_DEBUG -fsanitize=undefined
debug: clean
debug: clipsim

clipsim: $(objs)
	ctags --kinds-C=+l *.h *.c
	vtags.sed tags > .tags.vim
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(objs) $(ldlibs)

$(objs): Makefile clipsim.h

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

install: all
	install -Dm755 clipsim ${DESTDIR}${PREFIX}/bin/clipsim
	install -Dm644 clipsim.1 ${DESTDIR}${PREFIX}/man/man1/clipsim.1
	install -Dm644 completions/fish/clipsim.fish "${pkgdir}/usr/share/fish/vendor_completions.d/clipsim.fish"
	install -Dm644 completions/bash/clipsim-completion.bash "${pkgdir}/usr/share/bash-completion/completions/clipsim"
	install -Dm644 completions/zsh/_clipsim "${pkgdir}/usr/share/zsh/site-functions/_clipsim"
uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/clipsim
	rm -f ${DESTDIR}${PREFIX}/share/man/man1/clipsim.1
	rm -f "${pkgdir}/usr/share/fish/vendor_completions.d/clipsim.fish"
	rm -f "${pkgdir}/usr/share/bash-completion/completions/clipsim"
	rm -f "${pkgdir}/usr/share/zsh/site-functions/_clipsim"

clean:
	rm -f *.o *~ clipsim
