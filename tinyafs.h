#ifndef TINYAFS_H
#define TINYAFS_H

#include <stddef.h>


#define MAXNAME 100
#define MAXRIGHTS 16

struct Acl {
	size_t nplus;
	size_t nminus;
	struct AclEntry *plus;
	struct AclEntry *minus;
};

struct AclEntry {
	char name[MAXNAME];
	int rights;
};


void rights2str(int rights, char *s);

void init_acl(struct Acl *acl, size_t nplus, size_t nminus);
void free_acl(struct Acl *acl);
void dump_acl(const struct Acl *acl);

int has_afs();
int get_cell(const char *fname, char **cellname);
int get_acl(const char *fname, struct Acl *acl);
int list_mount(const char *path, char **mount);
int remove_mount(const char *path);
int make_mount(const char *path, const char *volume, int rw, const char *cell);

#endif
