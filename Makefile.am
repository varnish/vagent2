SUBDIRS = src include
EXTRA_DIST = autogen.sh include html
AUTOMAKE_OPTIONS=-Wno-portability

# html/
webrootdir = $(pkgdatadir)/html
webroot_DATA = $(wildcard html/*.html html/*.js)
webrootcssdir = $(pkgdatadir)/html/bootstrap/css
webrootcss_DATA = $(wildcard html/bootstrap/css/*.css)
webrootjsdir = $(pkgdatadir)/html/bootstrap/js
webrootjs_DATA = $(wildcard html/bootstrap/js/*.js)
webrootimgdir = $(pkgdatadir)/html/bootstrap/img
webrootimg_DATA = $(wildcard html/bootstrap/img/*.png)

install-data-local:
	$(install_sh) -d -m 0755 $(DESTDIR)$(localstatedir)/varnish-agent

dist_man_MANS = varnish-agent.1
MAINTAINERCLEANFILES = $(dist_man_MANS)

varnish-agent.1: README.rst
if HAVE_RST2MAN
	${RST2MAN} README.rst $@
else
	@echo "========================================"
	@echo "You need rst2man installed to make dist"
	@echo "========================================"
	@false
endif
