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

ZEND_DECLARE_MODULE_GLOBALS(fastcov);

#define FASTCOV_VERSION "0.2"

static ZEND_DLEXPORT void (*_zend_ticks_function) (int ticks);
static zend_op_array * (*_zend_compile_file) (zend_file_handle *file_handle,
                                              int type TSRMLS_DC);

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

#ifdef COMPILE_DL_FASTCOV
ZEND_GET_MODULE(fastcov);
#endif

void fc_clean();

ZEND_DLEXPORT zend_op_array* fc_compile_file (zend_file_handle *file_handle,
                                             int type TSRMLS_DC) {
	// let the original compiler do it's work
	zend_op_array* ret = _zend_compile_file(file_handle, type TSRMLS_CC);

	char *filename = ret->filename;
	uint line_count = ret->opcodes[(ret->last - 1)].lineno;

	// setup a basic coverage_file structure but do not allocate line counter
	coverage_file *tmp = emalloc(sizeof(coverage_file));
	// we can't trust the pointer to our filename, duplicate it
	tmp->filename = estrdup(filename);
	tmp->lines = NULL;
	tmp->allocated = 0;
	tmp->line_count = line_count;
	tmp->next = NULL;

	// setup start of chained list if needed
	if (FASTCOV_GLOBALS(first_file) == NULL) {
		FASTCOV_GLOBALS(first_file) = tmp;
	}

	// then update the last element
	if (FASTCOV_GLOBALS(last_file)) {
		FASTCOV_GLOBALS(last_file)->next = tmp;
	}

	FASTCOV_GLOBALS(last_file) = tmp;

	return ret;
}

ZEND_DLEXPORT void fc_ticks_function (int ticks) {
	TSRMLS_FETCH();

	char *filename = zend_get_executed_filename(TSRMLS_C);
	long line = zend_get_executed_lineno(TSRMLS_C);

	// if the current opcode is on a different file, switch the current_file variable
	if (!FASTCOV_GLOBALS(current_file) || (strcmp(FASTCOV_GLOBALS(current_file)->filename, filename) != 0)) {
		coverage_file *file = FASTCOV_GLOBALS(first_file);
		while (file != NULL) {
			if (strcmp(file->filename, filename) == 0) {
				FASTCOV_GLOBALS(current_file) = file;
				break;
			}
			file = file->next;
		}
	}

	if (FASTCOV_GLOBALS(current_file) != NULL) {
		coverage_file *current_file = FASTCOV_GLOBALS(current_file);
		// if the line count array wasn't allocated
		if (!current_file->allocated) {
			// alloc it
			current_file->lines = emalloc(sizeof(uint) * current_file->line_count);
			// and zero it
			memset(current_file->lines, 0, sizeof(uint) * current_file->line_count);
			current_file->allocated = 1;
		}
		// increment line counter
		current_file->lines[line]++;
	}
}

void fc_start(TSRMLS_D) {
	// add our tick function
	php_add_tick_function(fc_ticks_function);
}

void fc_clean(TSRMLS_D) {
	if (FASTCOV_GLOBALS(running) == 0) return;
	FASTCOV_GLOBALS(running) = 0;
}

void fc_init(TSRMLS_D) {
	// setup global variables
	FASTCOV_GLOBALS(running) = 0;

	// init our chained list items to NULL
	FASTCOV_GLOBALS(first_file) = NULL;
	FASTCOV_GLOBALS(last_file) = NULL;
	FASTCOV_GLOBALS(current_file) = NULL;
}

PHP_FUNCTION(fastcov_start) {
	// switch to running mode
	FASTCOV_GLOBALS(running) = 1;
	fc_start(TSRMLS_C);
}

PHP_FUNCTION(fastcov_stop) {
	zval *file_coverage;

	// stop covering
	fc_clean(TSRMLS_C);

	// initialise return array
	array_init(return_value);
	// retrieve first file
	coverage_file *file = FASTCOV_GLOBALS(first_file);
	do {
		// initialise local array for returning lines
		MAKE_STD_ZVAL(file_coverage);
		array_init(file_coverage);

		// then iterate on each line
		uint i;
		for (i = 0; i < file->line_count; i++) {
			if (file->lines[i] > 0) {
				// if there were calls on this line, return it
				add_index_long(file_coverage, i, file->lines[i]);
			}
		}

		// then add line array to the main array using the filename as a key
		add_assoc_zval(return_value, file->filename, file_coverage);
	} while((file->next != NULL) && (file = file->next));

}

/**
 * Module init callback.
 */
PHP_MINIT_FUNCTION(fastcov) {
#ifdef ZTS
	ZEND_INIT_MODULE_GLOBALS(fastcov, NULL, NULL);
#endif
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
	// we need to setup ticks very soon in the script
	zval ticks;
	ZVAL_LONG(&ticks, 1);
	CG(declarables).ticks = ticks;

	fc_init(TSRMLS_C);

	// then override the compile_file call to catch the file names and line count
	_zend_compile_file = zend_compile_file;
	zend_compile_file = fc_compile_file;
	
	return SUCCESS;
}

/**
 * Request shutdown callback. Stop profiling and return.
 */
PHP_RSHUTDOWN_FUNCTION(fastcov) {
	// restore initial compile callback
	zend_compile_file = _zend_compile_file;

	// stop covering
	fc_clean(TSRMLS_C);

	// free any used memory
	coverage_file *file = FASTCOV_GLOBALS(first_file);
	coverage_file *next;
	do {
		next = file->next;
		efree(file->filename);
		if (file->allocated) {
			// free lines if allocated
			efree(file->lines);
		}
		efree(file);
	} while((next != NULL) && (file = next));

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


