#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_COPYRIGHT([Copyright (c) 2012 Varnish Software])
AC_INIT([vagent2], [0.1])
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

m4_ifndef([PKG_PROG_PKG_CONFIG], [m4_fatal([pkg.m4 missing, please install pkg-config])])
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([VARNISHAPI],[varnishapi])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
