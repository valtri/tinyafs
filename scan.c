#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glite/lbu/db.h>

#include "browse.h"
#include "tinyafs.h"



char *dbhost = "localhost";
char *dbname = "afs";
char *dbuser = "afs";
char *pass = "";
char *dbcs = NULL;

char *basedir = NULL;
size_t basedir_len = 0;
char *volume = NULL;
glite_lbu_DBContext db;
glite_lbu_Statement points_stmt, rights_stmt;
int err = 0;


static void print_DBError(glite_lbu_DBContext db) {
	char *error, *desc;

	glite_lbu_DBError(db, &error, &desc);
	fprintf(stderr, "DB error: %s: %s\n", error, desc);
	free(error);
	free(desc);
}


static void save_rights(glite_lbu_Statement stmt, const char *volume, const char *relpath, const struct AclEntry *entry, int negative) {
	char rights[MAXRIGHTS + 2];

	if (negative) {
		rights[0] = '-';
		negative = 1;
	}
	rights2str(entry->rights, rights + negative);

	if (glite_lbu_ExecPreparedStmt(stmt, 4,
		GLITE_LBU_DB_TYPE_VARCHAR, volume,
		GLITE_LBU_DB_TYPE_VARCHAR, relpath,
		GLITE_LBU_DB_TYPE_VARCHAR, entry->name,
		GLITE_LBU_DB_TYPE_VARCHAR, rights
	    ) != 1) {
		print_DBError(db);
		err = 1;
	}
}


static void save_point(glite_lbu_Statement stmt, const char *volume, const char *relpath, const char *mount) {
	if (glite_lbu_ExecPreparedStmt(stmt, 3,
		GLITE_LBU_DB_TYPE_VARCHAR, volume,
		GLITE_LBU_DB_TYPE_VARCHAR, relpath,
		GLITE_LBU_DB_TYPE_VARCHAR, mount
	    ) != 1) {
		print_DBError(db);
		err = 1;
	}
}


static int action(const char *path, int level) {
	char *mount;
	const char *relpath;
	struct Acl acl;
	size_t i;

	if (level >= 200) {
		fprintf(stderr, "level too high, skipping: %s\n", path);
		return 1;
	}

	if (strncmp(path, basedir, basedir_len) == 0) relpath = path + basedir_len;
	else relpath = path;
	if (relpath[0] == '/') relpath++;

	if (list_mount(path, &mount) == 0) {
		printf("%d: %s, mountpoint: %s\n", level, path, mount);
		if (level != 0) {
			save_point(points_stmt, volume, relpath, mount);
			free(mount);
			return 1;
		} else {
			free(mount);
		}
	} else {
		/*printf("%d: %s\n", level, path);*/
		if (level <= 2) printf("%02d: %s:%s\n", level, volume, relpath);
	}

	if (get_acl(path, &acl) != 0) {
		fprintf(stderr, "error getting ACLs for '%s': %s\n", path, strerror(errno));
		err = 1;
		return 0;
	}
	for (i = 0; i < acl.nplus; i++) save_rights(rights_stmt, volume, relpath, acl.plus + i, 0);
	for (i = 0; i < acl.nminus; i++) save_rights(rights_stmt, volume, relpath, acl.minus + i, 1);

	return 0;
}


int main(int argc, char *argv[]) {
	int retval = 2;

	if (argc <= 2) {
		printf("Usage: %s VOLUME DIRECTORY\n", argv[0]);
		return 1;
	}
	volume = argv[1];
	basedir = argv[2];
	basedir_len = strlen(argv[2]);

	if (!has_afs()) {
		fprintf(stderr, "has_afs() failed\n");
		return 2;
	}

	asprintf(&dbcs, "%s/%s@%s:%s", dbuser, pass, dbhost, dbname);
	if (glite_lbu_InitDBContext(&db, GLITE_LBU_DB_BACKEND_MYSQL, "DB") != 0) {
		fprintf(stderr, "DB module initialization failed\n\n");
		return 2;
	}

	if (glite_lbu_DBConnect(db, dbcs) != 0) goto dberr;
	if (glite_lbu_PrepareStmt(db, "INSERT INTO mountpoints (pointvolume, pointdir, volume) VALUES (?, ?, ?)", &points_stmt) != 0) goto dberr;
	if (glite_lbu_PrepareStmt(db, "INSERT INTO rights (volume, dir, login, rights) VALUES (?, ?, ?, ?)", &rights_stmt) != 0) goto dberr;

	retval = browse(basedir, action);
	if (err) retval = 2;

	glite_lbu_FreeStmt(&points_stmt);
	glite_lbu_FreeStmt(&rights_stmt);
	glite_lbu_DBClose(db);

err:
	free(dbcs);
	glite_lbu_FreeDBContext(db);

	return retval;
dberr:
	print_DBError(db);
	goto err;
}
