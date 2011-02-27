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

PHP_MINIT_FUNCTION(fastcov);
PHP_MSHUTDOWN_FUNCTION(fastcov);
PHP_RINIT_FUNCTION(fastcov);
PHP_RSHUTDOWN_FUNCTION(fastcov);
PHP_MINFO_FUNCTION(fastcov);

PHP_FUNCTION(fastcov_start);
PHP_FUNCTION(fastcov_stop);

#endif /* PHP_FASTCOV_H_ */
