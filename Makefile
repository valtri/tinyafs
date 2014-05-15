-include Makefile.inc

libdir?=lib
AFS_PREFIX?=/usr
AFS_CPPFLAGS?=-I$(AFS_PREFIX)/include -DVENUS=1
AFS_LIBS?=-L$(AFS_PREFIX)/$(libdir)/afs -lsys

LBU_CPPFLAGS=
LBU_LIBS=-lglite_lbu_db -lglite_lbu_log

CC=gcc
CPPFLAGS?=-D_GNU_SOURCE -pthread $(AFS_CPPFLAGS) $(LBU_CPPFLAGS)
CFLAGS?=-W -Wall -g -O0

BINS=scan test_browse test_rmmount test_tinyafs

all: $(BINS)

scan: scan.o browse.o array.o tinyafs.o
	$(CC) $(LDFLAGS) -pthread $+ -o $@ $(AFS_LIBS) $(LBU_LIBS)

test_browse: test_browse.o browse.o array.o
	$(CC) $(LDFLAGS) $+ -o $@ $(AFS_LIBS)

test_rmmount: test_rmmount.o tinyafs.o
	$(CC) $(LDFLAGS) $+ -o $@ $(AFS_LIBS)

test_tinyafs: tinyafs.o test_tinyafs.o
	$(CC) $(LDFLAGS) $+ -o $@ $(AFS_LIBS)

clean:
	rm -fv *.o $(BINS)

array.o: array.h
scan.o: browse.h tinyafs.h
test_browse.o: browse.h
test_tinyafs.o: tinyafs.h
tinyafs.o: tinyafs.h
