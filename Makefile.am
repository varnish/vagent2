SUBDIRS = src

EXTRA_DIST = autogen.sh

# FIXME: This is utter rubbish.....

install-data-local:
	$(install_sh) -d -m 0755 $(DESTDIR)$(localstatedir)/varnish-agent
	$(install_sh) -d -m 0755 $(DESTDIR)$(datadir)/varnish-agent
	$(install_sh) -d -m 0755 $(DESTDIR)$(datadir)/varnish-agent/html
	$(install_sh) html/index.html $(DESTDIR)$(datadir)/varnish-agent/html/
	$(install_sh) -d -m 0755  $(DESTDIR)$(datadir)/varnish-agent/html/bootstrap
	$(install_sh) -d -m 0755  $(DESTDIR)$(datadir)/varnish-agent/html/bootstrap/css
	$(install_sh) -d -m 0755  $(DESTDIR)$(datadir)/varnish-agent/html/bootstrap/img
	$(install_sh) -d -m 0755  $(DESTDIR)$(datadir)/varnish-agent/html/bootstrap/js
	$(install_sh) html/bootstrap/css/bootstrap.css $(DESTDIR)$(datadir)/varnish-agent/html/bootstrap/css/
	$(install_sh) html/bootstrap/css/bootstrap-responsive.css $(DESTDIR)$(datadir)/varnish-agent/html/bootstrap/css/
	$(install_sh) html/bootstrap/css/bootstrap.min.css $(DESTDIR)$(datadir)/varnish-agent/html/bootstrap/css/
	$(install_sh) html/bootstrap/css/bootstrap-responsive.min.css $(DESTDIR)$(datadir)/varnish-agent/html/bootstrap/css/
	$(install_sh) html/bootstrap/img/glyphicons-halflings.png $(DESTDIR)$(datadir)/varnish-agent/html/bootstrap/img/
	$(install_sh) html/bootstrap/img/glyphicons-halflings-white.png $(DESTDIR)$(datadir)/varnish-agent/html/bootstrap/img/
	$(install_sh) html/bootstrap/js/bootstrap.js $(DESTDIR)$(datadir)/varnish-agent/html/bootstrap/js/
	$(install_sh) html/jquery.js $(DESTDIR)$(datadir)/varnish-agent/html/jquery.js

