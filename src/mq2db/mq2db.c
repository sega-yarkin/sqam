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
#include <sys/time.h>
#include <unistd.h>
#include <pwd.h>
#include <syslog.h>
#include "mongo.h"

#include "config.h"


#define USE_DEBUG	1
#define EOK		0
#define INIT_DELAY_ON_ERROR	10

#define LOCKFILE	LOCALSTATEDIR "/run/mq2db.lock"
#define SYSLOG_NAME	NULL


/* Print warning message */
#define WARN(fmt, ...) \
		syslog(LOG_WARNING, "%s(): " fmt, __func__, ##__VA_ARGS__);

/* Print error */
#define ERR(prefix) \
		syslog(LOG_ERR, "%s(): %s: %m\n", __func__, prefix);

/* Debug function */
#ifndef USE_DEBUG
# define DEBUG(...)
#else
# define DEBUG(fmt, ...) \
		syslog(LOG_DEBUG, "%s(): " fmt, __func__, ##__VA_ARGS__);
#endif


char  *mq_path;
mqd_t  mq;
int    lock;

char  *db_name, *db_host, db_ns[100];
int    db_port;
mongo  conn[1];
unsigned char opened = 0;


/**
 * Redirect standard streams.
 */
void redir_io( char* syslog_name ) {
	int fd = open( "/dev/null", O_RDWR );
	if( dup(fd)<0 );
	if( dup(fd)<0 );
	openlog( syslog_name, LOG_PID | LOG_NDELAY, LOG_DAEMON );
}

/**
 * Lock file `filename'.
 */
void lock_file( char* filename ) {
	DEBUG( "Locking file `%s'", filename );
	lock = open( filename, O_RDWR|O_CREAT, 0755 );
	if( lock == -1 ) {
		ERR( "Cannot open lock file" );
		exit( EXIT_FAILURE );
	}
	if( flock(lock, LOCK_EX|LOCK_NB) != EOK ) {
		WARN( "Cannot lock file, already running?" );
		exit( EXIT_FAILURE );
	}
}

/**
 * Write PID to lock file.
 */
void save_pid( int lock ) {
	char buf[20];
	snprintf( buf, sizeof buf, "%d\n", getpid() );
	if( write(lock, buf, strlen(buf)) == -1 )
		ERR( "Error occurred while writing pid" );
	DEBUG( "Child PID = %s\n", buf );
}


/**
 * Initialize message queue.
 */
int init_mq( char* path ) {
	mq = mq_open( path, O_RDONLY );
	if( mq == -1 ) {
		perror( "Cannot open mqueue for reading" );
		return 1;
	}
	return EOK;
}

/**
 * Connect to MongoDB.
 */
int init_mongo( mongo* conn, char* host, int port, char* db, char* ns, int ns_size ) {
	mongo_init( conn );
	mongo_set_op_timeout( conn, 1000 );
	if( mongo_client(conn, host, port) != MONGO_OK ) {
		fprintf( stderr, "Cannot connect to MongoDB: " );
		switch( conn->err ) {
			case MONGO_CONN_NO_SOCKET:  fprintf( stderr, "no socket\n"            ); break;
			case MONGO_CONN_FAIL:       fprintf( stderr, "connection failed\n"    ); break;
			case MONGO_CONN_NOT_MASTER: fprintf( stderr, "not master\n"           ); break;
			case MONGO_CONN_ADDR_FAIL:  fprintf( stderr, "cannot resolve address" ); break;
			default: break;
		}
		return 1;
	}
	if( snprintf(ns, ns_size, "%s.%s", db, DB_RAW_LOG_COLL) < 3 ) {
		fprintf( stderr, "Cannot make ns string" );
		return 1;
	}
	return EOK;
}

int init_sequence() {
	if( init_mq(mq_path) != EOK )
		return 1;
	if( init_mongo(conn, db_host, db_port, db_name, &db_ns[0], sizeof db_ns) != EOK )
		return 1;
	return EOK;
}

int init() {
	while( init_sequence() != EOK ) {
		sleep( INIT_DELAY_ON_ERROR );
	}
	opened = 1;
	return EOK;
}


int close_all() {
	mq_close( mq );
	mongo_destroy( conn );
	opened = 0;
	return EOK;
}


void reopen() {
	if( ! opened )
		return;
	close_all();
	init();
}


void terminate() {
	close_all();
	exit( 0 );
}


void pump() {
	ssize_t rcvd;
	char    buf[ MSG_SIZE+10 ];
	bson    doc[1];
	int64_t usec;
	struct timeval tv;
	
	while( 1 ) {
		// Receive message from mqueue
		rcvd = mq_receive( mq, (char*)&buf, sizeof buf, NULL );
		if( rcvd < 0 ) {
			perror( "Receive message error" );
			continue;
		}
		buf[ rcvd ] = '\0';
		
		// Send to MongoDB
		bson_init( doc );
		gettimeofday( &tv, NULL );
		usec = (int64_t) (tv.tv_sec) * 1000000 + tv.tv_usec;
		bson_append_long  ( doc, "time", usec );
		bson_append_string( doc, "data", (char*)&buf );
		bson_finish( doc );
		while( mongo_insert(conn, db_ns, doc, NULL) != MONGO_OK ) {
			unsigned char br=0;
			switch( conn->err ) {
				case MONGO_IO_ERROR:
				case MONGO_SOCKET_ERROR:
					reopen();
					break;
				case MONGO_WRITE_ERROR:
					fprintf( stderr, "MongoDB error: Write with given write_concern returned an error\n" );
					br = 1;
					break;
				case MONGO_NS_INVALID:
					fprintf( stderr, "MongoDB error: The name for the ns (%s) is invalid.\n", db_ns );
					exit( 1 );
					break;
				case MONGO_BSON_INVALID:
				case MONGO_BSON_NOT_FINISHED:
				case MONGO_BSON_TOO_LARGE:
					fprintf( stderr, "MongoDB BSON error: %s\n", conn->errstr );
					br = 1;
					break;
				default:
					fprintf( stderr, "Error occurred while inserting data to DB\n" );
			}
			if( br ) break;
		}
		bson_destroy( doc );
	}
}


int main( int argc, char *argv[] ) {
	int err;
	
	// Need at least two arguments
	if( argc < 3 ) {
		char *bn;
		bn = strdup( argv[0] );
		bn = basename( bn );
		fprintf( stderr, "Usage:   %s  mq_name  db_name  [db_host  [db_port]]\n", bn );
		fprintf( stderr, "         mq_name    Message queue path-name\n" );
		fprintf( stderr, "         db_name    MongoDB database name\n" );
		fprintf( stderr, "         db_host    MongoDB server address (default '%s')\n", DB_HOST_DEFAULT );
		fprintf( stderr, "         db_port    MongoDB server port (default %d)\n", DB_PORT_DEFAULT );
		return 1;
	}
	mq_path = strdup( argv[1] );
	db_name = strdup( argv[2] );
	if( argc > 3 )
		db_host = strdup( argv[3] );
	else
		db_host = DB_HOST_DEFAULT;
	if( argc > 4 ) {
		db_port = (int)strtol( argv[4], NULL, 10 );
		if( ! db_port )
			db_port = DB_PORT_DEFAULT;
	} else
		db_port = DB_PORT_DEFAULT;
	
	// Initialize
	err = init();
	if( err )
		return 1;
	
	// Set signal handlers
	signal( SIGUSR1, reopen );
	signal( SIGINT , terminate );
	
	pump();
	
	close_all();
	return 0;
}
