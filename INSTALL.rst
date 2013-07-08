Varnish Agent installation instructions
=======================================

Build requirements
------------------

* Varnish 3.0 (Preferred: 3.0.3 or newer). The varnishapi development
  files must be available. E.g: apt-get install libvarnishapi-dev
* libmicrohttpd (apt-get install libmicrohttpd-dev)
* pkg-config
* Common build environment (C compiler, make, etc).

If you are building from the git repository, not a tar-ball, you will need
automake, autoconf and others.

It is strongly advised to use Varnish 3.0.2 or newer, as earlier versions
may behave considerably different.

Build instructions
------------------

For git only::

	 $ ./autogen.sh

Common::

	$ ./configure
	$ make
	# make install

You may also build a debian package for your system by using
dpkg-buildpackage.

On RHEL6 you need the EPEL package libmicrohttpd-devel.

Running
-------

The agent does not require any arguments to run under normal conditions. If
your ``varnishd`` is using a ``-n`` argument, the agent will need the same
``-n`` argument.

The Debian packaging provides an init script for convenience.

The Varnish Agent normally listens on port 6085. This version of the agent
*does not* provide any authentication of incoming requests. It is meant as
a technical preview, or for internal use. Future versions will provide some
authentication.

While possible, it is not advisable to run the agent behind the Varnish
cache it is controlling, for what we hope are obvious reasons.

Bugs
----

All known bugs are tracked on http://github.com/varnish/vagent2/issues

See the manual page (or README.rst) for some of the more pressing bugs.

