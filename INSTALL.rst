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
automake, autoconf, libcurl, python-docutils for rst2man, and possibly others.

On RHEL6 you need the EPEL package libmicrohttpd-devel.

It is strongly advised to use Varnish 3.0.2 or newer, as earlier versions
may behave considerably differently.

Build instructions
------------------

For git only::

	 $ ./autogen.sh

Common::

	$ ./configure
	$ make
	# make install

NB if make fails due to missing packages, re-run autogen.sh and 
configure before trying make again.

You may also build a debian package for your system by using
dpkg-buildpackage.

Pre-built packages
------------------

Pre-built varnish-agent packages for Debian and RedHat are available 
from the varnish-cache repo (currently only for varnish-3.0). 
See https://www.varnish-cache.org/installation/debian 
or https://www.varnish-cache.org/installation/redhat for further details.

Inital configuration
--------------------

Varnish-agent requires a username and password to be provided at 
/etc/varnish/agent_secret, in the form user:password. If varnish-agent was 
installed from a package, this will be done automatically; if not you will 
need to create it yourself.

Start varnish-agent from /usr/local/bin/varnish-agent or from the init.d script.

Once running, the web interface should be available at
http://your-varnish-server:6085/html/
(including the trailing slash), using your credentials to log in. 

If there are issues at this stage, check syslog for logging, or try 
running varnish-agent in the foreground with the -vd flags.

Running
-------

See README.rst (or the manual) for what arguments the agent accepts.

The Varnish Agent normally listens on port 6085. This version of the agent
*does not* provide any authentication of incoming requests. It is meant as
a technical preview, or for internal use. Future versions will provide some
authentication.

While possible, it is not advisable to run the agent behind the Varnish
cache it is controlling, for what we hope are obvious reasons.

Integration with varnishd
-------------------------

Varnish-agent should not need any additional configuration to interact with
a varnishd instance running on the same server. The number of requests per 
second made to varnisd should automatically show in the varnish-agent web UI, 
top left under the blue 'Active VCL' box.

Integration with VAC
--------------------

Varnish-agent can also be configured to interact with the Varnish Administration
Console (VAC). 

VAC is typically installed on a separate server to varnishd/varnish-agent; 
to register the varnish-agent with VAC, the -z flag needs to be set either 
on the command line or in /etc/defaults/varnish-agent in the form
	DAEMON_OPTS="-z http://vac-server-name/api/rest/register"

If this is successful, the varnish-cache instance should be listed in the 
VAC UI under the Configure tab. To associate the cache with a cache group, 
drag and drop the instance from the list on the left-hand side to the group entry.

If the varnishd instance is receiving requests, statistics should be visible in 
the VAC UI after a short lag.

Bugs
----

All known bugs are tracked on http://github.com/varnish/vagent2/issues

See the manual page (or README.rst) for some of the more pressing bugs.

