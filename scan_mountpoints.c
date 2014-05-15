#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "browse.h"
#include "tinyafs.h"


const char *optstring = "hq";

const struct option longopts[] = {
	{ "help", no_argument, NULL, 'h' },
	{ "quiet", no_argument, NULL, 'q' },
};

int quiet = 0;
int was_err = 0;
int was_mountpoint = 0;


int action(const char *path, int level, void *user __attribute__((unused))) {
	int retval;
	char *volume = NULL;

	printf("%d: %s\n", level, path);

	if (level > 0) {
		retval = list_mount(path, quiet ? NULL : &volume);
		if (retval == 0) {
			if (!quiet) printf("%s -> %s\n", path, volume);
			free(volume);
			if (level > 0) {
				was_mountpoint = 1;
				return BROWSE_ACTION_SKIP;
			} else {
				return BROWSE_ACTION_OK;
			}
		}
		if (errno != EINVAL) {
			/* set our errors flag, by try to continue */
			fprintf(stderr, "%s\n", strerror(errno));
			was_err = 1;
		}
	}

	return BROWSE_ACTION_OK;
}


static void usage(const char *prog) {
	printf("Usage: %s DIRECTORY\n", prog);
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
			case 'q':
				quiet = 1;
				break;
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
