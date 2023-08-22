PREFIX ?= /usr/local

objs = ipc.o util.o clipboard.o history.o content.o send_signal.o main.o

ldlibs = $(LDLIBS) -lX11 -lXfixes -lmagic

all: release

.PHONY: all clean install uninstall
.SUFFIXES:
.SUFFIXES: .c .o

CC=clang

cflags += -std=c99 -D_DEFAULT_SOURCE
release: cflags += -O2 -Weverything
release: stripflag = -s
release: clipsim

debug: cflags += -g -Wall -Wextra
debug: clean
debug: clipsim

clipsim: $(objs)
	ctags --kinds-C=+l *.h *.c
	vtags.sed tags > .tags.vim
	$(CC) $(stripflag) $(cflags) $(LDFLAGS) -o $@ $(objs) $(ldlibs)

$(objs): Makefile clipsim.h

.c.o:
	$(CC) $(cflags) $(cppflags) -c -o $@ $<

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
