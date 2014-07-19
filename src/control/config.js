/**
 * SQAM Control configurtion.
 *
 * @author	Sergey I. Yarkin
 * @version	0.1.0
 */

let conf = {
	run_dir: "../../tmp",
	debug: true,

	/** Database params */
	db: {
		host: "127.0.0.1",	/**< MongoDB host */
		port:  27017,		/**< MongoDB port */
		name: "sqam"		/**< MongoDB database */
	}
};

module.exports = conf;
