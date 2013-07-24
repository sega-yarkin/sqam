#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <libgen.h>
#include <string.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sys/resource.h>
#include <signal.h>
#include <unistd.h>
#include <pwd.h>
#include <syslog.h>
#include "config.h"

/** Enable debug messages to logs. */
#define USE_DEBUG	1
/** Successfull result value. */
#define EOK		0

/** Path to .lock, in which will be written PID of the daemon. */
#define LOCKFILE	LOCALSTATEDIR "/run/fifo2mq.lock"
/** Syslog ident, if NULL used default value (basename of file). */
#define SYSLOG_NAME	NULL

/** Required memory for message queue. */
#define MQ_MEMORY	MSG_LIMIT*(MSG_SIZE+96)+10
/** FIFO file mode. */
#define FIFO_FMOD	0622


/** Print warning message. */
#define WARN(fmt, ...) \
		syslog(LOG_WARNING, "%s(): " fmt, __func__, ##__VA_ARGS__);

/** Print error message. */
#define ERR(prefix) \
		syslog(LOG_ERR, "%s(): %s: %m\n", __func__, prefix);

/** Print debug message. */
#ifndef USE_DEBUG
# define DEBUG(...)
#else
# define DEBUG(fmt, ...) \
		syslog(LOG_DEBUG, "%s(): " fmt, __func__, ##__VA_ARGS__);
#endif



char* fifo_path;	/**< Path to the input FIFO file */
char* mq_path;		/**< Message queue name */
mqd_t mq;		/**< Message queue pointer */
FILE* fp;		/**< FIFO file pointer */
int   lock;		/**< Lock file descriptor */


/**
 * Redirect standard streams.
 *
 * @param syslog_name	Name for this process into syslog
 */
void redir_io( char* syslog_name ) {
	int fd = open( "/dev/null", O_RDWR );
	if( dup(fd)<0 );
	if( dup(fd)<0 );
	openlog( syslog_name, LOG_PID | LOG_NDELAY, LOG_DAEMON );
}

/**
 * Lock file `filename'.
 *
 * @param filename	The file which will be locked
 * @returns		0 on success
 */
int lock_file( char* filename ) {
	DEBUG( "Locking file `%s'", filename );
	lock = open( filename, O_RDWR|O_CREAT, 0755 );
	if( lock == -1 ) {
		ERR( "Cannot open lock file" );
		return 1;
	}
	if( flock(lock, LOCK_EX|LOCK_NB) != EOK ) {
		WARN( "Cannot lock file, already running?" );
		return 1;
	}
	return EOK;
}

/**
 * Write PID to lock file.
 *
 * @param lock		The lock file descriptor
 */
void save_pid( int lock ) {
	char buf[20];
	snprintf( buf, sizeof buf, "%d\n", getpid() );
	if( write(lock, buf, strlen(buf)) == -1 )
		ERR( "Error occurred while writing pid" );
	DEBUG( "Child PID = %s\n", buf );
}

/**
 * Prepare resources.
 *
 * @returns		0 on success
 */
int init() {
	/*
	 *  Prepare and open Linux message queue
	 **/
	DEBUG( "MQ: message size = %d bytes\n", MSG_SIZE );
	DEBUG( "MQ: messages count limit = %d\n", MSG_LIMIT );
	DEBUG( "MQ: needed %d bytes\n", MQ_MEMORY );
	/* Get message queue memory limit */
	struct rlimit rl;
	if( getrlimit(RLIMIT_MSGQUEUE, &rl) != EOK ) {
		ERR( "Cannot get current memory limit for mqueue" );
		return 1;
	}
	DEBUG( "MQ: current limit is %d bytes, maximum is %d bytes\n", (int)rl.rlim_cur, (int)rl.rlim_max );
	if( rl.rlim_max < MQ_MEMORY ) {
		/* Set message queue memory limit */
		rl.rlim_cur = MQ_MEMORY;
		rl.rlim_max = MQ_MEMORY;
		if( setrlimit(RLIMIT_MSGQUEUE, &rl) != EOK ) {
			ERR( "Cannot set memory limit for mqueue" );
			return 1;
		}
		DEBUG( "MQ: limit has been increased successfully\n" );
	}
	/* Create message queue */
	struct mq_attr attr = { 0, MSG_LIMIT, MSG_SIZE, 0 };
	mq = mq_open( mq_path, O_WRONLY | O_CREAT, 0700, &attr );
	if( mq == -1 ) {
		ERR( "Cannot create message queue" );
		return 1;
	}
	DEBUG( "MQ: success\n" );
	
	/*
	 *  Create and open FIF file
	 */
	/* Get information about current file if it exists */
	struct stat st;
	unsigned char exists = 0;
	if( stat(fifo_path, &st) != EOK ) {
		if( errno != ENOENT ) {
			ERR( "Getting stat of file error" );
			return 1;
		}
		DEBUG( "FIFO: file does not exists\n" );
	} else {
		DEBUG( "FIFO: file exists\n" );
		exists = 1;
		if( ! S_ISFIFO(st.st_mode) ) {
			WARN( "File %s exists and it isn't a FIFO\n", fifo_path );
			DEBUG( "FIFO: removing file %s\n", fifo_path );
			if( unlink(fifo_path) != EOK ) {
				ERR( "Cannot remove existing file" );
				return 1;
			}
			exists = 0;
		}
	}
	/* Create FIFO file if needed */
	if( ! exists ) {
		DEBUG( "FIFO: creating FIFO file\n" );
		if( mkfifo(fifo_path, 0600) < 0 ) {
			if( errno != EEXIST ) {
				ERR( "Cannot create FIFO file" );
				return 1;
			}
		}
		if( chmod(fifo_path, FIFO_FMOD) < 0 ) {
			ERR( "Cannot chmod FIFO file" );
			return 1;
		}
	} else {
		DEBUG( "FIFO: current file mode = %lo\n", (unsigned long)st.st_mode );
		if( (st.st_mode & 0777) != FIFO_FMOD ) {
			DEBUG( "FIFO: updating file mod\n" );
			if( chmod(fifo_path, FIFO_FMOD) < 0 ) {
				ERR( "Cannot chmod FIFO file" );
				return 1;
			}
		}
	}
	/* Open FIFO */
	fp = fopen( fifo_path, "w+" );
	DEBUG( "FIFO: file pointer = 0x%lX\n", (unsigned long)fp );
	if( fp == NULL ) {
		ERR( "Cannot open FIFO file for R/W" );
		return 1;
	}
	DEBUG( "FIFO: success\n" );
	return EOK;
}

/**
 * Normaly terminate by signal.
 */
void terminate() {
	DEBUG( "Terminating\n" );
	mq_close( mq );
	fclose( fp );
	exit( EXIT_SUCCESS );
}

/**
 * Daemonize process.
 *
 * @returns		0 on success
 */
int daemonize() {
	/* already a daemon */
	if( getppid() == 1 ) {
		DEBUG( "Proccess already a daemon\n" );
		return EOK;
	}
	void child_success( int signum ) {
		DEBUG( "Parent catch signal %d\n", signum );
		switch( signum ) {
			case SIGCHLD:
			case SIGALRM:
				exit( EXIT_FAILURE );
				break;
			case SIGUSR1:
				exit( EXIT_SUCCESS );
				break;
		}
	}
	
	/* drop user if there is one, and we were run as root */
	if( getuid()==0 || geteuid()==0 ) {
		DEBUG( "Drop root privileges\n" );
		struct passwd *pw = getpwnam( USER );
		if( pw ) {
			if( setuid(pw->pw_uid)!=EOK )//|| setgid(pw->pw_gid)!=EOK )
				ERR( "Unable to drop root" );
		}
	}
	
	/* trap signals that we expect to recieve */
	signal( SIGCHLD, child_success );
	signal( SIGUSR1, child_success );
	signal( SIGALRM, child_success );
	
	DEBUG( "Forking\n" );
	pid_t pid = fork();
	if( pid < 0 ) {
		ERR( "Error occurred while forking" );
		return 1;
	} else if( pid > 0 ) {
		/* wait for confirmation from the child via SIGTERM or SIGCHLD, or
		   for two seconds to elapse (SIGALRM).  pause() should not return. */
		alarm( 2 );
		pause();
		exit( EXIT_FAILURE );
	}
	save_pid( lock );
	
	pid_t parent = getppid();
	DEBUG( "Set signals\n" );
	/* cancel certain signals */
	signal( SIGCHLD, SIG_DFL );
	signal( SIGTSTP, SIG_IGN ); /* various TTY signals */
	signal( SIGTTOU, SIG_IGN );
	signal( SIGTTIN, SIG_IGN );
	signal( SIGHUP , SIG_IGN ); /* ignore hangup signal */
	signal( SIGTERM, terminate );
	/* change the file mode mask */
	umask( 0027 );
	/* create a new session */
	DEBUG( "Create a new session\n" );
	if( setsid() == -1 ) {
		ERR( "Unable to create a new session" );
		return 1;
	}
	/* change the current working directory */
	if( chdir("/") == -1 ) {
		ERR( "Unable to change working directory" );
		return 1;
	}
	/* tell the parent process that we are A-okay */
	DEBUG( "Send signal to parent\n" );
	kill( parent, SIGUSR1 );
	return EOK;
}

/**
 * Pumping messages from FIFO to message queue.
 */
void pump() {
	char buf[ MSG_SIZE+1 ];		/**< Message buffer */
	int len;			/**< Message length */
	memset( buf, 0x00, MSG_SIZE );
	
	/* pump loop */
	DEBUG( "Start pumping\n" );
	while( fgets(buf, sizeof buf, fp) ) {
		len = strlen( buf );
		DEBUG( "Received new log, length = %d\n", len );
		if( mq_send(mq, buf, len, 0) != EOK )
			ERR( "Error while mq_send" );
	}
}

/**
 * Main function.
 */
int main( int argc, char *argv[] ) {
	/* need one or two arguments */
	if( argc < 2 ) {
		char *bn;
		bn = strdup( argv[0] );
		bn = basename( bn );
		fprintf( stdout, "Usage:   %s  fifo  [mq_name]\n", bn );
		fprintf( stdout, "         fifo       Path to fifo for read incoming data\n" );
		fprintf( stdout, "         mq_name    Message queue path-name\n" );
		exit( EXIT_FAILURE );
	}
	redir_io( SYSLOG_NAME );
	fifo_path = strdup( argv[1] );
	if( argc > 2 )
		mq_path = strdup( argv[2] );
	else
		mq_path = MQ_PATH;
	
	DEBUG( "Fifo file = %s\n", fifo_path );
	DEBUG( "  MQ name = %s\n", mq_path   );
	
	/* initialize */
	if( lock_file(LOCKFILE) != EOK )
		exit( EXIT_FAILURE );
	if( init()!=EOK || daemonize()!=EOK )
		exit( EXIT_FAILURE );
	
	/* infinite loop */
	pump();
	
	/* close all if loop aborted */
	mq_close( mq );
	fclose( fp );
	exit( EXIT_FAILURE );
}
