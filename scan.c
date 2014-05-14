#include <sys/time.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glite/lbu/db.h>
#include <glite/lbu/log.h>

#include "browse.h"
#include "tinyafs.h"


#define DEFAULT_DBHOST "localhost"
#define DEFAULT_DBNAME "afs"
#define DEFAULT_DBUSER "afs"
#define DEFAULT_DBPASS ""

#define DEFAULT_DBCS DEFAULT_DBNAME "/" DEFAULT_DBPASS "@" DEFAULT_DBHOST ":" DEFAULT_DBNAME

#define EXIT_OK 0
#define EXIT_ERRORS 1
#define EXIT_FATAL 2

#define COUNT_COMMIT 5000
#define COUNT_PROGRESS 100000



typedef struct {
	char *basedir; /* start directory */
	char *volume;  /* name of volume */

	size_t basedir_len;
	glite_lbu_DBContext db;
	glite_lbu_Statement points_stmt, rights_stmt;

	int was_err;
	size_t count;
	struct timeval begin, end;
} ctx_t;



char *program_name = NULL;
char *dbcs = NULL;

ctx_t ctx = {
	basedir: NULL,
	basedir_len: 0,
	volume: NULL,
	was_err: 0,
	count: 0,
};

const char *optstring = "hc:";

const struct option longopts[] = {
	{ "help", no_argument, NULL, 'h' },
	{ "config", required_argument, NULL, 'c' },
	{ NULL, 0, NULL, 0 }
};



static void usage() {
	printf("%s [OPTIONS] VOLUME DIRECTORY\n\
OPTIONS are:\n\
  -h, --help .......... help message\n\
  -c, --config=FILE ... config file\n\
", program_name);
}


static void split_key_value(char *buf, char **result_key, char **result_value) {
	char *delim, *key, *value;
	size_t i;

	delim = strchr(buf, '=');
	if (delim) delim[0] = 0;

	key = buf;
	if (key) key += strspn(buf, " \t");

	value = delim ? delim + 1 : NULL;
	if (value) {
		i = strlen(value);
		while (i > 0 && (value[i - 1] == '\n' || value[i - 1] == '\r')) i--;
		value[i] = 0;
	}

	*result_key = key;
	*result_value = value;
}


static int read_config(const char *config_file) {
	char buf[1024];
	FILE *f;
	char *host = NULL;
	char *name = NULL;
	char *user = NULL;
	char *password = NULL;
	char *key, *value;
	int retval = -1;

	if ((f = fopen(config_file, "r")) == NULL) {
		fprintf(stderr, "Can't read config file '%s': %s\n", config_file, strerror(errno));
		return -1;
	}

	while (fgets(buf, sizeof buf, f) != NULL) {
		split_key_value(buf, &key, &value);
		if ((strcmp(key, "host") == 0)) {
			free(host);
			host = value ? strdup(value) : NULL;
		} else if ((strcmp(buf, "name") == 0)) {
			free(name);
			name = value ? strdup(value) : NULL;
		} if ((strcmp(buf, "user") == 0)) {
			free(user);
			user = value ? strdup(value) : NULL;
		} if ((strcmp(buf, "password") == 0)) {
			free(password);
			password = value ? strdup(value) : NULL;
		}
	}
	if (ferror(f)) {
		fprintf(stderr, "Error reading config file '%s': %s\n", config_file, strerror(errno));
		fclose(f);
		goto err;
	}

	fclose(f);

	asprintf(&dbcs, "%s/%s@%s:%s", user ? : DEFAULT_DBUSER, password ? : DEFAULT_DBPASS, host ? : DEFAULT_DBHOST, name ? : DEFAULT_DBNAME);

	retval = 0;
err:
	free(host);
	free(name);
	free(user);
	free(password);
	return retval;
}


static void print_DBError(glite_lbu_DBContext db) {
	char *error, *desc;

	glite_lbu_DBError(db, &error, &desc);
	fprintf(stderr, "DB error: %s: %s\n", error, desc);
	free(error);
	free(desc);
}


static void save_rights(ctx_t *ctx, const char *relpath, const struct AclEntry *entry, int negative) {
	char rights[MAXRIGHTS + 2];

	if (negative) {
		rights[0] = '-';
		negative = 1;
	}
	rights2str(entry->rights, rights + negative);

	if (glite_lbu_ExecPreparedStmt(ctx->rights_stmt, 4,
		GLITE_LBU_DB_TYPE_VARCHAR, ctx->volume,
		GLITE_LBU_DB_TYPE_VARCHAR, relpath,
		GLITE_LBU_DB_TYPE_VARCHAR, entry->name,
		GLITE_LBU_DB_TYPE_VARCHAR, rights
	    ) != 1) {
		print_DBError(ctx->db);
		ctx->was_err = 1;
	}
}


