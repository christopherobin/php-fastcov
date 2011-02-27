/*
 * fastcov.c
 *
 *  Created on: Feb 26, 2011
 *      Author: Christophe Robin
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_fastcov.h"

#define FASTCOV_VERSION "0.1"

zval ticks_constant;
zval *coverage_table;

static ZEND_DLEXPORT void (*_zend_ticks_function) (int ticks);

/* List of functions implemented/exposed by fastcov */
zend_function_entry fastcov_functions[] = {
	PHP_FE(fastcov_start, NULL)
	PHP_FE(fastcov_stop, NULL)
	{NULL, NULL, NULL}
};

/* Callback functions for the fastcov extension */
zend_module_entry fastcov_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"fastcov",                        /* Name of the extension */
	fastcov_functions,                /* List of functions exposed */
	PHP_MINIT(fastcov),               /* Module init callback */
	PHP_MSHUTDOWN(fastcov),           /* Module shutdown callback */
	PHP_RINIT(fastcov),               /* Request init callback */
	PHP_RSHUTDOWN(fastcov),           /* Request shutdown callback */
	PHP_MINFO(fastcov),               /* Module info callback */
#if ZEND_MODULE_API_NO >= 20010901
	FASTCOV_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};

ZEND_GET_MODULE(fastcov);

ZEND_DLEXPORT void fc_ticks_function (int ticks) {
	char *filename = zend_get_executed_filename();
	long line = zend_get_executed_lineno();

	zval *rval;
	zval **val = NULL;
	if (!zend_hash_find(coverage_table->value.ht, filename, strlen(filename)+1, (void**) &val) == SUCCESS) {
		zval *lval;
		array_init_size(lval, 512);

		zend_hash_add(coverage_table->value.ht, filename, strlen(filename)+1, &lval, sizeof(zval*), NULL);
		rval = lval;
	} else {
		rval = *val;
	}

	if (zend_hash_index_exists(rval->value.ht, line) == SUCCESS) {
		zval *zline;
		MAKE_STD_ZVAL(zline);
		ZVAL_NULL(zline); // for now

		zend_hash_index_update(rval->value.ht, line, &zline, sizeof(zval*), NULL);
	}

	// send event to original tick function
	if (_zend_ticks_function) {
		_zend_ticks_function(ticks);
	}
}

void fc_start() {
	//zend_hash_init(coverage_table, 64, NULL, ZVAL_PTR_DTOR, 0);
	array_init_size(coverage_table, 64);

	_zend_ticks_function = zend_ticks_function;
	zend_ticks_function = fc_ticks_function;
}

PHP_FUNCTION(fastcov_start) {
	fc_start();
}

PHP_FUNCTION(fastcov_stop) {
	RETVAL_ZVAL(coverage_table, NULL, NULL);
}

/**
 * Module init callback.
 */
PHP_MINIT_FUNCTION(fastcov) {
	ZVAL_LONG(&ticks_constant, 1);
	return SUCCESS;
}

/**
 * Module shutdown callback.
 */
PHP_MSHUTDOWN_FUNCTION(fastcov) {
  return SUCCESS;
}

/**
 * Request init callback. Nothing to do yet!
 */
PHP_RINIT_FUNCTION(fastcov) {
	CG(declarables).ticks = ticks_constant;
	ALLOC_ZVAL(coverage_table);
	//ALLOC_HASHTABLE(coverage_table);
	return SUCCESS;
}

/**
 * Request shutdown callback. Stop profiling and return.
 */
PHP_RSHUTDOWN_FUNCTION(fastcov) {
	// TODO: clean coverage table
	return SUCCESS;
}

/**
 * Module info callback. Returns the fastcov version.
 */
PHP_MINFO_FUNCTION(fastcov)
{
  php_info_print_table_start();
  php_info_print_table_header(2, "fastcov", FASTCOV_VERSION);
  php_info_print_table_end();
}


