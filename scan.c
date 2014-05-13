#include <stdio.h>
#include <stdlib.h>

#include "browse.h"
#include "tinyafs.h"


int action(const char *path, int level) {
	char *mount;

	if (level >= 200) {
		fprintf(stderr, "level too high, skipping: %s\n", path);
		return 1;
	}

	if (list_mount(path, &mount) == 0) {
		printf("%d: %s, mountpoint: %s\n", level, path, mount);
		free(mount);
		if (level != 0) return 1;
	} else {
		printf("%d: %s\n", level, path);
	}

	return 0;
}


int main(int argc, char *argv[]) {
	if (argc <= 1) {
		printf("Usage: %s DIRECTORY\n", argv[0]);
		return 1;
	}

	if (!has_afs()) {
		fprintf(stderr, "has_afs() failed\n");
		return 2;
	}

	return browse(argv[1], action);
}
