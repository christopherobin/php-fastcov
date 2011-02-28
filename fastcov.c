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

ZEND_GET_MODULE(fastcov);
ZEND_DECLARE_MODULE_GLOBALS(fastcov);

ZEND_DLEXPORT zend_op_array* fc_compile_file (zend_file_handle *file_handle,
                                             int type TSRMLS_DC) {
	// let the original compiler do it's work
	zend_op_array* ret = _zend_compile_file(file_handle, type TSRMLS_CC);

	char *filename = ret->filename;
	uint line_count = ret->opcodes[(ret->last - 1)].lineno;

	// setup a basic coverage_file structure but do not allocate line counter
	coverage_file *tmp = emalloc(sizeof(coverage_file));
	tmp->filename = filename;
	tmp->lines = NULL;
	tmp->allocated = 0;
	tmp->line_count = line_count;
	tmp->next = NULL;

	// setup start of chained list if needed
	if (FASTCOV(first_file) == NULL) {
		FASTCOV(first_file) = tmp;
	}

	// then update the last element
	if (FASTCOV(last_file)) {
		FASTCOV(last_file)->next = tmp;
	}

	FASTCOV(last_file) = tmp;

	return ret;
}

ZEND_DLEXPORT void fc_ticks_function (int ticks) {
	char *filename = zend_get_executed_filename();
	long line = zend_get_executed_lineno();

	// if the current opcode is on a different file, switch the current_file variable
	if (!FASTCOV(current_file) || FASTCOV(current_file)->filename != filename) {
		coverage_file *file = FASTCOV(first_file);
		do {
			if (file->filename == filename) {
				FASTCOV(current_file) = file;
				break;
			}
		} while((file->next != NULL) && (file = file->next));
	}

	if (FASTCOV(current_file) != NULL) {
		coverage_file *current_file = FASTCOV(current_file);
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

	// send event to original tick function
	if (_zend_ticks_function) {
		_zend_ticks_function(ticks);
	}
}

void fc_start() {
	// replace the tick function by our function
	_zend_ticks_function = zend_ticks_function;
	zend_ticks_function = fc_ticks_function;
}

PHP_FUNCTION(fastcov_start) {
	fc_start();
}

PHP_FUNCTION(fastcov_stop) {
	zval *file_coverage;

	// initialise return array
	array_init(return_value);
	// retrieve first file
	coverage_file *file = FASTCOV(first_file);
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
	// the tick global uses a zval as a value
	ZVAL_LONG(&FASTCOV(ticks_constant), 1);

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
	CG(declarables).ticks = FASTCOV(ticks_constant);

	// init our chained list items to NULL
	FASTCOV(first_file) = NULL;
	FASTCOV(last_file) = NULL;
	FASTCOV(current_file) = NULL;

	// then override the compile_file call to catch the file names and line count
	_zend_compile_file = zend_compile_file;
	zend_compile_file = fc_compile_file;

	return SUCCESS;
}

/**
 * Request shutdown callback. Stop profiling and return.
 */
PHP_RSHUTDOWN_FUNCTION(fastcov) {
	// free any used memory
	coverage_file *file = FASTCOV(first_file);
	coverage_file *next;
	do {
		next = file->next;
		if (file->allocated) {
			// free lines if allocated
			efree(file->lines);
		}
		efree(file);
	} while((next != NULL) && (file = next));
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