static void save_point(ctx_t *ctx, const char *relpath, const char *mount) {
	if (glite_lbu_ExecPreparedStmt(ctx->points_stmt, 3,
		GLITE_LBU_DB_TYPE_VARCHAR, ctx->volume,
		GLITE_LBU_DB_TYPE_VARCHAR, relpath,
		GLITE_LBU_DB_TYPE_VARCHAR, mount
	    ) != 1) {
		print_DBError(ctx->db);
		ctx->was_err = 1;
	}
}


static int action(const char *path, int level, void *data) {
	char *mount;
	const char *relpath;
	struct Acl acl;
	size_t i;
	ctx_t *ctx = (ctx_t *)data;

	if (level >= 200) {
		fprintf(stderr, "level too high, skipping: %s\n", path);
		return BROWSE_ACTION_SKIP;
	}

	if (strncmp(path, ctx->basedir, ctx->basedir_len) == 0) relpath = path + ctx->basedir_len;
	else relpath = path;
	if (relpath[0] == '/') relpath++;

	if (list_mount(path, &mount) == 0) {
		if (level != 0) {
			printf("(%s -> %s) ", relpath, mount); fflush(stdout);
			save_point(ctx, relpath, mount);
			free(mount);
			return BROWSE_ACTION_SKIP;
		} else {
			free(mount);
		}
	}

	if (get_acl(path, &acl) != 0) {
		fprintf(stderr, "error getting ACLs for '%s': %s\n", path, strerror(errno));
		ctx->was_err = 1;
		return BROWSE_ACTION_OK;
	}
	for (i = 0; i < acl.nplus; i++) save_rights(ctx, relpath, acl.plus + i, 0);
	for (i = 0; i < acl.nminus; i++) save_rights(ctx, relpath, acl.minus + i, 1);
	free_acl(&acl);

	ctx->count++;
	if ((ctx->count % COUNT_COMMIT) == 0) {
		glite_lbu_Commit(ctx->db);
		glite_lbu_Transaction(ctx->db);
	}
	if ((ctx->count % COUNT_PROGRESS) == 0) {
		printf("%zd ", ctx->count); fflush(stdout);
	}

	return BROWSE_ACTION_OK;
}


static double timeval2double(const struct timeval *tv) {
	return tv->tv_sec + ((double)tv->tv_usec) / 1000000;
}


int main(int argc, char *argv[]) {
	int retval = EXIT_FATAL;
	int arg;

	program_name = strrchr(argv[0], '/');
	if (program_name) program_name++;
	else program_name = argv[0];

	while ((arg = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
		switch (arg) {
		case 'h':
			usage();
			return EXIT_OK;
			break;
		case 'c':
			if (read_config(optarg) != 0) return EXIT_FATAL;
			break;
		}
	}
	if (optind + 2 != argc) {
		usage();
		return EXIT_FATAL;
	}
	ctx.volume = argv[optind++];
	ctx.basedir = argv[optind++];
	ctx.basedir_len = strlen(ctx.basedir);
	if (!dbcs) dbcs = strdup(DEFAULT_DBCS);

	if (!has_afs()) {
		fprintf(stderr, "has_afs() failed\n");
		return EXIT_FATAL;
	}

	glite_common_log_init();
	if (glite_lbu_InitDBContext(&ctx.db, GLITE_LBU_DB_BACKEND_MYSQL, "DB") != 0) {
		fprintf(stderr, "DB module initialization failed\n\n");
		return EXIT_FATAL;
	}

	if (glite_lbu_DBConnect(ctx.db, dbcs) != 0) goto dberr;
	if (glite_lbu_PrepareStmt(ctx.db, "INSERT INTO mountpoints (pointvolume, pointdir, volume) VALUES (?, ?, ?)", &ctx.points_stmt) != 0) goto dberr;
	if (glite_lbu_PrepareStmt(ctx.db, "INSERT INTO rights (volume, dir, login, rights) VALUES (?, ?, ?, ?)", &ctx.rights_stmt) != 0) goto dberr;

	printf("%s: ", ctx.volume); fflush(stdout);
	gettimeofday(&ctx.begin, NULL);
	glite_lbu_Transaction(ctx.db);
	if (browse(ctx.basedir, action, &ctx) == BROWSE_ACTION_ABORT) retval = EXIT_FATAL;
	printf("%zd ", ctx.count); fflush(stdout);
	glite_lbu_Commit(ctx.db);
	gettimeofday(&ctx.end, NULL);
	printf("done, time %lf\n", timeval2double(&ctx.end) - timeval2double(&ctx.begin)); fflush(stdout);
	if (ctx.was_err) retval = EXIT_ERRORS;

	glite_lbu_FreeStmt(&ctx.points_stmt);
	glite_lbu_FreeStmt(&ctx.rights_stmt);
	glite_lbu_DBClose(ctx.db);

err:
	free(dbcs);
	glite_lbu_FreeDBContext(ctx.db);
	glite_common_log_fini();

	return retval;
dberr:
	print_DBError(ctx.db);
	goto err;
}
