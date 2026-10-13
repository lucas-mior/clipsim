# This makefile builds libryu.a. Mostly useful for building
# the lib as a dependency in non-Bazel projects (e.g. CMake).
# Contributed by @gritzko. Supported on a best-effort basis.

SRC=d2fixed.c d2s.c f2s.c generic_128.c s2d.c s2f.c

OBJ = $(SRC:.c=.o)

ALIB = libryu.a

INCLUDES = -I..

.SUFFIXES: .c

default: $(ALIB)

.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) -c $< -o $@

$(ALIB): $(OBJ)
	$(AR) rcs $(ALIB) $(OBJ)

libs: $(ALIB)

.PHONY: install
install: libs
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/include
	cp $(ALIB) $(DESTDIR)$(PREFIX)/lib/$(ALIB)
	cp ryu.h $(DESTDIR)$(PREFIX)/include/
	cp ryu_parse.h $(DESTDIR)$(PREFIX)/include/
	cp ryu_generic_128.h $(DESTDIR)$(PREFIX)/include/

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/lib/$(ALIB)
	rm -f $(DESTDIR)$(PREFIX)/include/ryu.h
	rm -f $(DESTDIR)$(PREFIX)/include/ryu_parse.h
	rm -f $(DESTDIR)$(PREFIX)/include/ryu_generic_128.h

.PHONY: clean
clean:
	rm -f $(OBJ) $(ALIB)
