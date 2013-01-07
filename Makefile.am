SUBDIRS = src include

EXTRA_DIST = autogen.sh

# FIXME: This is utter rubbish.....

install-data-local:
	$(install_sh) -d -m 0755 $(DESTDIR)$(localstatedir)/varnish-agent
	$(install_sh) -d -m 0755 $(DESTDIR)$(datadir)/varnish-agent
	$(install_sh) -d -m 0755 $(DESTDIR)$(datadir)/varnish-agent/html
	cp -a html/* ${DESTDIR}$(datadir)/varnish-agent/html/

