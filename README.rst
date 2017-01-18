=============
varnish-agent
=============

.. image:: https://travis-ci.org/varnish/vagent2.svg?branch=master
    :target: https://travis-ci.org/varnish/vagent2

-------------
Varnish Agent
-------------

:Manual section: 1
:Authors: Kristian Lyngstøl, Yves Hwang, Dag Haavi Finstad
:Date: 04-11-2015
:Version: 4.1.0

SYNOPSIS
========

::

        varnish-agent [-C cafile] [-c port] [-d] [-g group] [-H directory]
                      [-h] [-k allow-insecure-vac][-K agent-secret-file] [-n name] [-P pidfile]
                      [-p directory] [-q] [-r] [-S varnishd-secret-file]
                      [-T host:port] [-t timeout] [-u user] [-V] [-v]
                      [-z vac_register_url]

DESCRIPTION
===========

The ``varnish-agent`` is a small daemon meant to communicate with Varnish
and other varnish-related services to allow remote control and monitoring
of Varnish.

It listens to port 6085 by default. Try ``http://hostname:6085/html/`` for
the HTML front-end. All arguments are optional.  The Varnish Agent will
read all the necessary options from the shm-log, with the exception of the
username and password, which is read from the -K option or the default
value.

For default values of options, including but not limited to where username
and password is read from (``-K``), where VCL is saved to (``-p``) and
where HTML is read from (``-H``), see ``varnish-agent -h``.

OPTIONS
=======

-C cafile   CA certificate for use by the cURL module. For use when
            the VAC register URL is specified as https using a
            certificate that can not be validated with the
            certificates in the system's default certificate
            directory.

-c port     Port number to listen for incoming connections. Defaults to
            6085.

-d          Run in foreground.

-g group    Group to run as. Defaults to ``varnish``.

-H directory
            Specify where html files are found. This directory will be
            accessible through ``/html/``. The default provides a proof of
            concept front end.

-h          Print help.

-k allow-insecure-vac
            This option explicitly allows curl to perform 'insecure' SSL
            connections and transfers.

-K agent-secret-file
            Path to a file containing a single line representing the
            username and password required to authenticate. It should
            have a format of ``username:password``.

-n name     Specify the varnish name. Should match the ``varnishd -n``
            option. Amongst other things, this name is used to construct a
            path to the SHM-log file.

-P pidfile  Write pidfile.

-p directory
            Specify persistence directory. This is where VCL is stored.

-q          Quiet mode. Only log/output warnings/errors.

-r          Read-only mode. Only accept GET, HEAD and OPTIONS request
            methods.

-S varnishd-secret-file
            Path to the shared secret file, used to authenticate with
            varnish.

-T hostport
            Hostname and port number for the management interface of
            varnish.

-t timeout  Timeout in seconds for talking to ``varnishd``.

-u user     User to run as. Defaults to ``varnish``.

-V          Print version.

-v          Verbose mode. Be extra chatty, including all CLI chatter.

-z vac_register_url
            Specify the callback vac register url.

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

* varnishadm(1)
* varnishd(1)
* varnishlog(1)
* varnishstat(1)
* varnish-cli(7)
* vcl(7)

HISTORY
=======

The first generic WebUI for Varnish was written by Petter Knudsen of Linpro
AS in 2009. This led to the creation of the Varnish Administration Console,
built to manage multiple Varnish instances. Until 2013, the Varnish
Administration Console used a minimal wrapper around the Varnish CLI
language, requiring that the Varnish Administration Console knew the CLI
language. This wrapper was known as the Varnish Agent version 1, written by
Martin Blix Grydeland.

Development of the Varnish Agent version 2 begun in late 2012, with the
first release in early 2013. Unlike the first version, it exposes a HTTP
REST interface instead of trying to simulate a Varnish CLI session.

BUGS
====

The agent is multi-threaded, but the HTTP listener is not. As such, the
agent is vulnerable to DOS by any slow client. This should not be a problem
if you are using it internally, and if you are exposing it to the public,
consider sticking it behind Varnish itself (and consider read-only mode
with ``-r``).

Trying to "use" the boot VCL will regularly cause a "VCL deployed OK but
not persisted". This is because the agent can only persist VCL if the VCL
was stored through the agent - the boot vcl was not stored through the
agent so there is no matching auto-generated VCL for it on disk.
Workaround: Don't re-use the boot VCL.

The ``vlog`` module is limited and the filter largely broken after the
Varnish 4.0 API changes.

You may also want to add some SSL on top of it. The agent provides
HTTP Basic authentication, but that is in no way secure as credentials
are easy to extract to anyone listening in.

For more, see http://github.com/varnish/vagent2

COPYRIGHT
=========

This document is licensed under the same license as the Varnish Agent
itself. See LICENSE for details.

* Copyright 2012-2015 Varnish Software Group
