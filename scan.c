#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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

#define DEFAULT_MAX_DEPTH 200

#define EXIT_OK 0
#define EXIT_ERRORS 1
#define EXIT_FATAL 2

#define COUNT_COMMIT 5000
#define COUNT_PROGRESS 100000



typedef struct {
	int id;
	pthread_t thread;

	char *basedir; /* start directory */
	char *volume;  /* name of volume */
	char *mountpoint; /* mountpoint if mounted by scanner */

	size_t basedir_len;
	glite_lbu_DBContext db;
	glite_lbu_Statement points_stmt, rights_stmt;

	int was_err;
	size_t count;
	struct timeval begin, end;
} ctx_t;


typedef struct {
	char *volume;
	char *mountpoint;
} volume_t;


pthread_mutex_t lock;
char *program_name = NULL;
char *dbcs = NULL;
char *servicedir = NULL;
size_t nthreads = 1;
int max_depth = DEFAULT_MAX_DEPTH;
char *cell = NULL;

volume_t *volume_list = NULL;
size_t nvolumes = 0;
size_t maxnvolumes = 0;
size_t ivolume = 0;
sig_atomic_t quit = 0;

ctx_t *threads = NULL;

const char *optstring = "a:hc:l:m:n:s:";

const struct option longopts[] = {
	{ "cell", required_argument, NULL, 'a' },
	{ "help", no_argument, NULL, 'h' },
	{ "config", required_argument, NULL, 'c' },
	{ "list", required_argument, NULL, 'l' },
	{ "max-depth", required_argument, NULL, 'm' },
	{ "num-threads", required_argument, NULL, 'n' },
	{ "servicedir", required_argument, NULL, 's' },
	{ NULL, 0, NULL, 0 }
};



static void usage() {
	printf("Usage:\n\
  %s [OPTIONS] -l, --list FILE | VOLUME [DIRECTORY]\n\
\n\
OPTIONS are:\n\
  -a, --cell=CELL ........ default AFS cell name\n\
  -h, --help ............. help message\n\
  -c, --config=FILE ...... config file\n\
  -m, --max-depth=DEPTH .. maximal directory level to go into [%d]\n\
  -n, --num-threads=N .... number of parallel threads [%d]\n\
  -s, --servicedir=DIR ... empty helper directory for mounting\n\
", program_name, max_depth, nthreads);
}


