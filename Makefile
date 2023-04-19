PREFIX ?= /usr/local

objs = comm.o util.o clip.o hist.o text.o send_signal.o main.o

ldlibs = $(LDLIBS) -lX11 -lXfixes -pthread

all: clipsim

.PHONY: all clean install uninstall
.SUFFIXES:
.SUFFIXES: .c .o

CC=clang

clipsim: $(objs)
	$(CC) -O2 -Weverything $(cflags) $(LDFLAGS) -o $@ $(objs) $(ldlibs)

# CLIPSIM_DEBUG=-DCLIPSIM_DEBUG

$(objs): Makefile clipsim.h config.h

comm.o: clipsim.h config.h comm.h util.h hist.h text.h
clip.o: clipsim.h config.h clip.h util.h hist.h send_signal.h
util.o: clipsim.h config.h util.h
hist.o: clipsim.h config.h util.h hist.h
text.o: clipsim.h config.h util.h text.h
send_signal.o: clipsim.h config.h send_signal.h
main.o: clipsim.h config.h comm.h clip.h util.h hist.h send_signal.h

.c.o:
	$(CC) -O2 -Weverything $(cflags) $(cppflags) -c -o $@ $< $(CLIPSIM_DEBUG)

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
