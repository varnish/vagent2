=================
Varnish Agent (2)
=================

``varnish-agent`` is a small daemon meant to communicate with Varnish and
other varnish-related service to allow remote control and monitoring of
Varnish.

Since it is written with the Varnish Administration Console (VAC) as the
primary user, the documentation uses the VAC as the example, even though it
is not really vac-specific.

Design
======

The varnish agent is designed to be split up in modules. Each module may
spawn a background thread but should then offer convenience function to
communicate with said thread. Some modules are special and built in. They
are:

1. varnishadm is the primary module that will be talking to Varnish. Upon
   module initialization other modules can issue ``vadmin_register()`` to
   register a connection to this module. This gives the caller a handle
   which will later be passed to ``vadmin_run()`` to issue commands. Behind
   the scene, this handle maps to a set of pipes to communicate with the
   background thread. The background thread issues all commands and parses
   the response, it also takes care of reconnection if needed.
   It is an error to issue ``vadmin_register()`` after module
   initialization.
2. ctl is the thread which listens for communication from the VAC. It's
   meant to be synchronous - any command issued over this channel should
   get a response immediately. If the operation takes time, the response
   should come in the form of an asynchronous callback instead. The ctl
   module is single threaded, but supports multiple connections.
   ctl offers a simple api for registering commands:
   ``ctl_register("command",func,data)``, where func is the callback and
   data is the private data.
3. responder is the module sending asynchronous responses back to the VAC.
   E.g.: periodical health checks, statistics, etc.

Other modules are loaded after both of these are initialized, but before
they start.

Examples of other modules are:

- pingd - pings varnish over varnishadm frequently.
- call-home - sends a request back to the vac with info
- 


