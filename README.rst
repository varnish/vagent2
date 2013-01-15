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

-T host:port
            Hostname and port number for the management interface of
            varnishd.

-t timeout  Timeout in seconds for talking to varnishd.

-c port     Port number to listen for incomming connections. Defaults to
            6085.

-d          Run in foreground.

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

- Varnish 3.0 (might work on other 3.0-versions too) with the api dev files 
  (e.g: ``apt-get install libvarnish-dev``)
- ``libmicrohttpd``
- ``pkg-config``
- ``pthreads``

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

COPYRIGHT
=========

This document is licensed under the same license as the Varnish Agent
itself. See LICENSE for details.

* Copyright 2012-2013 Varnish Software AS
