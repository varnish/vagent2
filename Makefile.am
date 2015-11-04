SUBDIRS = include src tests
EXTRA_DIST = autogen.sh include html INSTALL.rst README.rst debian tests redhat
AUTOMAKE_OPTIONS=-Wno-portability -Wno-override

# html/
webrootdir = $(pkgdatadir)/html
webroot_DATA = $(wildcard html/*.html)
webrootcssdir = $(pkgdatadir)/html/css
webrootcss_DATA = $(wildcard html/css/*.css)
webrootjsdir = $(pkgdatadir)/html/js
webrootjs_DATA = $(wildcard html/js/*.js)
webrootimgdir = $(pkgdatadir)/html/img
webrootimg_DATA = $(wildcard html/img/*.png)

install-data-local:
	$(install_sh) -d -m 0755 $(DESTDIR)$(localstatedir)/varnish-agent

dist_man_MANS = varnish-agent.1
MAINTAINERCLEANFILES = $(dist_man_MANS)

varnish-agent.1: README.rst
if HAVE_RST2MAN
	awk '/^\.\. image/ { getline;getline; } {print}' README.rst | ${RST2MAN} > $@
else
	@echo "========================================"
	@echo "You need rst2man installed to make dist"
	@echo "========================================"
	@false
endif

distcleancheck_listfiles = \
  find . -type f -exec sh -c 'test -f $(srcdir)/$$1 || echo $$1' \
       sh '{}' ';'
