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

#define FASTCOV_VERSION "0.3"

/* {{{ fastcov_functions[]
 */
zend_function_entry fastcov_functions[] = {
	PHP_FE(fastcov_start, NULL)
	PHP_FE(fastcov_running, NULL)
	PHP_FE(fastcov_stop, NULL)
	{NULL, NULL, NULL}
};
/* }}} */

ZEND_DECLARE_MODULE_GLOBALS(fastcov);

/* {{{ fastcov_module_entry
 */
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
/* }}} */

#ifdef COMPILE_DL_FASTCOV
ZEND_GET_MODULE(fastcov);
#endif

/* {{{ prototypes */ 
void fc_ticks_function();
void fc_start(TSRMLS_D);
void fc_clean(TSRMLS_D);
void fc_init(TSRMLS_D);
/* }}} */

/* pointer to the original PHP compile_file */
zend_op_array *(*fc_orig_compile_file) (zend_file_handle *file_handle, int type TSRMLS_DC);

/* we override compile_file to sniff compiled files and prebuild our coverage structures */
/* {{{ fc_compile_file() */
ZEND_DLEXPORT zend_op_array* fc_compile_file (zend_file_handle *file_handle, int type TSRMLS_DC) {

	/* let the original compiler do it's work */
	zend_op_array* ret = fc_orig_compile_file(file_handle, type TSRMLS_CC);

	char *filename = ret->filename;
	uint line_count = ret->opcodes[(ret->last - 1)].lineno;

	/* setup a basic coverage_file structure but do not allocate line counter */
	coverage_file *tmp = emalloc(sizeof(coverage_file));
	/* we can't trust the pointer to our filename, duplicate it */
	tmp->filename = estrdup(filename);
	tmp->lines = NULL;
	tmp->allocated = 0;
	tmp->line_count = line_count;
	tmp->next = NULL;

	/* set the start of the chained list if needed */
	if (FASTCOV_G(first_file) == NULL) {
		FASTCOV_G(first_file) = tmp;
	}

	/* then update the previous element's pointer */
	if (FASTCOV_G(last_file)) {
		FASTCOV_G(last_file)->next = tmp;
	}

	/* finally replace it */
	FASTCOV_G(last_file) = tmp;

	return ret;
}
/* }}} */

/* {{{ fc_ticks_function() */
void fc_ticks_function() {
	TSRMLS_FETCH();

	/* ignore the ticks if we are not running the coverage ( shoudln't happens ) */
	if (FASTCOV_G(running) == 0) {
		return;
	}

	/* retrieve the current opcode's filename and lineno */
	char *filename = zend_get_executed_filename(TSRMLS_C);
	ulong line = zend_get_executed_lineno(TSRMLS_C);

	/* if the current opcode is on a different file than our current_file pointer, switch to it */
	if ((FASTCOV_G(current_file) == NULL) || (strcmp(FASTCOV_G(current_file)->filename, filename) != 0)) {
		coverage_file *file = FASTCOV_G(first_file);
		while (file != NULL) {
			if (strcmp(file->filename, filename) == 0) {
				FASTCOV_G(current_file) = file;
				break;
			}
			file = file->next;
		}
	}

	if (FASTCOV_G(current_file) != NULL) {
		coverage_file *current_file = FASTCOV_G(current_file);
		/* if the line count array wasn't allocated */
		if (current_file->allocated == 0) {
			/* alloc it */
			current_file->lines = emalloc(sizeof(long) * (current_file->line_count + 1));
			/* and zero it */
			memset(current_file->lines, 0, sizeof(long) * (current_file->line_count + 1));
			current_file->allocated = 1;
		}
		/* increment line counter */
		current_file->lines[line]++;
	}
}
/* }}} */

/* {{{ fc_start() */
void fc_start(TSRMLS_D) {
	/* register our tick function */
	php_add_tick_function(fc_ticks_function);
}
/* }}} */

/* {{{ fc_clean() */
void fc_clean(TSRMLS_D) {
	/* unregister tick function */
	php_remove_tick_function(fc_ticks_function);
	/* then remove running status */
	FASTCOV_G(running) = 0;
}
/* }}} */

/* {{{ fc_init() */
void fc_init(TSRMLS_D) {
	/* setup global variables */
	FASTCOV_G(running) = 0;

	/* init our chained list items */
	FASTCOV_G(first_file) = NULL;
	FASTCOV_G(last_file) = NULL;
	FASTCOV_G(current_file) = NULL;
}
/* }}} */

/* {{{ proto bool fastcov_start()
   Starts the code coverage */
PHP_FUNCTION(fastcov_start) {
	if (FASTCOV_G(running) == 1) {
		RETURN_FALSE; /* the coverage is already started, ignore it */
	}
	/* switch to running mode */
	FASTCOV_G(running) = 1;
	fc_start(TSRMLS_C);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool fastcov_running()
   Whether code coverage is enabled or not */
PHP_FUNCTION(fastcov_running) {
	if (FASTCOV_G(running) == 1) {
		RETURN_TRUE; /* the coverage is already started, ignore it */
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto array fastcov_stop()
   Stop the code coverage return an bi-dimensional array with the file list and call count for each line */
PHP_FUNCTION(fastcov_stop) {
	zval *file_coverage;

	/* stop covering */
	fc_clean(TSRMLS_C);

	/* initialise return array */
	array_init(return_value);

	/* retrieve first file */
	coverage_file *file = FASTCOV_G(first_file);
	while (file != NULL) {
		/* initialise local array for returning lines */
		MAKE_STD_ZVAL(file_coverage);
		array_init(file_coverage);

		/* check if the file was called even once */
		if (file->allocated == 1) {
			/* then iterate on each line */
			ulong i;
			for (i = 0; i < file->line_count; i++) {
				if (file->lines[i] > 0) {
					/* if there were calls on this line, return it */
					add_index_long(file_coverage, i, file->lines[i]);
				}
			}
		}

		/* then add line array to the main array using the filename as a key */
		add_assoc_zval(return_value, file->filename, file_coverage);
		file = file->next;
	}
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(fastcov) {
#ifdef ZTS
	ZEND_INIT_MODULE_GLOBALS(fastcov, NULL, NULL);
#endif
	// then override the compile_file call to catch the file names and line count
	fc_orig_compile_file = zend_compile_file;
	zend_compile_file = fc_compile_file;

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(fastcov) {
	// restore initial compile callback
	if (zend_compile_file == fc_compile_file) {
		zend_compile_file = fc_orig_compile_file;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(fastcov) {
	// we need to setup ticks very soon in the script
	zval ticks;
	ZVAL_LONG(&ticks, 1);
	CG(declarables).ticks = ticks;

	fc_init(TSRMLS_C);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(fastcov) {
	// stop covering
	fc_clean(TSRMLS_C);

	// free any used memory
	coverage_file *file = FASTCOV_G(first_file);
	while (file != NULL) {
		coverage_file *tmp = file;
		file = tmp->next;
		
		efree(tmp->filename);
		if (tmp->allocated) {
			efree(tmp->lines);
		}
		efree(tmp);
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(fastcov)
{
	php_info_print_table_start();
	php_info_print_table_header(1, "fastcov");
	php_info_print_table_row(2, "version", FASTCOV_VERSION);
	php_info_print_table_end();
}
/* }}} */


