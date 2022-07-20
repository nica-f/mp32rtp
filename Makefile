CFLAGS=-Wall -Wuninitialized -O2
LDFLAGS=-lm -g

INSTDIR=/usr/local/bin

DISTFILES=README INSTALL ChangeLog Makefile mp32rtp.c mp3parse.h \
	mp3radio2rtp mp3dir2rtp

all: mp32rtp

mp32rtp: mp32rtp.c mp3parse.h
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $<
	
install: mp32rtp mp3dir2rtp mp3radio2rtp
	install -c mp32rtp -m 755 $(INSTDIR)
	install -c mp3dir2rtp -m 755 $(INSTDIR)
	install -c mp3radio2rtp -m 755 $(INSTDIR)

uninstall:
	rm -rf $(INSTDIR)/mp32rtp $(INSTDIR)/mp3dir2rtp $(INSTDIR)/mp3radio2rtp

clean:
	rm mp32rtp

dist: $(DISTFILES)
	cd ..; tar czvf mp32rtp.tgz $(patsubst %,mp32rtp/%,$^)
