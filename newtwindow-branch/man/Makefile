# a BSD compatible 'install' program
INSTALL = install

include ../config.mak

all:
clean:
distclean:
	echo "nothing to do"
install:
	$(INSTALL) -D -b -c -m 644 beye.1 $(DESTDIR)$(MANDIR)/man1/beye.1
uninstall:
	rm -f $(DESTDIR)$(MANDIR)/man1/beye.1
	rmdir -p --ignore-fail-on-non-empty $(DESTDIR)$(MANDIR)/man1
dep:
