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
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/md5.h"
#include "php_fastcov.h"

#define FASTCOV_VERSION "0.9.0"

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

#ifdef P_tmpdir
# define FASTCOV_TMP_DIR P_tmpdir
#else
# define FASTCOV_TMP_DIR "/tmp"
#endif

/* {{{ PHP_INI */
PHP_INI_BEGIN()
/* automatically starts the coverage, you may provide
 * either an output dir in fastcov.output_dir or use 
 * fastcov_stop() yourself */
PHP_INI_ENTRY("fastcov.auto_start", "0", PHP_INI_ALL, NULL)
/* automatically output the coverage data in a file when
 * stopping code coverage, On is implied if auto_start is
 * enabled */
PHP_INI_ENTRY("fastcov.auto_output", "0", PHP_INI_ALL, NULL)
/* output directory for coverage files, php must have write
 * permission in this directory */
STD_PHP_INI_ENTRY("fastcov.output_dir", FASTCOV_TMP_DIR, PHP_INI_ALL, OnUpdateString, output_dir, zend_fastcov_globals, fastcov_globals)

PHP_INI_END()
/* }}} */

#ifdef COMPILE_DL_FASTCOV
ZEND_GET_MODULE(fastcov);
#endif

/* {{{ prototypes */ 
fastcov_coverage_file *fc_register_file(zend_op_array *op_array TSRMLS_DC);
int fc_free_allocated_data(void *item TSRMLS_DC);
int fc_build_array_element(void *item, void *return_value_ptr TSRMLS_DC);
void fc_ticks_function();
void fc_check_context(char *filename TSRMLS_DC);
int fc_start(TSRMLS_D);
void fc_clean(zend_bool TSRMLS_DC);
void fc_init(TSRMLS_D);
void fc_output(TSRMLS_D);
/* }}} */

/* {{{ fc_register_file() */
fastcov_coverage_file *fc_register_file(zend_op_array *op_array TSRMLS_DC) {
	/* let the original compiler do it's work */
	zend_op_array* ret = op_array;

	char *filename = ret->filename;
	uint line_count = ret->opcodes[(ret->last - 1)].lineno;

	/* setup a basic coverage_file structure but do not allocate line counter */
	fastcov_coverage_file tmp, *res;
	/* we can't trust the pointer to our filename, duplicate it */
	tmp.filename = estrdup(filename);
	tmp.filename_ptr = (intptr_t)filename;
	tmp.lines = NULL;
	tmp.allocated = 0;
	tmp.line_count = line_count;
	tmp.next = NULL;

	zend_hash_add(&FASTCOV_G(covered_files), filename, strlen(filename), &tmp, sizeof(fastcov_coverage_file), (void**)&res);

	return res;
}
/* }}} */

/* {{{ fc_free_allocated_data() */
int fc_free_allocated_data(void *item TSRMLS_DC) {
	fastcov_coverage_file *file = (fastcov_coverage_file*)item;

	efree(file->filename);
	if (file->allocated == 1) {
		efree(file->lines);
	}

	return ZEND_HASH_APPLY_KEEP;
}
/* }}} */

/* {{{ fc_build_array_element() */
int fc_build_array_element(void *item, void *return_value_ptr TSRMLS_DC) {
	zval *file_coverage;
	zval *return_value = (zval*)return_value_ptr;
	fastcov_coverage_file *file = (fastcov_coverage_file*)item;

	/* initialise local array for returning lines */
	MAKE_STD_ZVAL(file_coverage);
	array_init(file_coverage);

	/* check if the file was called even once */
	if (file->allocated == 1) {
		/* then iterate on each line */
		ulong line;
		for (line = 0; line < file->line_count; line++) {
			if (file->lines[line] > 0) {
				/* if there were calls on this line, return it */
				add_index_long(file_coverage, line, file->lines[line]);
			}
		}
	}

	/* then add line array to the main array using the filename as a key */
	add_assoc_zval(return_value, file->filename, file_coverage);
}
/* }}} */

