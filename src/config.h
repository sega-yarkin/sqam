
#ifndef _CONFIG_H_
#define _CONFIG_H_
/*
 * Common options
 */
/** Unprivileged user. */
#define USER		"nobody"
/** Local state directory (/var). */
#define LOCALSTATEDIR	"./tmp"

/*
 * Message queue options
 */
/** Maximum reserved message size. */
#define MSG_SIZE	4096
/** Maximum count messages in queue before blocking. */
#define MSG_LIMIT	2048
/** Default message queue pathname. */
#define MQ_PATH		"/sqam_fifo2db"

/*
 * MongoDB options
 */
/** Default database hostname. */
#define DB_HOST_DEFAULT	"127.0.0.1"
/** Default database port. */
#define DB_PORT_DEFAULT	27017
/** Raw logs collection name. */
#define DB_RAW_LOG_COLL	"log_raw"

#endif
