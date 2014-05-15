#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "array.h"
#include "browse.h"


#define log_errno(MSG, ARGS...) log_errno_raw(__LINE__, ((MSG)), ##ARGS)


typedef struct {
	char *path;
	DIR *dir;
} browse_node_t;


static void log_errno_raw(int line, const char *msg, ...) {
	va_list ap;
	const char *err;

	err = strerror(errno);
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);

	fprintf(stderr, ": %s (%s:%d)\n", err, __FILE__, line);
}


static void browse_node_free(browse_node_t *node) {
	if (node)
		free(node->path);
}


static void path_concat(char **path, const char *parent, const char *name) {
	size_t len_parent, len_name;
	char *s;

	len_parent = strlen(parent);
	len_name = strlen(name);

	*path = s = malloc(len_parent + len_name + 2);
	memcpy(s, parent, len_parent); s += len_parent;
	s[0] = '/'; s++;
	memcpy(s, name, len_name); s += len_name;
	s[0] = 0;
}


int browse(const char *path, browse_action_f *action_cb, void *data) {
	array_t stack;
	browse_node_t *node;
	int level = 0;
	struct dirent *entry, *entry_r;
	int retval, err = 0;
	char *subpath;

	switch (retval = action_cb(path, level, data)) {
	case BROWSE_ACTION_OK: break;
	case BROWSE_ACTION_SKIP: return 0;
	default: return 2;
	}

	/* XXX: POSIX doesn't specify d_name field length, add some padding */
	entry = malloc(sizeof(struct dirent) + (4096 > NAME_MAX ? 4096 : NAME_MAX));
	array_init(&stack, sizeof(browse_node_t));

	node = array_push(&stack);
	node->path = strdup(path);
	if ((node->dir = opendir(path)) == NULL) {
		log_errno("can't open directory '%s'", path);
		err = 1;
		browse_node_free(node);
		return 1;
	}
	level++;

	/* continue with next node */
	while (node) {
		/* read the directory and go down */
		while (retval != BROWSE_ACTION_ABORT) {
			if (readdir_r(node->dir, entry, &entry_r) != 0) {
				log_errno("error reading directory '%s'", path);
				err = 1;
				entry_r = NULL;
			}
			if (!entry_r) readdir_r(node->dir, entry, &entry_r);
			if (!entry_r) break;

			path_concat(&subpath, node->path, entry->d_name);

			if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") && (retval = action_cb(subpath, level, data)) == BROWSE_ACTION_OK) {
			/* yes, continue to go down */
				level++;

				node = array_push(&stack);
				node->path = subpath;
				if ((node->dir = opendir(node->path)) == NULL) {
					log_errno("can't open directory '%s'", node->path);
					err = 1;
					break;
				}
			} else {
			/* no, stay and read the next directory item */
				free(subpath);
			}
		}

		/* node processed, go up */
		if (node->dir) closedir(node->dir);
		browse_node_free(node);
		node = (browse_node_t *)array_pop(&stack);
		level--;
	}

	free(entry);
	array_destroy(&stack);

	if (retval == BROWSE_ACTION_ABORT) err |= 2;
	return err;
}
