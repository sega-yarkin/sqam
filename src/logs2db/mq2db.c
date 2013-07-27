/**
 * Message queue -> MongoDB utilite.
 * Read specified message queue and puts all
 * log messages to raw log capped collection
 * in MongoDB.
 *
 * @author	Sergey Yarkin
 * @version	0.1.0
 */


#include <libgen.h>
#include <mqueue.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "mongo.h"
#include "config.h"
#include "utils.h"

/** Path to .lock, in which will be written PID of the daemon. */
#define LOCKFILE	RUNDIR "/mq2db.lock"
/** Syslog ident, if NULL used default value (basename of file). */
#define SYSLOG_NAME	NULL


char* mq_path;		/**< Message queue name */
mqd_t mq = 0;		/**< Message queue pointer */
int   lock;		/**< Lock file descriptor */

char* db_name;		/**< Database name */
char* db_host;		/**< Host where located MongoDB */
int   db_port;		/**< Port number of MongoDB */
char  db_ns[100];	/**< Raw log collection ns */
mongo conn[1];		/**< MongoDB connection */
int   opened = 0;	/**< Connection status */


/**
 * Initialize message queue.
 *
 * @param path		Message queue pathname
 */
int init_mq( char* path ) {
	DEBUG( "MQ: message size = %d bytes", MSG_SIZE );
	DEBUG( "MQ: messages count limit = %d", MSG_LIMIT );
	DEBUG( "MQ: needed %d bytes", MQ_MEMORY );
	/* Get message queue memory limit */
	struct rlimit rl;
	if( getrlimit(RLIMIT_MSGQUEUE, &rl) != EOK ) {
		ERR( "Cannot get current memory limit for mqueue" );
		return 1;
	}
	DEBUG( "MQ: current limit is %d bytes, maximum is %d bytes", (int)rl.rlim_cur, (int)rl.rlim_max );
	if( rl.rlim_max < MQ_MEMORY ) {
		/* Set message queue memory limit */
		rl.rlim_cur = MQ_MEMORY;
		rl.rlim_max = MQ_MEMORY;
		if( setrlimit(RLIMIT_MSGQUEUE, &rl) != EOK ) {
			ERR( "Cannot set memory limit for mqueue" );
			return 1;
		}
		DEBUG( "MQ: limit has been increased successfully" );
	}
	/* Create message queue */
	mode_t um = umask( 0000 );
	struct mq_attr attr = { 0, MSG_LIMIT, MSG_SIZE, 0 };
	mq = mq_open( mq_path, O_RDONLY | O_CREAT, 0722, &attr );
	if( mq == -1 ) {
		ERR( "Cannot create message queue" );
		return 1;
	}
	umask( um );
	DEBUG( "MQ: success" );
	return EOK;
}

/**
 * Connect to MongoDB.
 *
 * @param conn		MongoDB connection object
 * @param host		MongoDB host
 * @param port		MongoDB port
 * @param db		Work database
 * @param ns		Buffer for output ns for raw log collection
 * @param ns_size	Size of ns buffer
 */
int init_mongo( mongo* conn, char* host, int port, char* db, char* ns, int ns_size ) {
	mongo_init( conn );
	mongo_set_op_timeout( conn, 1000 );
	if( mongo_client(conn, host, port) != MONGO_OK ) {
		char* e;
		switch( conn->err ) {
			case MONGO_CONN_NO_SOCKET:  e = "no socket";              break;
			case MONGO_CONN_FAIL:       e = "connection failed";      break;
			case MONGO_CONN_NOT_MASTER: e = "not master";             break;
			case MONGO_CONN_ADDR_FAIL:  e = "cannot resolve address"; break;
			default:                    e = "unknown error";
		}
		WARN( "Cannot connect to MongoDB: %s", e );
		return 1;
	}
	if( snprintf(ns, ns_size, "%s.%s", db, DB_RAW_LOG_COLL) < 3 ) {
		WARN( "Cannot make ns string" );
		return 1;
	}
	return EOK;
}

/**
 * Initialization sequence.
 */
int init_sequence() {
	if( init_mq(mq_path) != EOK )
		return 1;
	if( init_mongo(conn, db_host, db_port, db_name, &db_ns[0], sizeof db_ns) != EOK )
		return 1;
	return EOK;
}

/**
 * Initialize resources.
 */