/* {{{ fc_check_context() */
void fc_check_context(char *filename TSRMLS_DC) {
	intptr_t filename_ptr = (intptr_t)filename;
	/* if the current opcode is on a different file than our current_filename_ptr pointer, switch to it.
	/* we don't use strcmp there in high_compatibility as it would reduce performances on large section of
	 * procedural code where the op_array stay the same */
	if ((FASTCOV_G(current_filename_ptr) == 0) || (FASTCOV_G(current_filename_ptr) != filename_ptr)) {
		fastcov_coverage_file *file;
		if (zend_hash_find(&FASTCOV_G(covered_files), filename, strlen(filename), (void**)&file) == SUCCESS) {
			FASTCOV_G(current_file) = file;
		} else {
			FASTCOV_G(current_file) = fc_register_file(EG(active_op_array) TSRMLS_CC);
		}
		/* then store pointer to current file */
		FASTCOV_G(current_filename_ptr) = filename_ptr;
	}
}
/* }}} */

/* {{{ fc_ticks_function() */
void fc_ticks_function() {
	TSRMLS_FETCH();

	/* ignore the ticks if we are not running the coverage ( shoudln't happens ) */
	if (FASTCOV_G(running) == 0) {
		return;
	}

	fc_check_context(zend_get_executed_filename(TSRMLS_C) TSRMLS_CC);

	/* retrieve the current opcode's filename and lineno */
	ulong line = zend_get_executed_lineno(TSRMLS_C);

	if (FASTCOV_G(current_file) != NULL) {
		fastcov_coverage_file *current_file = FASTCOV_G(current_file);
		/* if the line count array wasn't allocated */
		if (current_file->allocated == 0) {
			/* alloc it */
			current_file->lines = emalloc(sizeof(long) * (current_file->line_count + 1));
			/* and zero it */
			memset(current_file->lines, 0, sizeof(long) * (current_file->line_count + 1));
			current_file->allocated = 1;
		}
		/* there are cases where the last opcode of the active array may not be the file end,
		 * for now, update the array size ( slow )
		 * TODO: find a fast and reliable way to count the lines a file without opening it */
		if (line > current_file->line_count) {
			/* redetect data from op_array */
			zend_op_array *active = EG(active_op_array);
			uint line_count = active->opcodes[(active->last - 1)].lineno;
			/* if the op array still doesn't return any useful value */
			if (line_count <= current_file->line_count) {
				/* fallback to the current line */
				line_count = line;
			}
			uint previous_count = current_file->line_count; /* used to zero new values */
			/* update file object and reallocate memory, should be fast as those arrays are pretty small */
			current_file->line_count = line_count;
			current_file->lines = erealloc(current_file->lines, sizeof(long) * (current_file->line_count + 1));
			/* amount of new lines inserted */
			uint diff = line_count - previous_count;
			/* only zero new values */
			memset(current_file->lines + previous_count + 1, 0, sizeof(long) * diff);
		}
		/* increment line counter */
		current_file->lines[line]++;
	}
}
/* }}} */

/* {{{ fc_start() */
int fc_start(TSRMLS_D) {
	if (FASTCOV_G(running) == 1) {
		return 0; /* the coverage is already started, ignore it */
	}
	/* switch to running mode */
	FASTCOV_G(running) = 1;

	/* register our tick function */
	php_add_tick_function(fc_ticks_function);

	/* init our file array */
	zend_hash_init(&FASTCOV_G(covered_files), 5, NULL, NULL, 0);

	return 1;
}
/* }}} */

/* {{{ fc_clean() */
void fc_clean(zend_bool force_output TSRMLS_DC) {
	if (FASTCOV_G(running) == 0) {
		return;
	}

	/* unregister tick function */
	php_remove_tick_function(fc_ticks_function);

	/* then remove running status */
	FASTCOV_G(running) = 0;

	if (force_output || (INI_BOOL("fastcov.auto_start") == 1) || (INI_BOOL("fastcov.auto_output") == 1)) {
		fc_output(TSRMLS_C);
	}

	/* free any used memory */
	zend_hash_apply(&FASTCOV_G(covered_files), fc_free_allocated_data TSRMLS_CC);

	/* destroy hash */
	zend_hash_destroy(&FASTCOV_G(covered_files));
}
/* }}} */

/* {{{ fc_init() */
void fc_init(TSRMLS_D) {
	/* setup global variables */
	FASTCOV_G(running) = 0;

	FASTCOV_G(current_filename_ptr) = 0;
}
/* }}} */


