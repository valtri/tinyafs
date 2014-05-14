#include <stdio.h>

#include "browse.h"


int action(const char *path, int level, void *user __attribute__((unused))) {
	printf("%d: %s\n", level, path);

	if (level >= 2) return 1;

	return 0;
}


int main(int argn, char *argv[]) {
	if (argn <= 1) {
		printf("Usage: %s DIRECTORY\n", argv[0]);
		return 1;
	}

	return browse(argv[1], action, NULL);
}
