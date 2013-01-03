AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall -Werror
varnish_dir = $(top_builddir)/../varnish-cache/


vagent2_CFLAGS = @VARNISHAPI_CFLAGS@

bin_PROGRAMS = vagent2
vagent2_SOURCES = \
	main.c \
	varnishadm.c \
	pingd.c \
	plugins.c \
	logd.c \
	httpd.c \
	echo.c \
	status.c \
	params.c \
	vcl.c \
	html.c \
	ipc.c

vagent2_LDADD = \
	@VARNISHAPI_LIBS@ \
	-lmicrohttpd \
	${PTHREAD_LIBS} ${NET_LIBS} ${LIBM}
