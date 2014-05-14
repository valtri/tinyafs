-include Makefile.inc

libdir?=lib
AFS_PREFIX?=/usr
AFS_CPPFLAGS?=-I$(AFS_PREFIX)/include -DVENUS=1
AFS_LIBS?=-L$(AFS_PREFIX)/$(libdir)/afs -lsys

LBU_CPPFLAGS=
LBU_LIBS=-lglite_lbu_db

CC=gcc
CPPFLAGS?=-D_GNU_SOURCE $(AFS_CPPFLAGS) $(LBU_CPPFLAGS)
CFLAGS?=-W -Wall -g -O0

BINS=scan test_browse test_tinyafs

all: $(BINS)

scan: scan.o browse.o list.o tinyafs.o
	$(CC) $(LDFLAGS) $+ -o $@ $(AFS_LIBS) $(LBU_LIBS)

test_browse: test_browse.o browse.o list.o
	$(CC) $(LDFLAGS) $+ -o $@ $(AFS_LIBS)

test_tinyafs: tinyafs.o test_tinyafs.o
	$(CC) $(LDFLAGS) $+ -o $@ $(AFS_LIBS)

clean:
	rm -fv *.o $(BINS)

browse.o: list.h
list.o: list.h
scan.o: browse.h tinyafs.h
test_browse.o: browse.h
test_tinyafs.o: tinyafs.h
tinyafs.o: tinyafs.h
