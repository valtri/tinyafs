#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <afs/afssyscalls.h>
#ifdef VENUS
#include <netinet/in.h>
#include <afs/venus.h>
#else
#include <afs/afs_consts.h>
#include <afs/vioc.h>
#endif
#include <afs/prs_fs.h>
#include "tinyafs.h"


#define MAX_CELLNAME 512
#ifndef AFS_PIOCTL_MAXSIZE
#define AFS_PIOCTL_MAXSIZE 2048
#endif


static int pioctl(const char *path, afs_int32 cmd, struct ViceIoctl *data, afs_int32 follow) {
	/* XXX: proper way would be to use strdup() on the path */
	return lpioctl((char *)path, cmd, (char *)data, follow);
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


void init_acl(struct Acl *acl, size_t nplus, size_t nminus) {
	memset(acl, 0, sizeof(struct Acl));
	if (nplus) {
		acl->plus = calloc(nplus, sizeof(struct AclEntry));
		acl->nplus = nplus;
	}
	if (nminus) {
		acl->minus = calloc(nminus, sizeof(struct AclEntry));
		acl->nminus = nminus;
	}
}


void free_acl(struct Acl *acl) {
	free(acl->plus);
	free(acl->minus);
	memset(acl, 0, sizeof(struct Acl));
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


int has_afs() {
	struct ViceIoctl iob;

	memset(&iob, 0, sizeof iob);
	pioctl(NULL, VIOCSETTOK, &iob, 0);

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
	code = pioctl(fname, VIOC_FILE_CELL_NAME, &blob, 1);

	if (code  == 0 ) {
		*cellname = strdup(buf);
		return 0;
	} else {
		*cellname = NULL;
		return errno;
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

	p = s;
	sscanf(p, "%zu dfs:%d %1024s", &acl->nplus, &dfs, buf /* cell */);
	p = nextline(p);
	sscanf(p, "%zu", &acl->nminus);
	p = nextline(p);

	init_acl(acl, acl->nplus, acl->nminus);
	for (i = 0; i < acl->nplus; i++) {
		entry = acl->plus + i;
		entry->name[0] = '\0';
		entry->rights = 0;
		sscanf(p, "%99s %d", entry->name, &entry->rights);
		p = nextline(p);
	}
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
	code = pioctl(fname, VIOCGETAL, &blob, 1);
	if (code) goto err;

	parse_acl(space, acl);
err:
	free(space);
	return code;
}


static void get_parent(const char *path, char **parent, char **name) {
	const char *last_slash;

	last_slash = strrchr(path, '/');
	if (last_slash == path) {
		/* parent is the root directory */
		*parent = strdup("/");
		*name = strdup(last_slash + 1);
	} else if (last_slash) {
		/* found both components */
		*parent = strndup(path, last_slash - path);
		*name = strdup(last_slash + 1);
	} else {
		/* no slash, the parent is the current directory */
		*parent = strdup(".");
		*name = strdup(path);
	}
}


int list_mount(const char *path, char **mount) {
	struct ViceIoctl blob;
	char *space = NULL, *parent = NULL, *name = NULL;
	int code;

	if (mount) *mount = NULL;

	get_parent(path, &parent, &name);
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
		errno = code = EINVAL;
		goto err;
	}

	space = calloc(1, AFS_PIOCTL_MAXSIZE);
	blob.in = name;
	blob.in_size = strlen(name) + 1;
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.out = space;

	code = pioctl(parent, VIOC_AFS_STAT_MT_PT, &blob, 1);
	if (code == 0) {
		if (mount) {
			*mount = space;
			space = NULL;
		}
	}
err:
	free(space);
	free(parent);
	free(name);
	return code;
}


int remove_mount(const char *path) {
	struct ViceIoctl blob;
	struct stat info;
	char *parent = NULL, *name = NULL;
	int code;

	/* lstat() may fail, but pioctl() can be still successfull */
	if (lstat(path, &info) == 0) {
		/* we won't support symlinks (requires translation of relative links, ...) */
		if (S_ISLNK(info.st_mode)) { errno = ENOTDIR; return ENOTDIR; }
		if (!S_ISDIR(info.st_mode)) { errno = ENOTDIR; return ENOTDIR; }
	}

	get_parent(path, &parent, &name);
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
		errno = code = EINVAL;
		goto err;
	}

	blob.out_size = 0;
	blob.in = name;
	blob.in_size = strlen(name) + 1;
	code = pioctl(parent, VIOC_AFS_DELETE_MT_PT, &blob, 1);
err:
	free(parent);
	free(name);
	return code;
}


int make_mount(const char *path, const char *volume, int rw, const char *cell) {
	char space[512];

	if (rw) strcpy(space, "%");
	else strcpy(space, "#");
	if (cell) {
		strncat(space, cell, sizeof space);
	}
	strncat(space, volume, sizeof space);
	strncat(space, ".", sizeof space);

	return symlink(space, path);
}
