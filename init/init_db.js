// Parameters
var DBNAME = "sam";
var RAWLOG_SIZE = 50; // in mebibytes

// connect to server
conn = new Mongo();
print( "Connecting to `"+DBNAME+"' database" );
db = conn.getDB( DBNAME );

// existing collection
var curr = db.getCollectionNames();

if( curr.indexOf("log_raw") == -1 ) {
	db.createCollection( "log_raw", { capped: true, size: RAWLOG_SIZE*1024*1024, autoIndexId: false } );
	db.log_raw.ensureIndex( { time: -1 } );
	print( "\tCollection `log_raw' created" );
} else {
	print( "\tCollection `log_raw' already exists" );
}

