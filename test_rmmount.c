#include <stdio.h>
#include <errno.h>

#include "tinyafs.h"


int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("Usage: %s MOUNTPOINT\n", argv[0]);
		return 1;
	}

	if (remove_mount(argv[1]) != 0) {
		perror(NULL);
		return 1;
	}

	return 0;
}
