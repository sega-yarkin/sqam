#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <pwd.h>
#include <signal.h>

#include "config.h"

/** Successfull result value. */
#define EOK		0

/** Required memory for message queue. */
#define MQ_MEMORY	MSG_LIMIT*(MSG_SIZE+96)+10


/**
 * Print warning message.
 *
 * @param fmt		Format string
 */
#define WARN(fmt, ...) syslog(LOG_WARNING, "%s(): " fmt, __func__, ##__VA_ARGS__);

/**
 * Print error message.
 *
 * @param prefix	Text before error string
 */
#define ERR(prefix) syslog(LOG_ERR, "%s(): %s: %m\n", __func__, prefix);

#ifndef USE_DEBUG
# define DEBUG(...)
#else
/**
 * Print debug message.
 *
 * @param fmt		Format string
 */
# define DEBUG(fmt, ...) syslog(LOG_DEBUG, "%s(): " fmt, __func__, ##__VA_ARGS__);
#endif


/**
 * Redirect standard streams.
 *
 * @param close_stdin	0 if no need to close stdin
 * @param syslog_name	Name for this process into syslog
 */
void redir_io( int close_stdin, char* syslog_name );

/**
 * Lock file `filename' or just create it.
 *
 * @param filename	The file which will be locked
 * @param lock		Pointer to FD
 * @param is_pid	If great than 0 file will not lock
 * @returns		0 on success
 */
int lock_file( char* filename, int* lock, int is_pid );

/**
 * Write PID to lock file.
 *
 * @param lock		The lock file descriptor
 */
void save_pid( int lock );

/**
 * Daemonize process.
 *
 * @returns		0 on success
 */
int daemonize();


#endif /* _UTILS_H_ */