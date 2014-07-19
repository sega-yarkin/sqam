/**
 * Utilites & rotines.
 *
 * @author	Sergey I. Yarkin <sega.yarkin@gmail.com>
 * @version	1.0.1
 * @extends	util
 */

'use strict';

/**
 * Extended Node util module.
 *
 * @type Object
 */
let util = require( 'util' );

/**
 * Return object type.
 * Types: undefined, null, boolean, number, string, array, object, function, regexp, date, error
 * 
 * @public
 * @param {Object} o		Object for getting type
 * @returns {String}
 */
util.getType = function getType( o ) {
	let ot = typeof o; // 'object' is included: array, object, regexp, date, error
	if( ot === 'object' ) {
		if( Array.isArray(o) ) ot = 'array';
		else if( o instanceof Date   ) ot = 'date';
		else if( o instanceof RegExp ) ot = 'regexp';
		else if( o instanceof Error  ) ot = 'error';
	}
	return ot;
};

/**
 * Convert array of objects to hash with keys of values `key` field of each object.
 *
 * @public
 * @param {Array} arr		Source array of objects
 * @param {String} key		Field name whose value will used for keys
 * @returns {Object}
 */
util.arrayToHash = function arrayToHash( arr, key ) {
	let res = {};
	for( let i in arr ) {
		if( arr[i][key] !== undefined ) {
			res[ arr[i][key] ] = arr[i];
		}
	}
	return res;
};

module.exports = util;
