# Initialize Autoconf and Automake
AC_INIT([libfrec], [1.0], [gabor.kovesdan@aut.bme.hu])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Determine C compiler and run necessary setup for libraries
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_AR

# Check that pkg-config is installed and check required modules afterwards
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([CHECK], [check >= 0.9.6])

# Check for required libraries
AC_CHECK_LIB([tre], [tre_regncomp])


## vv TODO CHECK WHETHER THESE ARE NECESSARY vv ##

AC_CHECK_LIB([z], [gzread], [use_gzip=yes])
AC_CHECK_LIB([bz2], [BZ2_bzRead], [use_bzip2=yes])
AC_CHECK_LIB([lzma], [lzma_stream_decoder], [use_lzma=yes])

AC_CHECK_LIB([c], [fgetln], [have_fgetln=yes])
AC_CHECK_LIB([c], [getprogname], [have_getprogname=yes])

AC_SEARCH_LIBS([floor], [m])
AC_SEARCH_LIBS([timer_create], [rt])
AC_SEARCH_LIBS([pthread_create], [pthread])

AM_CONDITIONAL(WITH_GZIP, [test "$use_gzip" = yes])
AM_CONDITIONAL(WITH_BZIP2, [test "$use_bzip2" = yes])
AM_CONDITIONAL(WITH_LZMA, [test "$use_lzma" = yes])

AM_CONDITIONAL(HAVE_FGETLN, [test "$have_fgetln" = yes])
AM_CONDITIONAL(HAVE_GETPROGNAME, [test "$have_getprogname" = yes])

## ^^ TODO CHECK WHETHER THESE ARE NECESSARY ^^ ##


# Check for deprecated regex.h header in TRE
AC_CHECK_HEADERS_ONCE([tre/regex.h])

# Set project configuration headers and Makefiles
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile bin/Makefile bin/grep/Makefile lib/Makefile lib/libfrec/Makefile tests/Makefile])

# Output configuration files
AC_OUTPUT
