=============
varnish-agent
=============

-------------
Varnish Agent
-------------

:Manual section: 1
:Author: Kristian Lyngstøl
:Date: 2013-01-15
:Version: 2.0

SYNOPSIS
========

::

        varnish-agent [-p directory] [-H directory] [-n name] [-c port]
                      [-S file] [-T host:port] [-t timeout] [-h]
                      [-P pidfile] [-V]

DESCRIPTION
===========

The ``varnish-agent`` is a small daemon meant to communicate with Varnish
and other varnish-related service to allow remote control and monitoring of
Varnish.

It listens to port 6085 by default. Try ``http://hostname:6085/html/`` for
the HTML front-end. All arguments are optional. The Varnish Agent will read
necessary options from the shm-log.

The Varnish Agent persists VCL changes to ``/var/lib/varnish-agent/`` and
maintains ``/var/lib/varnish-agent/boot.vcl``.

OPTIONS
=======

-p directory
            Specify persistence directory. This is where VCL is stored. See
            ``varnish-agent -h`` to see the compiled in default.

-H directory
            Specify where html files are found. This directory will be
            accessible through ``/html/``. The default provides a proof of
            concept front end.

-n name     Specify the varnishd name. Should match the ``varnishd -n``
            option. Amongst other things, this name is used to construct a
            path to the SHM-log file.

-S secretfile
            Path to the shared secret file, used to authenticate with
            varnishd.

-T hostport
            Hostname and port number for the management interface of
            varnishd.

-t timeout  Timeout in seconds for talking to varnishd.

-c port     Port number to listen for incomming connections. Defaults to
            6085.

-P pidfile  Write pidfile.

-d          Run in foreground.

-V          Print version.

-h          Print help.

INSTALL
=======

See INSTALL for generic details.

In short::

        ./autogen.sh
        ./configure
        make
        make install
        varnish-agent

Requirements:

- Varnish 3.0 with the api dev files
  (e.g: ``apt-get install libvarnish-dev``)
- ``libmicrohttpd``
- ``pkg-config``
- ``pthreads``

VARNISH CONFIGURATION
=====================

The agent does not require Varnish configuration changes for most changes.
However, if you wish to boot Varnish up with the last known VCL, you may
tell Varnish to use ``/var/lib/varnish-agent/boot.vcl``. E.g by modifying
``/etc/sysconfig/varnish`` or ``/etc/default/varnish`` and changing the
``-f`` argument.

DESIGN
======

Keep it simple.

Everything is written as a module, and the goal is:

- Close to 0 configuration
- "Just works"
- Maintainable
- Generic
- Stateless

SEE ALSO
========

* varnish-cli(7)
* varnishd(1)
* varnishadm(1)
* varnishlog(1)
* varnishstat(1)
* vcl(7)

HISTORY
=======

The first generic webui for Varnish was written by Petter Knudsen of Linpro
AS in 2009. This led to the creation of the Varnish Administration Console,
built to manage multiple Varnish instances. Until 2013, the Varnish
Administration Console used a minimal wrapper around the Varnish CLI
language, requiring that the Varnish Administration Console knew the CLI
language. This wrapper was known as the Varnish-Agent version 1, written by
Martin Blix Grydeland.

Development of the Varnish Agent version 2 begun in late 2012, with the
first release in early 2013. Unlike the first version, it exposes a HTTP
REST interface instead of trying to simulate a Varnish CLI session.

BUGS
====

Trying to "use" the boot VCL will regularly cause a "VCL deployed OK but
not persisted". This is because the agent can only persist VCL if the VCL
was stored through the agent - the boot vcl was not stored through the
agent so there is no matching auto-generated VCL for it on disk.
Workaround: Don't re-use the boot VCL.

The ``vlog`` module is limited. First of all, the limit it provides only
works on unfiltered commands, and it's disregarded for tags. Secondly, the
limit is a "head"-type limit now. It will give you the FIRST log entries,
not the last matching. Additionally it only lists the content of the shmlog
from the beginning of the file running up to the "here"-marker. If
``varnishd`` just wrapped around you will get minimal amount of feedback,
while you'll get a truckload of feedback if you query the module right
before ``varnishd`` wraps around.

Oh, and also, you may want to add some firewall rules. In case you didn't
notice, there is currently 0 authorization of requests.

For more, see http://github.com/varnish/vagent2

COPYRIGHT
=========

This document is licensed under the same license as the Varnish Agent
itself. See LICENSE for details.

* Copyright 2012-2013 Varnish Software AS
