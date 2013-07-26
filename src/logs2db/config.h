#ifndef _CONFIG_H_
#define _CONFIG_H_

/*
 * Common options
 */
/** Enable debug messages to logs. */
#define USE_DEBUG	1
/** Unprivileged user. */
#define USER		"nobody"
/** Local state directory (/var). */
#define RUNDIR		"./tmp"
/** Null device path. */
#define DEVNULL		"/dev/null"
/** Delay before next attempt if error occurred. */
#define INIT_DELAY_ON_ERROR	10

/*
 * Message queue options
 */
/** Maximum reserved message size. */
#define MSG_SIZE	4096
/** Maximum count messages in queue before blocking. */
#define MSG_LIMIT	2048
/** Default message queue pathname. */
#define MQ_PATH		"/sqam_logs"

/*
 * MongoDB options
 */
/** Default database hostname. */
#define DB_HOST_DEFAULT	"127.0.0.1"
/** Default database port. */
#define DB_PORT_DEFAULT	27017
/** Default database. */
#define DB_NAME_DEFAULT	"sqam"
/** Raw logs collection name. */
#define DB_RAW_LOG_COLL	"log_raw"


#endif /* _CONFIG_H_ */
