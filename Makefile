AFS_PREFIX=/usr
AFS_CPPFLAGS=-I$(AFS_PREFIX)/include -DVENUS=1
AFS_LIBS=-L$(AFS_PREFIX)/lib/afs -lsys

CC=gcc
CPPFLAGS=$(AFS_CPPFLAGS)
CFLAGS=-W -Wall -g -O0

all: test

test: tinyafs.o test.o
	$(CC) $(LDFLAGS) $+ -o $@ $(AFS_LIBS)

clean:
	rm -fv *.o test

test: tinyafs.h
tinyafs.o: tinyafs.h
