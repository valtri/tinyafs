#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <afs/afssyscalls.h>
#include <afs/afs_consts.h>
#include <afs/vioc.h>
#include <afs/prs_fs.h>
#include "tinyafs.h"


#define MAX_CELLNAME 512

int has_afs() {
	struct ViceIoctl iob;

	memset(&iob, 0, sizeof iob);
	lpioctl(NULL, VIOCSETTOK, &iob, 0);

	return errno == EINVAL;
}


int get_cell(const char *fname, char **cellname) {
	int code;
	struct ViceIoctl blob;
	char buf[MAX_CELLNAME];

	blob.in_size = 0;
	blob.out_size = MAX_CELLNAME;
	blob.out = buf;

	memset(buf, 0, sizeof(buf));
	code = lpioctl(fname, VIOC_FILE_CELL_NAME, &blob, 1);

	if (code  == 0 ) {
		*cellname = strdup(buf);
		return 0;
	} else {
		*cellname = NULL;
		return errno;
	}
}


void free_acl(struct Acl *acl) {
	free(acl->plus);
	free(acl->minus);
	memset(acl, 0, sizeof(struct Acl));
}


void rights2str(int rights, char *s) {
	char *p;

	p = s;

	if (rights & PRSFS_READ) (p++)[0] = 'r';
	if (rights & PRSFS_LOOKUP) (p++)[0] = 'l';
	if (rights & PRSFS_INSERT) (p++)[0] = 'i';
	if (rights & PRSFS_DELETE) (p++)[0] = 'd';
	if (rights & PRSFS_WRITE) (p++)[0] = 'w';
	if (rights & PRSFS_LOCK) (p++)[0] = 'k';
	if (rights & PRSFS_ADMINISTER) (p++)[0] = 'a';
	if (rights & PRSFS_USR0) (p++)[0] = 'A';
	if (rights & PRSFS_USR1) (p++)[0] = 'B';
	if (rights & PRSFS_USR2) (p++)[0] = 'C';
	if (rights & PRSFS_USR3) (p++)[0] = 'D';
	if (rights & PRSFS_USR4) (p++)[0] = 'E';
	if (rights & PRSFS_USR5) (p++)[0] = 'F';
	if (rights & PRSFS_USR6) (p++)[0] = 'G';
	if (rights & PRSFS_USR7) (p++)[0] = 'H';
	p[0] = '\0';
}


void dump_acl(const struct Acl *acl) {
	size_t i;
	struct AclEntry *entry;
	char rights[MAXRIGHTS];

	if (acl->nplus) {
		printf("Rights:\n");
		for (i = 0; i < acl->nplus; i++) {
			entry = acl->plus + i;
			rights2str(entry->rights, rights);
			printf("  %-8s %s\n", rights, entry->name);
		}
	}
	if (acl->nminus) {
		printf("Negative rights:\n");
		for (i = 0; i < acl->nminus; i++) {
			entry = acl->minus + i;
			rights2str(entry->rights, rights);
			printf("  %-8s %s\n", rights, entry->name);
		}
	}
}


static const char* nextline(const char *s) {
	while (*s && *s != '\n') s++;
	if (*s) s++;

	return s;
}


static int parse_acl(const char *s, struct Acl *acl) {
	char buf[1024];
	struct AclEntry *entry;
	int dfs;
	const char *p;
	size_t i;

	memset(acl, 0, sizeof(struct Acl));
	p = s;
	sscanf(p, "%lu dfs:%d %1024s", &acl->nplus, &dfs, buf /* cell */);
	p = nextline(p);
	sscanf(p, "%lu", &acl->nminus);
	p = nextline(p);

	if (acl->nplus) acl->plus = calloc(acl->nplus, sizeof(struct AclEntry));
	for (i = 0; i < acl->nplus; i++) {
		entry = acl->plus + i;
		entry->name[0] = '\0';
		entry->rights = 0;
		sscanf(p, "%99s %d", entry->name, &entry->rights);
		p = nextline(p);
	}

	if (acl->nminus) acl->minus = calloc(acl->nminus, sizeof(struct AclEntry));
	for (i = 0; i < acl->nminus; i++) {
		entry = acl->minus + i;
		entry->name[0] = '\0';
		entry->rights = 0;
		sscanf(p, "%99s %d", entry->name, &entry->rights);
		p = nextline(p);
	}

	return 0;
}


int get_acl(const char *fname, struct Acl *acl) {
	char *space;
	struct ViceIoctl blob;
	int idf = 0; /* 0 = curent, 1 = initial dir , 2 = initial file */
	int code;

	space = calloc(1, AFS_PIOCTL_MAXSIZE);
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.in_size = idf;
	blob.in = blob.out = space;
	code = lpioctl(fname, VIOCGETAL, &blob, 1);
	if (code) goto err;

	parse_acl(space, acl);
err:
	free(space);
	return code;
}
