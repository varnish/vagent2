#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_COPYRIGHT([Copyright (c) 2012 Varnish Software])
AC_INIT([vagent2], [0.1])
AC_CONFIG_SRCDIR([config.h.in])
AM_CONFIG_HEADER(config.h)

AM_INIT_AUTOMAKE(vagent2, 0.1, [-Wall -Werror foreign])
AM_CFLAGS="-Wall -Werror"

save_LIBS="${LIBS}"
LIBS=""
AC_SEARCH_LIBS(pthread_create, [thr pthread c_r])
PTHREAD_LIBS="${LIBS}"
LIBS="${save_LIBS}"
AC_SUBST(PTHREAD_LIBS)

save_LIBS="${LIBS}"
LIBS=""
AC_CHECK_LIB(socket, socket)
AC_CHECK_LIB(nsl, getaddrinfo)
NET_LIBS="${LIBS}"
LIBS="${save_LIBS}"
AC_SUBST(NET_LIBS)

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
# Checks for libraries.

m4_ifndef([PKG_PROG_PKG_CONFIG], [m4_fatal([pkg.m4 missing, please install
pkg-config])])
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([VARNISHAPI],[varnishapi])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([limits.h stdlib.h string.h sys/param.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([strcasecmp strerror strtol])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
