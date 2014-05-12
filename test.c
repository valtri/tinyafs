#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinyafs.h"


int main() {
	char *cell;
	struct Acl acl;

	if (!has_afs()) {
		printf("has AFS: no\n");
		return 1;
	}
	printf("has AFS: yes\n");

	if (get_cell("/afs/zcu.cz", &cell) != 0) {
		cell = strdup("(error)");
	}
	printf("cell: %s\n", cell);
	free(cell);

	if (get_acl("/afs/zcu.cz", &acl) != 0) {
		perror(NULL);
	} else {
		printf("acl: ");
		dump_acl(&acl);
		free_acl(&acl);
	}

	return 0;
}