int init() {
	while( init_sequence() != EOK ) {
		sleep( INIT_DELAY_ON_ERROR );
	}
	opened = 1;
	return EOK;
}

/**
 * Close all resources.
 */
int close_all() {
	mq_close( mq );
	mongo_destroy( conn );
	opened = 0;
	return EOK;
}

/**
 * Reinitialize all resources.
 */
void reopen() {
	if( ! opened )
		return;
	close_all();
	init();
}

/**
 * Normal terminate by signal.
 */
void terminate() {
	DEBUG( "Terminating" );
	close_all();
	exit( EXIT_SUCCESS );
}

/**
 * Pumping messages from message queueto database.
 */
void pump() {
	ssize_t rcvd;
	char    buf[ MSG_SIZE+10 ];
	bson    doc[1];
	int64_t usec;
	struct timeval tv;
	
	while( 1 ) {
		/* Receive message from mqueue */
		rcvd = mq_receive( mq, (char*)&buf, sizeof buf, NULL );
		if( rcvd < 0 ) {
			ERR( "Receive message error" );
			continue;
		}
		buf[ rcvd ] = '\0';
		//DEBUG( "Received new log, length = %d", (int)rcvd );
		
		/* Send to MongoDB */
		bson_init( doc );
		gettimeofday( &tv, NULL );
		usec = (int64_t) (tv.tv_sec) * 1000000 + tv.tv_usec;
		bson_append_long  ( doc, "time", usec );
		bson_append_string( doc, "data", &buf[0] );
		bson_finish( doc );
		while( mongo_insert(conn, db_ns, doc, NULL) != MONGO_OK ) {
			int br = 0;
			switch( conn->err ) {
				case MONGO_IO_ERROR:
				case MONGO_SOCKET_ERROR:
					reopen();
					break;
				case MONGO_WRITE_ERROR:
					WARN( "MongoDB error: Write with given write_concern returned an error" );
					br = 1;
					break;
				case MONGO_NS_INVALID:
					WARN( "MongoDB error: The name for the ns (%s) is invalid.", db_ns );
					exit( EXIT_FAILURE );
					break;
				case MONGO_BSON_INVALID:
				case MONGO_BSON_NOT_FINISHED:
				case MONGO_BSON_TOO_LARGE:
					WARN( "MongoDB BSON error: %s", conn->errstr );
					br = 1;
					break;
				default:
					WARN( "Error occurred while inserting data to DB" );
			}
			if( br ) break;
		}
		bson_destroy( doc );
	}
}


int main( int argc, char *argv[] ) {
	if( argc==2 && strcmp(argv[1],"-h")==0 ) {
		char *bn;
		bn = strdup( argv[0] );
		bn = basename( bn );
		fprintf( stderr, "Usage:   %s  [mq_name  [db_name  [db_host  [db_port]]]]\n", bn );
		fprintf( stderr, "         mq_name    Message queue path-name (default: '%s')\n", MQ_PATH );
		fprintf( stderr, "         db_name    MongoDB database name (default: '%s')\n", DB_NAME_DEFAULT );
		fprintf( stderr, "         db_host    MongoDB server address (default: '%s')\n", DB_HOST_DEFAULT );
		fprintf( stderr, "         db_port    MongoDB server port (default: %d)\n\n", DB_PORT_DEFAULT );
		exit( EXIT_FAILURE );
	}
	redir_io( 1, SYSLOG_NAME );
	mq_path = ( argc > 1 ) ? strdup(argv[1]) : MQ_PATH;
	db_name = ( argc > 2 ) ? strdup(argv[2]) : DB_NAME_DEFAULT;
	db_host = ( argc > 3 ) ? strdup(argv[3]) : DB_HOST_DEFAULT;
	db_port = ( argc > 4 ) ? (int)strtol(argv[4], NULL, 10) : DB_PORT_DEFAULT;
	
	/* initialize */
	if( lock_file(LOCKFILE, &lock, 0) != EOK )
		exit( EXIT_FAILURE );
	if( init()!=EOK || daemonize()!=EOK )
		exit( EXIT_FAILURE );
	save_pid( lock );
	signal( SIGTERM, terminate );
	signal( SIGUSR1, reopen );
	
	/* infinite loop */
	pump();
	
	/* close all if loop aborted */
	close_all();
	return EXIT_FAILURE;
}
