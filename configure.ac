#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_COPYRIGHT([Copyright (c) 2012-2013 Varnish Software])
AC_INIT([varnish-agent], [2.2])
AC_CONFIG_SRCDIR([src/main.c])
AM_CONFIG_HEADER(config.h)

AM_INIT_AUTOMAKE([-Wall -Werror foreign parallel-tests subdir-objects])

# Check for rst utilities
AC_ARG_WITH([rst2man],
               AS_HELP_STRING([--with-rst2man=PATH],
                              [Location of rst2man (auto)]),
               [RST2MAN="$withval"],
               [AC_CHECK_PROGS(RST2MAN, [rst2man rst2man.py], "no")
	        if test "x$RST2MAN" = "xno"; then
		   AC_MSG_WARN([rst2man not found – not building man pages])
		fi])
AM_CONDITIONAL(HAVE_RST2MAN,[test "x$RST2MAN" != "xno"])

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

AC_PROG_CC
AM_PROG_CC_C_O
AGENT_PERSIST_DIR='${localstatedir}/varnish-agent'
AC_SUBST(AGENT_PERSIST_DIR)
AGENT_HTML_DIR='${datadir}/varnish-agent/html'
AC_SUBST(AGENT_HTML_DIR)

# Checks for library functions.
AC_MSG_CHECKING([for program_invocation_short_name])
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([[#include <errno.h>]],
	                 [[const char *p = program_invocation_short_name;]])],
	[AC_DEFINE([HAVE_PROGRAM_INVOCATION_SHORT_NAME], [1],
	           [Define to 1 if you have program_invocation_short_name])
	 AC_MSG_RESULT([yes])],
	[AC_MSG_RESULT([no])])

AC_MSG_CHECKING([for __progname])
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([[]],
	                 [[extern char *__progname;
	                   const char *p = __progname;]])],
	[AC_DEFINE([HAVE___PROGNAME], [1], [Define to 1 if you have __progname])
	 AC_MSG_RESULT([yes])],
	[AC_MSG_RESULT([no])])

AC_CHECK_FUNCS([dirfd __fpurge getexecname getline sysconf])
m4_ifndef([PKG_PROG_PKG_CONFIG], [m4_fatal([pkg.m4 missing, please install pkg-config])])
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([VARNISHAPI],[varnishapi])
PKG_CHECK_MODULES([MICROHTTPD],[libmicrohttpd])
PKG_CHECK_MODULES([LIBCURL],[libcurl])

AC_CONFIG_FILES([Makefile
		 include/Makefile
                 src/Makefile
		 tests/Makefile])
AC_OUTPUT