static void handle_signal(int sig __attribute__((unused))) {
	quit = 1;
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
	struct stat statinfo;
	char buf[1024];
	FILE *f;
	char *host = NULL;
	char *name = NULL;
	char *user = NULL;
	char *password = NULL;
	char *key, *value;
	int retval = -1;

	if (stat(config_file, &statinfo) != 0) {
		fprintf(stderr, "Can't stat config file '%s': %s\n", config_file, strerror(errno));
		return -1;
	}
	if ((statinfo.st_mode & 0177) != 0) {
		fprintf(stderr, "Warning: rights for the config file '%s' are too permissive!\n", config_file);
	}

	if ((f = fopen(config_file, "r")) == NULL) {
		fprintf(stderr, "Can't read config file '%s': %s\n", config_file, strerror(errno));
		return -1;
	}

	while (fgets(buf, sizeof buf, f) != NULL) {
		if (buf[0] == '#') continue;
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
		} if ((strcmp(buf, "cell") == 0)) {
			free(cell);
			cell = value ? strdup(value) : NULL;
		} if ((strcmp(buf, "servicedir") == 0)) {
			free(servicedir);
			servicedir = value ? strdup(value) : NULL;
		} if ((strcmp(buf, "threads") == 0)) {
			nthreads = atoi(value);
			if (nthreads == 0 || nthreads > 1000) {
				fprintf(stderr, "Strange number of threads (%zd)\n", nthreads);
				fclose(f);
				goto err;
			}
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


static void add_volume(const char *volume, const char *mountpoint) {
	void *tmp;

	if (nvolumes >= maxnvolumes) {
		if (maxnvolumes) maxnvolumes *= 2;
		else maxnvolumes = 512;
		tmp = realloc(volume_list, maxnvolumes * sizeof(volume_t));
		volume_list = tmp;
	}

	volume_list[nvolumes].volume = strdup(volume);
	volume_list[nvolumes].mountpoint = mountpoint ? strdup(mountpoint) : NULL;
	nvolumes++;
}


static int read_list(const char *list_file) {
	char buf[1024];
	FILE *f;
	size_t i;
	char *volume, *mountpoint;

	if ((f = fopen(list_file, "r")) == NULL) {
		fprintf(stderr, "Can't read volume list file '%s': %s\n", list_file, strerror(errno));
		return -1;
	}

	while (fgets(buf, sizeof buf, f) != NULL) {
		if (buf[0] == '#') continue;

		i = strlen(buf);
		while (i > 0 && (buf[i - 1] == '\n' || buf[i - 1] == '\r')) i--;
		buf[i] = 0;

		volume = buf;
		mountpoint = buf + strcspn(buf, " \t");
		if (mountpoint[0]) {
			mountpoint[0] = 0;
			mountpoint++;
		} else {
			mountpoint = NULL;
		}

		add_volume(volume, mountpoint);
	}
	if (ferror(f)) {
		fprintf(stderr, "Error reading volume list file '%s': %s\n", list_file, strerror(errno));
		fclose(f);
		return -1;
	}

	return 0;
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
	const char *volname;
	size_t len;

	volname = mount + 1;
	if (cell) {
		len = strlen(cell);
		if (strncasecmp(volname, cell, len) == 0 && volname[len] == ':')  volname += len + 1;
	}
	if (glite_lbu_ExecPreparedStmt(ctx->points_stmt, 3,
		GLITE_LBU_DB_TYPE_VARCHAR, ctx->volume,
		GLITE_LBU_DB_TYPE_VARCHAR, relpath,
		GLITE_LBU_DB_TYPE_VARCHAR, volname
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

	if (strncmp(path, ctx->basedir, ctx->basedir_len) == 0) relpath = path + ctx->basedir_len;
	else relpath = path;
	if (relpath[0] == '/') relpath++;

	if (max_depth > 0 && level >= max_depth) {
		fprintf(stderr, "level too high, skipping: %s:%s\n", ctx->volume, relpath);
		return BROWSE_ACTION_SKIP;
	}

	if (list_mount(path, &mount) == 0) {
		if (level != 0) {
			printf("[thread %d] %s: (%s -> %s)\n", ctx->id, ctx->volume, relpath, mount);
			save_point(ctx, relpath, mount);
			free(mount);
			return BROWSE_ACTION_SKIP;
		} else {
			free(mount);
		}
	}

	if (get_acl(path, &acl) != 0) {
		fprintf(stderr, "error getting ACLs for %s:%s: %s\n", ctx->volume, relpath, strerror(errno));
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
		printf("[thread %d] %s: %zd dirs so far\n", ctx->id, ctx->volume, ctx->count);
	}

	return BROWSE_ACTION_OK;
}


static double timeval2double(const struct timeval *tv) {
	return tv->tv_sec + ((double)tv->tv_usec) / 1000000;
}


/**
 * Get next volume to process.
 */
static int get_volume(ctx_t *ctx) {
	int retval = 0;
	char *original_mountpoint;

	pthread_mutex_lock(&lock);
	if (ivolume < nvolumes) {
		ctx->volume = volume_list[ivolume].volume;
		original_mountpoint = volume_list[ivolume].mountpoint;
		ivolume++;
	} else {
		ctx->volume = NULL;
		original_mountpoint = NULL;
	}
	pthread_mutex_unlock(&lock);

	if (ctx->volume) {
		if (original_mountpoint) {
		/* use existing mountpoint */
			ctx->basedir = original_mountpoint;
			ctx->mountpoint = NULL;
		} else {
		/* mount into helper directory */
			asprintf(&ctx->mountpoint, "%s/%d-%s", servicedir, ctx->id, ctx->volume);
			if ((retval = make_mount(ctx->mountpoint, ctx->volume, 0, NULL)) != 0) {
				fprintf(stderr, "%d: can't mount '%s' to '%s': %s\n", ctx->id, ctx->volume, ctx->mountpoint, strerror(errno));
				ctx->was_err = 1;
				free(ctx->mountpoint);
				ctx->mountpoint = NULL;
				return retval;
			}
			ctx->basedir = ctx->mountpoint;
		}
		ctx->basedir_len = strlen(ctx->basedir);
	} else {
		ctx->basedir = NULL;
	}

	return 0;
}


/**
 * Release the processed volume.
 */
static void return_volume(ctx_t *ctx) {
	if (ctx->mountpoint) {
		if (remove_mount(ctx->mountpoint) != 0) {
			fprintf(stderr, "%d: can't unmount '%s' from '%s': %s\n", ctx->id, ctx->volume, ctx->mountpoint, strerror(errno));
		}
		free(ctx->mountpoint);
		ctx->mountpoint = NULL;
	}
}


void *browser_thread(void *data) {
	ctx_t *ctx = (ctx_t *)data;
	int connected = 0;
	double duration, ratio;

	printf("[thread %d] started\n", ctx->id);

	if (glite_lbu_InitDBContext(&ctx->db, GLITE_LBU_DB_BACKEND_MYSQL, "DB") != 0) {
		fprintf(stderr, "DB module initialization failed\n\n");
		return NULL;
	}

	if (glite_lbu_DBConnect(ctx->db, dbcs) != 0) goto dberr;
	connected = 1;
	if (glite_lbu_PrepareStmt(ctx->db, "INSERT INTO mountpoints (pointvolume, pointdir, volume) VALUES (?, ?, ?)", &ctx->points_stmt) != 0) goto dberr;
	if (glite_lbu_PrepareStmt(ctx->db, "INSERT INTO rights (volume, dir, login, rights) VALUES (?, ?, ?, ?)", &ctx->rights_stmt) != 0) goto dberr;

	do {
		ctx->count = 0;

		while (get_volume(ctx) != 0 && ctx->volume) { };
		if (!ctx->volume) break;

		gettimeofday(&ctx->begin, NULL);
		glite_lbu_Transaction(ctx->db);

		browse(ctx->basedir, action, ctx);

		glite_lbu_Commit(ctx->db);

		gettimeofday(&ctx->end, NULL);
		duration = timeval2double(&ctx->end) - timeval2double(&ctx->begin);
		ratio = (duration > 0.001) ? ctx->count / duration : 0;
		printf("[thread %d] %s: %zd dirs, time %lf, ratio %lf dirs/s\n", ctx->id, ctx->volume, ctx->count, duration, ratio);

		return_volume(ctx);
	} while (ctx->volume && !quit);

	glite_lbu_FreeStmt(&ctx->points_stmt);
	glite_lbu_FreeStmt(&ctx->rights_stmt);
	glite_lbu_DBClose(ctx->db);
	glite_lbu_FreeDBContext(ctx->db);

	printf("[thread] %d finished\n", ctx->id);

	return NULL;
dberr:
	print_DBError(ctx->db);
	if (connected) glite_lbu_DBClose(ctx->db);
	glite_lbu_FreeDBContext(ctx->db);
	return NULL;
}


int main(int argc, char *argv[]) {
	struct sigaction sa, osa;
	int retval = EXIT_FATAL;
	int arg;
	size_t i;
	struct timeval begin, end;

	program_name = strrchr(argv[0], '/');
	if (program_name) program_name++;
	else program_name = argv[0];

	if (pthread_mutex_init(&lock, NULL) != 0) {
		fprintf(stderr, "Error initializing pthread mutex\n");
		return EXIT_FATAL;
	}

	while ((arg = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
		switch (arg) {
		case 'h':
			usage();
			return EXIT_OK;
			break;
		case 'a':
			free(cell);
			cell = strdup(optarg);
			break;
		case 'c':
			if (read_config(optarg) != 0) goto err;
			break;
		case 's':
			free(servicedir);
			servicedir = strdup(optarg);
			break;
		case 'l':
			if (read_list(optarg) != 0) goto err;
			break;
		case 'm':
			max_depth = atoi(optarg);
			break;
		case 'n':
			nthreads = atoi(optarg);
			if (nthreads == 0 || nthreads > 1000) {
				fprintf(stderr, "Strange number of threads (%zd)\n", nthreads);
				goto err;
			}
			break;
		}
	}
	if (optind < argc) {
		char *volume, *point;

		volume = argv[optind++];
		if (optind < argc) point = argv[optind++];
		else point = NULL;
		add_volume(volume, point);
	}
	if (!volume_list) {
		fprintf(stderr, "No volumes specified.\n\n");
		usage();
		goto err;
	}
	if (!servicedir) {
		for (ivolume = 0; ivolume < nvolumes; ivolume++) {
			if (!volume_list[ivolume].mountpoint) {
				fprintf(stderr, "Service directory is needed or all mountpoints needs to be specified (volume '%s').\n\n", volume_list[ivolume].volume);
				usage();
				goto err;
			}
		}
	}
	if (!dbcs) dbcs = strdup(DEFAULT_DBCS);

	if (!has_afs()) {
		fprintf(stderr, "has_afs() failed\n");
		goto err;
	}

	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = handle_signal;
	if (sigaction(SIGINT, &sa, &osa) != 0) {
		fprintf(stderr, "Warning: installing signal handler failed: %s\n", strerror(errno));
	}
	gettimeofday(&begin, NULL);
	glite_common_log_init();
	ivolume = 0;
	threads = calloc(nthreads, sizeof(ctx_t));
	for (i = 0; i < nthreads; i++) {
		threads[i].id = i;
		if (pthread_create(&threads[i].thread, NULL, browser_thread, threads + i) != 0) {
			fprintf(stderr, "Error creating %d. thread: %s\n", i, strerror(errno));
			goto err;
		}
	}
	retval = EXIT_OK;
	for (i = 0; i < nthreads; i++) {
		if (pthread_join(threads[i].thread, NULL) != 0) {
			fprintf(stderr, "Error can't join %d. thread: %s\n", i, strerror(errno));
			retval = EXIT_FATAL;
		}
		if (threads[i].was_err && retval != EXIT_FATAL) retval = EXIT_ERRORS;
	}
	gettimeofday(&end, NULL);
	if (ivolume > 0) printf("[main] last volume: %s\n", volume_list[ivolume - 1].volume);
	printf("[main] run duration: %lf\n", timeval2double(&end) - timeval2double(&begin));

err:
	free(dbcs);
	free(servicedir);
	free(cell);
	if (volume_list) {
		for (ivolume = 0; ivolume < nvolumes; ivolume++) {
			free(volume_list[ivolume].volume);
			free(volume_list[ivolume].mountpoint);
		}
		free(volume_list);
	}
	glite_common_log_fini();
	pthread_mutex_destroy(&lock);

	return retval;
}
