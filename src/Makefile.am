AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall -Werror
varnish_dir = $(top_builddir)/../varnish-cache/

INCLUDES = -I$(varnish_dir)/include

bin_PROGRAMS = vagent2
vagent2_SOURCES = \
	main.c \
	varnishadm.c \
	$(varnish_dir)/lib/libvarnish/vas.c \
	$(varnish_dir)/lib/libvarnish/vtcp.c \
	$(varnish_dir)/lib/libvarnish/vss.c

vagent2_LDADD = \
	-lvarnishapi \
	-lvarnishcompat \
	${PTHREAD_LIBS} ${NET_LIBS} ${LIBM}
