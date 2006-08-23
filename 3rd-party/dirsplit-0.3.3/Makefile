
DESTDIR=/usr/local

all:
	@echo Nothing to do, try \"make install\"

clean:
	rm -f *~ *.list log*

install:
	mkdir -p $(DESTDIR)/bin $(DESTDIR)/share/doc $(DESTDIR)/share/man/man1
	install dirsplit $(DESTDIR)/bin
	install -m 644 dirsplit.1 $(DESTDIR)/share/man/man1
	install -m 644 README ChangeLog $(DESTDIR)/share/doc

