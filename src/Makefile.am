AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall -Werror


vagent2_CFLAGS = @VARNISHAPI_CFLAGS@ -Wall -Werror -Wextra

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
	ipc.c \
	vss.c

vagent2_LDADD = \
	@VARNISHAPI_LIBS@ \
	-lmicrohttpd \
	${PTHREAD_LIBS} ${NET_LIBS} ${LIBM}
