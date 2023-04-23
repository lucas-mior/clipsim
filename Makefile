PREFIX ?= /usr/local

objs = ipc.o util.o clipboard.o history.o text.o send_signal.o main.o

ldlibs = $(LDLIBS) -lX11 -lXfixes -pthread

all: release

.PHONY: all clean install uninstall
.SUFFIXES:
.SUFFIXES: .c .o

CC=clang

release: cflags += -O2 -Weverything
release: clipsim

debug: cflags += -DCLIPSIM_DEBUG -g -Wall -Wextra
debug: clean
debug: clipsim

clipsim: $(objs)
	ctags --kinds-C=+l *.h *.c
	vtags.sed tags > .tags.vim
	$(CC) $(cflags) $(LDFLAGS) -o $@ $(objs) $(ldlibs)

$(objs): Makefile clipsim.h

ipc.o: clipsim.h ipc.h util.h history.h text.h
clipboard.o: clipsim.h clipboard.h util.h history.h send_signal.h
util.o: clipsim.h util.h
history.o: clipsim.h util.h history.h
text.o: clipsim.h util.h text.h
send_signal.o: clipsim.h send_signal.h
main.o: clipsim.h ipc.h clipboard.h util.h history.h send_signal.h

.c.o:
	$(CC) $(cflags) $(cppflags) -c -o $@ $<

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f clipsim ${DESTDIR}${PREFIX}/bin
	cp -f clipsim.1 ${DESTDIR}${PREFIX}/man/man1/clipsim.1
	chmod 755 ${DESTDIR}${PREFIX}/bin/clipsim

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/clipsim
	rm -f ${DESTDIR}${PREFIX}/man/man1/clipsim.1

clean:
	rm -f *.o *~ clipsim
