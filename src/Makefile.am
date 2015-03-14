AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -g -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wwrite-strings -Wswitch -Wshadow -Wcast-align -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls -Wformat -Wextra -Wno-missing-field-initializers -Wno-sign-compare -fstack-protector-all


varnish_agent_CFLAGS = @VARNISHAPI_CFLAGS@ $(AM_CFLAGS) -DAGENT_PERSIST_DIR='"${AGENT_PERSIST_DIR}"' -DAGENT_HTML_DIR='"${AGENT_HTML_DIR}"' @MICROHTTPD_CFLAGS@ @LIBCURL_CFLAGS@

bin_PROGRAMS = varnish-agent
varnish_agent_SOURCES = \
	main.c \
	plugins.c \
	ipc.c \
	helpers.c \
	foreign/vss.c \
	foreign/vsb.c \
	foreign/pidfile.c \
	foreign/base64.c \
	modules/vadmin.c \
	modules/vping.c \
	modules/logger.c \
	modules/http.c \
	modules/echo.c \
	modules/vstatus.c \
	modules/vparams.c \
	modules/vcl.c \
	modules/html.c \
	modules/vban.c \
	modules/vstat.c \
	modules/curl.c \
	modules/vdirect.c \
	modules/vlog.c \
	modules/vac_register.c

varnish_agent_LDADD = \
	@VARNISHAPI_LIBS@ \
	@MICROHTTPD_LIBS@ \
	${PTHREAD_LIBS} ${NET_LIBS} \
	${LIBCURL_LIBS} ${LIBM}
