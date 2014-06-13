#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "browse.h"
#include "tinyafs.h"


const char *optstring = "hu:n:";

const struct option longopts[] = {
	{ "help", no_argument, NULL, 'h' },
	{ "user", required_argument, NULL, 'u' },
	{ "new-user", required_argument, NULL, 'n' },
};

int was_err = 0;
int was_mountpoint = 0;

const char *login = NULL;
const char *new_login = NULL;


int action(const char *path, int level, void *user __attribute__((unused))) {
	struct Acl acl;
	char rights_str[MAXRIGHTS];
	int retval;
	size_t i, j;
	char *epath;

	if (level > 0) {
		retval = list_mount(path, NULL);
		if (retval == 0) {
			was_mountpoint = 1;
			return BROWSE_ACTION_SKIP;
		}
		if (errno != EINVAL) {
			/* set our errors flag, but try to continue */
			fprintf(stderr, "lsmount '%s': %s\n", path, strerror(errno));
			was_err = 1;
		}
	}

	errno = 0;
	retval = get_acl(path, &acl);
	if (retval != 0 || errno) {
		/* set our errors flag, but try to continue */
		fprintf(stderr, "la '%s': %s\n", path, strerror(errno));
		was_err = 1;
		return BROWSE_ACTION_OK;
	}

	epath = malloc(2 * strlen(path) + 1);
	for (i = 0, j = 0; path[i]; i++) {
		if (path[i] == '\"' || path[i] == '\\') {
			epath[j++] = '\\';
		}
		epath[j++] = path[i];
	}
	epath[j] = '\0';

	for (i = 0; i < acl.nplus; i++) {
		if (strcmp(login, acl.plus[i].name) == 0) {
			rights2str(acl.plus[i].rights, rights_str);
			printf("fs sa \"%s\" %s %s\n", epath, new_login ? : login, rights_str);
		}
	}
	for (i = 0; i < acl.nminus; i++) {
		if (strcmp(login, acl.minus[i].name) == 0) {
			rights2str(acl.minus[i].rights, rights_str);
			printf("fs sa \"%s\" %s -%s\n", epath, new_login ? : login, rights_str);
		}
	}

	free_acl(&acl);
	free(epath);

	return BROWSE_ACTION_OK;
}


static void usage(const char *prog) {
	printf("Usage: %s [OPTIONS] DIRECTORY\n\
\n\
OPTIONS are:\n\
  -h, --help ....... usage\n\
  -u, --user ....... userame\n\
  -n, --new-user ... new username\n\
", prog);
}


int main(int argc, char *argv[]) {
	char *prog;
	int arg;

	prog = strrchr(argv[0], '/');
	if (prog) prog++;
	else prog = argv[0];

	while ((arg = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
		switch (arg) {
			case 'h':
				usage(prog);
				return 0;
			case 'u':
				login = optarg;
				break;
			case 'n':
				new_login = optarg;
				break;
			default:
				usage(prog);
				return 1;
		}
	}
	if (optind >= argc) {
		usage(prog);
		return 0;
	}

	while (optind < argc) {
		if (browse(argv[optind++], action, NULL) != 0) was_err = 1;
	}

	if (was_err) return 2;
	if (was_mountpoint) return 1;
	return 0;
}
