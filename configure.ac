#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_COPYRIGHT([Copyright (c) 2012 Varnish Software])
AC_INIT([varnish-agent], [0.1])
AC_CONFIG_SRCDIR([src/main.c])
AM_CONFIG_HEADER(config.h)

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

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
AGENT_PERSIST_DIR='${localstatedir}/varnish-agent'
AC_SUBST(AGENT_PERSIST_DIR)
AGENT_HTML_DIR='${datadir}/varnish-agent/html'
AC_SUBST(AGENT_HTML_DIR)
m4_ifndef([PKG_PROG_PKG_CONFIG], [m4_fatal([pkg.m4 missing, please install pkg-config])])
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([VARNISHAPI],[varnishapi])

AC_CONFIG_FILES([Makefile
		 include/Makefile
                 src/Makefile])
AC_OUTPUT
