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

#define FASTCOV_VERSION "0.4"

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
void fc_execute(zend_op_array *op_array TSRMLS_DC);
void fc_ticks_function();
void fc_check_context(char *filename TSRMLS_DC);
int fc_start(TSRMLS_D);
void fc_clean(zend_bool TSRMLS_DC);
void fc_init(TSRMLS_D);
void fc_output(TSRMLS_D);
/* }}} */

coverage_file *fc_register_file(zend_op_array *op_array TSRMLS_DC) {
	/* let the original compiler do it's work */
	zend_op_array* ret = op_array;

	char *filename = ret->filename;
	uint line_count = ret->opcodes[(ret->last - 1)].lineno;

	/* left here for debugging purposes */
	/*php_printf("<pre>Registering %s (lines: %d, op_array_ptr: %p, filename_ptr: %p)</pre>\n",
		filename, line_count, ret, filename);*/

	/* setup a basic coverage_file structure but do not allocate line counter */
	coverage_file *tmp = emalloc(sizeof(coverage_file));
	/* we can't trust the pointer to our filename, duplicate it */
	tmp->filename = estrdup(filename);
	tmp->filename_ptr = (intptr_t)filename;
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

	return tmp;
}

void fc_check_context(char *filename TSRMLS_DC) {
	intptr_t filename_ptr = (intptr_t)filename;
	/* if the current opcode is on a different file than our current_filename_ptr pointer, switch to it.
	/* we don't use strcmp there in high_compatibility as it would reduce performances on large section of
	 * procedural code where the op_array stay the same */
	if ((FASTCOV_G(current_filename_ptr) == 0) || (FASTCOV_G(current_filename_ptr) != filename_ptr)) {
		coverage_file *file = FASTCOV_G(first_file);
		int found = 0;
		while (file != NULL) {
			/* if filename pointers could be wrong, switch back to strcmp */
			if (FASTCOV_G(high_compatibility) == 1) {
				if (strcmp(file->filename, filename) == 0) {
					FASTCOV_G(current_file) = file;
					found = 1;
					break;
				}
			} else {
				if (file->filename_ptr == filename_ptr) {
					FASTCOV_G(current_file) = file;
					found = 1;
					break;
				}
			}
			file = file->next;
		}
		/* file not found, register it */
		if (found == 0) {
			FASTCOV_G(current_file) = fc_register_file(EG(active_op_array));
		}
		/* then store pointer to current file */
		FASTCOV_G(current_filename_ptr) = filename_ptr;
	}
}

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
int fc_start(TSRMLS_D) {
	if (FASTCOV_G(running) == 1) {
		return 0; /* the coverage is already started, ignore it */
	}
	/* switch to running mode */
	FASTCOV_G(running) = 1;

	/* register our tick function */
	php_add_tick_function(fc_ticks_function);

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
	
	FASTCOV_G(current_filename_ptr) = 0;
}
/* }}} */

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

	FILE *output_file = fopen(output_filename, "w");
	if (output_file) {
		/* generate a json file for output */
		fputc('{', output_file);
		/* retrieve first file */
		coverage_file *file = FASTCOV_G(first_file);
		int file_no = 0;
		while (file != NULL) {
			/* add commas between each files */
			if (file_no > 0) {
				fputc(',', output_file);
			}
			/* write the filename and open the line count array */
			fprintf(output_file, "\"%s\":{", file->filename);
			
			/* check if the file was called even once */
			if (file->allocated == 1) {
				/* then iterate on each line */
				ulong i;
				int line_no = 0;
				for (i = 0; i < file->line_count; i++) {
					if (file->lines[i] > 0) {
						/* add comma between each lines */
						if (line_no) {
							fputc(',', output_file);
						}
						/* and output the line number */
						fprintf(output_file, "\"%d\":%d", i, file->lines[i]);
						line_no++;
					}
				}
			}

			/* close the line count array */
			fputc('}', output_file);
			file_no++;
			file = file->next;
		}
		/* then close the whole json block */
		fputc('}', output_file);
		fclose(output_file);
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

	/* stop covering */
	fc_clean(force_output TSRMLS_CC);

	if (no_return == 0) {
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
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(fastcov) {
#ifdef ZTS
	ZEND_INIT_MODULE_GLOBALS(fastcov, NULL, NULL);
#endif

	FASTCOV_G(high_compatibility) = 0;
	/* when apc loads opcode_arrays from cache, it uses one filename pointer per
	 * opcode array, preventing us to use this pointer to find files, we must then
	 * switch to high_compatibility mode and use strcmp for filename comparisons,
	 * making us actually slower */
	if (zend_hash_exists(&module_registry, "apc", sizeof("apc"))) {
		FASTCOV_G(high_compatibility) = 1;
	}

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

	/* free any used memory */
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


