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


int has_afs();
int get_cell(const char *fname, char **cellname);

void free_acl(struct Acl *acl);
void dump_acl(const struct Acl *acl);
void rights2str(int rights, char *s);
int get_acl(const char *fname, struct Acl *acl);

#endif
