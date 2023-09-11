PREFIX ?= /usr/local

objs = ipc.o util.o clipboard.o history.o content.o send_signal.o main.o

ldlibs = $(LDLIBS) -lX11 -lXfixes -lmagic

all: release

.PHONY: all clean install uninstall
.SUFFIXES:
.SUFFIXES: .c .o

CC=tcc

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
debug: CFLAGS += -DCLIPSIM_DEBUG
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
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f clipsim ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${PREFIX}/man/man1/
	cp -f clipsim.1 ${DESTDIR}${PREFIX}/man/man1/clipsim.1
	chmod 755 ${DESTDIR}${PREFIX}/bin/clipsim
	chmod 644 ${DESTDIR}${PREFIX}/man/man1/clipsim.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/clipsim
	rm -f ${DESTDIR}${PREFIX}/man/man1/clipsim.1

clean:
	rm -f *.o *~ clipsim
