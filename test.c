#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinyafs.h"


int main(int argn, char *argc[]) {
	char *cell;
	struct Acl acl;
	const char *path = "/afs/zcu.cz";

	if (argn > 1) path = argc[1];
	printf("using path: %s\n", path);

	if (!has_afs()) {
		printf("has AFS: no\n");
		return 1;
	}
	printf("has AFS: yes\n");

	if (get_cell(path, &cell) != 0) {
		cell = strdup("(error)");
	}
	printf("cell: %s\n", cell);
	free(cell);

	if (get_acl(path, &acl) != 0) {
		perror(NULL);
	} else {
		printf("acl: ");
		dump_acl(&acl);
		free_acl(&acl);
	}

	return 0;
}
