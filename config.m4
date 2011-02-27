
PHP_ARG_ENABLE(fastcov, whether to enable fastcov support,
[ --enable-fastcov      Enable fastcov support])

if test "$PHP_FASTCOV" != "no"; then
  PHP_NEW_EXTENSION(fastcov, fastcov.c, $ext_shared)
fi
