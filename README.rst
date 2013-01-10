=============
Varnish Agent
=============

``varnish-agent`` is a small daemon meant to communicate with Varnish and
other varnish-related service to allow remote control and monitoring of
Varnish.

In addition to many varnishadm-commands, it also persists configuration
changes to disk. Uploading VCL will both load it in varnish and store it to
disk (assuming varnish compiled the VCL OK). Deploying vcl (e.g: using it)
will make sure ``/usr/lib/varnish-agent/boot.vcl`` points to the deployed
VCL. Similar logic will be available for the parameters too, though it's
not currently implemented.

It is mainly meant for the Varnish Administration Console, but also
provides a fully functional "HTML" interface (Aka: web interface).

Installation and usage
======================

See INSTALL for generic details.

In short:

- ``./autogen.sh``
- ``./configure``
- ``make``
- ``make install``
- ``varnish-agent``

Requirements:

- Varnish 3.0.3 (might work on other 3.0-versions too) with the api dev files 
  (e.g: ``apt-get install libvarnish-dev``)
- ``libmicrohttpd``
- ``pkg-config``
- ``pthreads``

The agent will use the same -n argument as whatever Varnish it's built
against. If you do not start ``varnishd`` with a ``-n`` argument, then the
agent does not require it either - otherwise they much match. Your
``varnishd`` should be running with a ``-T`` option. Otherwise you will
miss a lot of functions.

The agent normally runs on port 6085, but this can be configured with ``-c
PORT``.

For an introduction to using the agent, visit ``http://localhost:6085/``
and/or ``http://localhost:6085/html/``.

Design
======

Keep it simple.

Everything is written as a module, and the goal is:

- Close to 0 configuration
- "Just works"
- Maintainable
- Generic
- Stateless

Hacking
=======

1. Read ``include/*.h``
2. Read ``src/main.c`` to grasp the module init stuff
3. Read some simple modules, e.g: ``src/modules/echo.c`` and
   ``src/modules/status.c``
4. Write a module.

Everything is done in threads, but we currently assume that individual
modules are a single thread (sort of). That is to say: Until further
notice, you will not have to deal with handling two requests to /foo at the
same time, as they will come in sequence. This has two consequences:

1. It's easier to write modules
2. If you freeze while handling a HTTP request, all HTTP requests freeze.

You should anticipate that at some point we might want to re-factor this to
actually be re-entrant, where possible.

Never turn off compiler warnings during development.

Using assert() as much as you can. To (miss-)quote PHK (who possibly quoted
someone else): The cost of adding assert() to your code is negative:
whatever time you spend adding it and running it is made up for ten-fold
once it reveals a bug.

Document any API change thoroughly.

If an API is causing you headache, don't be afraid to fix the API itself
instead of working around it.

Avoid printf(). Use logger() instead, as that's easy to redirect. We might
want to extend the log logic. 

While it has not been the rule thus far, try to write "unit tests" for the
REST api as much as possible, see ``tests/*.sh``.

Ensure that a GET request to / can provide enough help to explain the REST
interface itself. Some modules provide additional help urls (e.g:
``/help/param``). That is encouraged if the function of the interface isn't
obvious (e.g: no point documenting ``/stop`` and ``/stats``).

