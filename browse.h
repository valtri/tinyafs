#ifndef BROWSE_H
#define BROWSE_H

#define BROWSE_ACTION_OK 0
#define BROWSE_ACTION_SKIP 1
#define BROWSE_ACTION_ABORT 2

/**
 * Callback function for directories.
 * \ret BROWSE_ACTION_OK use directory, BROWSE_ACTION_SKIP don't browse this directory, BROWSE_ACTION_ABORT abort the whole process
 */
typedef int (browse_action_f)(const char *path, int level);

/**
 * Browse directory tree.
 *
 * path: starting path
 * action_cn: callback per each directory
 *
 * \ret 0 OK, 1 some errors, 2 aborted by callback, 3 errors and aborted by callback
 */
int browse(const char *path, browse_action_f *action_cb);

#endif
