#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT(amundsen, 0.80, john@bergbomconsulting.se)
#AC_CONFIG_SRCDIR([alfabeta.c])
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lm':
AC_CHECK_LIB([m], [main])
AC_CHECK_LIB([pthread], [main])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h sys/time.h sys/timeb.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([ftime gettimeofday pow strchr strcspn strspn strstr strtol])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
