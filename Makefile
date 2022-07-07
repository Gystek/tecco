# tecco
include config.mk

SRC=tecco.c
OBJ=$(SRC:.c=.o)

all: libtecco.a
libtecco.a: $(OBJ)
	$(AR) -rcs $@ $^

clean:
	rm -f $(OBJ)
	rm -f libtecco.a
dist: clean
	mkdir -p tecco-$(VERSION)
	cp -R LICENSE Makefile README.md tecco.pod $(SRC) config.mk\
		tecco.h\
		tecco-$(VERSION)
	tar -cf - tecco-$(VERSION) | gzip > tecco-$(VERSION).tar.gz
	rm -rf tecco-$(VERSION)
install: libtecco.a
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	cp -f libtecco.a $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/include
	cp -f tecco.h $(DESTDIR)$(PREFIX)/include
	pod2man tecco.pod --date 2022-07-07 --section 3 --center "" --release "tecco-VERSION" >> tecco.3
	mkdir -p $(DESTDIR)$(MANPREFIX)/man3
	sed "s/VERSION/$(VERSION)/g" < tecco.3 > $(DESTDIR)$(MANPREFIX)/man3/tecco.3
	chmod 644 $(DESTDIR)$(MANPREFIX)/man3/tecco.3
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/lib/libtecco.3
	rm -f $(DESTDIR)$(MANPREFIX)/man3/tecco.3

.PHONY: all clean dist install uninstall
