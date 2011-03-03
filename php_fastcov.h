/*
 * php_fastcov.h
 *
 *  Created on: Feb 26, 2011
 *      Author: Christophe Robin
 */

#ifndef PHP_FASTCOV_H_
#define PHP_FASTCOV_H_

extern zend_module_entry fastcov_module_entry;
#define phpext_fastcov_ptr &fastcov_module_entry

#ifdef PHP_WIN32
#define PHP_FASTCOV_API __declspec(dllexport)
#else
#define PHP_FASTCOV_API
#endif

/* {{{ typedef struct _coverage_file */
typedef struct _coverage_file {
	char *filename;
	intptr_t filename_ptr;
	long *lines;
	char allocated;
	uint line_count;
	struct _coverage_file *next;
} coverage_file;
/* }}} */

/* {{{ ZEND_BEGIN_MODULE_GLOBALS */
ZEND_BEGIN_MODULE_GLOBALS(fastcov)
	/* our file coverage chained list */
	coverage_file *first_file;
	coverage_file *last_file;
	coverage_file *current_file;
	intptr_t current_filename_ptr;
	/* whether we must run in high compatibility mode or not */
	int high_compatibility;
	/* whether code coverage is running or not */
	int running;
	/* output directory */
	char *output_dir;
ZEND_END_MODULE_GLOBALS(fastcov)
/* }}} */

ZEND_EXTERN_MODULE_GLOBALS(fastcov)

#ifdef ZTS
#	include "TSRM.h"
#	define FASTCOV_G(v) TSRMG(fastcov_globals_id, zend_fastcov_globals *, v)
#	define FASTCOV_GLOBALS ((zend_fastcov_globals *) (*((void ***) tsrm_ls))[TSRM_UNSHUFFLE_RSRC_ID(fastcov_globals_id)])
#else
#	define FASTCOV_G(v) (fastcov_globals.v)
#	define FASTCOV_GLOBALS (&fastcov_globals)
#endif

PHP_MINIT_FUNCTION(fastcov);
PHP_MSHUTDOWN_FUNCTION(fastcov);
PHP_RINIT_FUNCTION(fastcov);
PHP_RSHUTDOWN_FUNCTION(fastcov);
PHP_MINFO_FUNCTION(fastcov);

PHP_FUNCTION(fastcov_start);
PHP_FUNCTION(fastcov_running);
PHP_FUNCTION(fastcov_stop);

#endif /* PHP_FASTCOV_H_ */
