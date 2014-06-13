-include Makefile.inc

prefix=/usr
libdir?=lib
AFS_PREFIX?=/usr
AFS_CPPFLAGS?=-I$(AFS_PREFIX)/include -DVENUS=1
AFS_LIBS?=-L$(AFS_PREFIX)/$(libdir)/afs -lsys

LBU_CPPFLAGS=
LBU_LIBS=-lglite_lbu_db -lglite_lbu_log

CC=gcc
CPPFLAGS:=-D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -pthread $(AFS_CPPFLAGS) $(LBU_CPPFLAGS) $(CPPFLAGS)
CFLAGS:=-W -Wall -g -O2 $(CFLAGS)
ifeq ($(COVERAGE),)
else
CFLAGS+=-pg -fprofile-arcs -ftest-coverage
LDFLAGS+=-pg -fprofile-arcs -ftest-coverage -static
endif

COMPILE=libtool --mode=compile $(CC) $(CFLAGS) $(CPPFLAGS)
LINK=libtool --mode=link $(CC) $(LDFLAGS)
INSTALL=libtool --mode=install install

BINS=scan-sql scan-mountpoints scan-rights
TESTS=test_array test_browse test_rmmount test_tinyafs

all: $(BINS) $(TESTS)

libtinyafs.la: array.lo tinyafs.lo browse.lo
	$(LINK) -pthread -rpath $(prefix)/$(libdir) $+ -o $@ $(AFS_LIBS)

scan-sql: scan_sql.lo libtinyafs.la
	$(LINK) $(LDFLAGS) -pthread $+ -o $@ $(LBU_LIBS)

scan-mountpoints: scan_mountpoints.lo libtinyafs.la
	$(LINK) $(LDFLAGS) -pthread $+ -o $@

scan-rights: scan_rights.lo libtinyafs.la
	$(LINK) $(LDFLAGS) $+ -o $@

test_array: test_array.lo libtinyafs.la
	$(LINK) $(LDFLAGS) $+ -o $@

test_browse: test_browse.lo libtinyafs.la
	$(LINK) $(LDFLAGS) $+ -o $@

test_rmmount: test_rmmount.lo libtinyafs.la
	$(LINK) $(LDFLAGS) $+ -o $@

test_tinyafs: test_tinyafs.lo libtinyafs.la
	$(LINK) $(LDFLAGS) $+ -o $@

install:
	mkdir -p $(DESTDIR)$(prefix)/bin $(DESTDIR)$(prefix)/$(libdir) $(DESTDIR)$(prefix)/include $(DESTDIR)$(prefix)/share/man/man1 $(DESTDIR)$(prefix)/share/tinyafs
	$(INSTALL) $(BINS) $(DESTDIR)$(prefix)/bin
	$(INSTALL) libtinyafs.la $(DESTDIR)$(prefix)/$(libdir)
	$(INSTALL) -m 0644 *.h $(DESTDIR)$(prefix)/include
	$(INSTALL) -m 0644 doc/*.1 $(DESTDIR)$(prefix)/share/man/man1
	$(INSTALL) -m 0644 *.sql $(DESTDIR)$(prefix)/share/tinyafs
	$(INSTALL) -m 0755 post-import-points.pl refresh.sh $(DESTDIR)$(prefix)/share/tinyafs

clean:
	rm -rfv *.o *.lo *.la .libs/ $(BINS) $(TESTS)
	rm -fv *.gcno *.gcda gmon.out

%.lo: %.c
	$(COMPILE) -c $<

array.lo: array.h
scan_mountpoints.lo: browse.h tinyafs.h
scan_sql.lo: browse.h tinyafs.h
test_browse.lo: browse.h
test_tinyafs.lo: tinyafs.h
tinyafs.lo: tinyafs.h

.PHONY: all install clean