int fc_print_file(void *item, void *arg TSRMLS_DC) {
	fastcov_output *output = (fastcov_output*)arg;
	fastcov_coverage_file *file = (fastcov_coverage_file*)item;

	/* print file names */
	if (!output->first_file) {
		fputc(',', output->fd);
	}
	fprintf(output->fd, "\"%s\":{", file->filename);

	/* print lines */
	if (file->allocated == 1) {
		int line, first_line = 1;
		for (line = 0; line < file->line_count; line++) {
			if (file->lines[line] > 0) {
				// print a comma starting from second item
				if (!first_line) {
					fputc(',', output->fd);
				}
				fprintf(output->fd, "\"%d\":%d", line, file->lines[line]);
				first_line = 0;
			}
		}
	}

	fputc('}', output->fd);
	output->first_file = 0;

	return ZEND_HASH_APPLY_KEEP;
}

/* {{{ fc_output() */
void fc_output(TSRMLS_D) {
	if (strlen(FASTCOV_G(output_dir)) == 0) {
		/* output dir is empty */
		return;
	}
	/* build the output filename */
	char *output_dir, *output_filename;
	int len;
	PHP_MD5_CTX context;
	unsigned char digest[16];
	char md5str[33];
	char random_number[10];

	/* cache output dir */
	output_dir = FASTCOV_G(output_dir);
	len = strlen(output_dir);

	/* compute a random number */
	php_sprintf(random_number, "%ld", (long) (100000000 * php_combined_lcg(TSRMLS_C)));

	/* transform it to a md5 value */
	md5str[0] = '\0';
	PHP_MD5Init(&context);
	PHP_MD5Update(&context, (unsigned char*)random_number, 10);
	PHP_MD5Final(digest, &context);
	make_digest(md5str, digest);

	/* build the output filename */
	output_filename = emalloc(len + sizeof("/fastcov-") - 1 + sizeof(md5str));
	php_sprintf(output_filename, "%s/fastcov-%s", output_dir, md5str);
	/*php_printf("filename: %s\n", output_filename);*/

	fastcov_output output = { NULL, 1 };
	output.fd = fopen(output_filename, "w");
	if (output.fd) {
		/* generate a json file for output */
		fputc('{', output.fd);
		/* apply function for writing */
		zend_hash_apply_with_argument(&FASTCOV_G(covered_files), fc_print_file, (void*)&output TSRMLS_CC);
		/* then close the whole json block */
		fputc('}', output.fd);
		fclose(output.fd);
	} else {
		/* output an error */
		php_error(E_NOTICE, "Couldn't open code coverage output file %s", output_filename);
	}

	efree(output_filename);
}
/* }}} */

/* {{{ proto bool fastcov_start()
   Starts the code coverage */
PHP_FUNCTION(fastcov_start) {
	RETVAL_BOOL(fc_start(TSRMLS_C));
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

/* {{{ proto array fastcov_stop([bool force_output [, bool no_return ]])
   Stop the code coverage return an bi-dimensional array with the file list and call count for each line, pass
   true to force_output to the function to write a file in output_dir, the no_return boolean prevent the function
   from returning data ( useful to save memory when force_output is enabled ), in this case the function will
   return NULL */
PHP_FUNCTION(fastcov_stop) {
	zval *file_coverage;
	zend_bool force_output = 0;
	zend_bool no_return = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|bb", &force_output, &no_return) == FAILURE) {
		RETURN_FALSE;
	}

	if (FASTCOV_G(running) == 0) {
		RETURN_FALSE;
	}

	if (no_return == 0) {
		/* initialise return array */
		array_init(return_value);

		/* build our array */
		zend_hash_apply_with_argument(&FASTCOV_G(covered_files), fc_build_array_element, (void*)return_value TSRMLS_CC);
	}

	/* stop covering */
	fc_clean(force_output TSRMLS_CC);
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(fastcov) {
#ifdef ZTS
	ZEND_INIT_MODULE_GLOBALS(fastcov, NULL, NULL);
#endif

	REGISTER_INI_ENTRIES();

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(fastcov) {
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(fastcov) {
	/* we need to setup ticks very soon in the script */
	zval ticks;
	ZVAL_LONG(&ticks, 1);
	CG(declarables).ticks = ticks;

	/* setup memory */
	fc_init(TSRMLS_C);

	/* starts the code coverage if enabled in the ini file */
	if (INI_BOOL("fastcov.auto_start") == 1) {
		fc_start(TSRMLS_C);
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(fastcov) {
	/* stop covering */
	fc_clean(0 TSRMLS_CC);

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


