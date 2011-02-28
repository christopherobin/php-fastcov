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

#ifdef ZTS
#include "TSRM.h"
#endif

typedef struct _coverage_file {
	char *filename;
	uint *lines;
    char allocated;
	uint line_count;
	struct _coverage_file *next;
} coverage_file;

ZEND_BEGIN_MODULE_GLOBALS(fastcov)
    // our file coverage chained list
    coverage_file *first_file;
    coverage_file *last_file;
    // the currently parsed file
    coverage_file *current_file;
    // ticks before we override them
    zval ticks_constant;
    char running;
ZEND_END_MODULE_GLOBALS(fastcov)

#ifdef ZTS
	#define FASTCOV(v) TSRMG(fastcov_globals_id, zend_fastcov_globals*, v)
#else
	#define FASTCOV(v) (fastcov_globals.v)
#endif

PHP_MINIT_FUNCTION(fastcov);
PHP_MSHUTDOWN_FUNCTION(fastcov);
PHP_RINIT_FUNCTION(fastcov);
PHP_RSHUTDOWN_FUNCTION(fastcov);
PHP_MINFO_FUNCTION(fastcov);

PHP_FUNCTION(fastcov_start);
PHP_FUNCTION(fastcov_stop);

#endif /* PHP_FASTCOV_H_ */
