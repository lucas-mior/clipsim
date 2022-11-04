PREFIX ?= /usr/local

objs = comm.o util.o clip.o hist.o sig_dwmblocks.o main.o

ldlibs = $(LDLIBS) -lX11 -lXfixes -pthread

all: clipsim

.PHONY: all clean install uninstall
.SUFFIXES:
.SUFFIXES: .c .o

CC=clang

clipsim: $(objs)
	$(CC) -O2 -Weverything $(cflags) $(LDFLAGS) -o $@ $(objs) $(ldlibs)
	ctags --kinds-C=+l *.h *.c
	vtags.sed tags > .tags.vim

bear: Makefile
	bear -- make > compile_commands.json
$(objs): Makefile clipsim.h config.h

comm.o: clipsim.h config.h comm.h util.h hist.h
clip.o: clipsim.h config.h clip.h util.h hist.h sig_dwmblocks.h
util.o: clipsim.h config.h util.h
hist.o: clipsim.h config.h util.h hist.h
sig_dwmblocks.o: clipsim.h config.h sig_dwmblocks.h
main.o: clipsim.h config.h comm.h clip.h util.h hist.h sig_dwmblocks.h

.c.o:
	$(CC) -O2 -Weverything $(cflags) $(cppflags) -c -o $@ $<

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
