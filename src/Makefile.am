AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings  -Wswitch -Wshadow -Wcast-align -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls -Wformat -Wextra -Wno-missing-field-initializers -Wno-sign-compare -fstack-protector-all


vagent2_CFLAGS = @VARNISHAPI_CFLAGS@ $(AM_CFLAGS)

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
