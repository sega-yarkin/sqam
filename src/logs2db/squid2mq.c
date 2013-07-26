#include <libgen.h>
#include <mqueue.h>

#include "config.h"
#include "utils.h"

/** Syslog ident, if NULL used default value (basename of file). */
#define SYSLOG_NAME	NULL


char* mq_path;		/**< Message queue name */
mqd_t mq;		/**< Message queue pointer */
int   lock;		/**< Lock file descriptor */


/**
 * Prepare resources.
 *
 * @returns		0 on success
 */
int init() {
	mq = mq_open( mq_path, O_WRONLY );
	if( mq == -1 ) {
		ERR( "Cannot open mqueue for writing" );
		return 1;
	}
	return EOK;
}

/**
 * Pumping messages from stdin to message queue.
 */
void pump() {
	char buf[ MSG_SIZE+1 ];		/**< Message buffer */
	int len;			/**< Message length */
	memset( buf, 0x00, sizeof buf );
	
	/* pump loop */
	DEBUG( "Start pumping" );
	while( fgets(buf, sizeof buf, stdin) ) {
		len = strlen( buf );
		DEBUG( "Received new log, length = %d", len );
		if( mq_send(mq, buf, len, 0) != EOK )
			ERR( "Error while mq_send" );
	}
}

/**
 * Main function.
 */
int main( int argc, char *argv[] ) {
	if( argc>1 && strcmp(argv[1],"-h")==0 ) {
		char *bn;
		bn = strdup( argv[0] );
		bn = basename( bn );
		fprintf( stdout, "Usage:   %s  [mq_name]\n", bn );
		fprintf( stdout, "         mq_name    Message queue path-name (default: '%s')\n\n", MQ_PATH );
		exit( EXIT_FAILURE );
	}
	redir_io( 0, SYSLOG_NAME );
	mq_path = ( argc > 1 ) ? strdup(argv[1]) : MQ_PATH;
	
	DEBUG( "MQ name = %s", mq_path   );
	
	/* initialize */
	if( init()!=EOK )
		exit( EXIT_FAILURE );
	
	/* infinite loop */
	pump();
	
	/* close all if loop aborted */
	DEBUG( "Terminating" );
	mq_close( mq );
	exit( EXIT_SUCCESS );
}
