AC_INIT([libfrec], [1.0], [gabor.kovesdan@aut.bme.hu])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Werror])

AM_PROG_AR
AC_PROG_CC
AC_PROG_RANLIB

PKG_CHECK_MODULES([CHECK], [check >= 0.9.6])
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h])
AC_FUNC_MALLOC

AC_CHECK_HEADERS([tre/regex.h])

AC_CHECK_LIB([tre], [tre_regncomp], [TRELIB=-ltre])

AC_CHECK_LIB([z], [gzread], [use_gzip=yes])
AC_CHECK_LIB([bz2], [BZ2_bzRead], [use_bzip2=yes])
AC_CHECK_LIB([lzma], [lzma_stream_decoder], [use_lzma=yes])

AC_CHECK_LIB([c], [fgetln], [have_fgetln=yes])
AC_CHECK_LIB([c], [getprogname], [have_getprogname=yes])

AM_CONDITIONAL(WITH_GZIP, [test "$use_gzip" = yes])
AM_CONDITIONAL(WITH_BZIP2, [test "$use_bzip2" = yes])
AM_CONDITIONAL(WITH_LZMA, [test "$use_lzma" = yes])

AM_CONDITIONAL(HAVE_FGETLN, [test "have_fgetln" = yes])
AM_CONDITIONAL(HAVE_GETPROGNAME, [test "have_getprogname" = yes])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile bin/Makefile bin/grep/Makefile lib/Makefile lib/libfrec/Makefile tests/Makefile])
AC_OUTPUT
