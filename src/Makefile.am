AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings  -Wswitch -Wshadow -Wcast-align -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls -Wformat -Wextra -Wno-missing-field-initializers -Wno-sign-compare -fstack-protector-all


vagent2_CFLAGS = @VARNISHAPI_CFLAGS@ $(AM_CFLAGS)

bin_PROGRAMS = vagent2
vagent2_SOURCES = \
	main.c \
	plugins.c \
	ipc.c \
	vss.c \
	helpers.c \
	modules/varnishadm.c \
	modules/pingd.c \
	modules/logd.c \
	modules/httpd.c \
	modules/echo.c \
	modules/status.c \
	modules/params.c \
	modules/vcl.c \
	modules/html.c \
	modules/ban.c

vagent2_LDADD = \
	@VARNISHAPI_LIBS@ \
	-lmicrohttpd \
	${PTHREAD_LIBS} ${NET_LIBS}
