#include "utils.h"

void redir_io( int close_stdin, char* syslog_name ) {
	int fd = open( DEVNULL, O_RDWR );
	if( close_stdin && dup2(fd,STDIN_FILENO )<0 );
	if(                dup2(fd,STDOUT_FILENO)<0 );
	openlog( syslog_name, LOG_PID | LOG_NDELAY, LOG_DAEMON );
}

int lock_file( char* filename, int* lock ) {
	DEBUG( "Locking file `%s'", filename );
	*lock = open( filename, O_RDWR|O_CREAT, 0755 );
	if( *lock == -1 ) {
		ERR( "Cannot open lock file" );
		return 1;
	}
	if( flock(*lock, LOCK_EX|LOCK_NB) != EOK ) {
		WARN( "Cannot lock file, already running?" );
		return 1;
	}
	return EOK;
}

void save_pid( int lock ) {
	char buf[20];
	snprintf( buf, sizeof buf, "%d\n", getpid() );
	if( write(lock, buf, strlen(buf)) == -1 )
		ERR( "Error occurred while writing pid" );
	DEBUG( "Child PID = %s", buf );
}

int daemonize() {
	/* already a daemon */
	if( getppid() == 1 ) {
		DEBUG( "Proccess already a daemon" );
		return EOK;
	}
	void child_success( int signum ) {
		DEBUG( "Parent catch signal %d", signum );
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
		DEBUG( "Drop root privileges" );
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
	
	DEBUG( "Forking" );
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
	
	pid_t parent = getppid();
	DEBUG( "Set signals" );
	/* cancel certain signals */
	signal( SIGCHLD, SIG_DFL );
	signal( SIGTSTP, SIG_IGN ); /* various TTY signals */
	signal( SIGTTOU, SIG_IGN );
	signal( SIGTTIN, SIG_IGN );
	signal( SIGHUP , SIG_IGN ); /* ignore hangup signal */
	/* change the file mode mask */
	umask( 0027 );
	/* create a new session */
	DEBUG( "Create a new session" );
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
	DEBUG( "Send signal to parent" );
	kill( parent, SIGUSR1 );
	return EOK;
}
