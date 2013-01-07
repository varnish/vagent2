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

Design
======

Keep it simple.

Everything is written as a module, and the goal is:

- Close to 0 configuration
- "Just works"
- Maintainable
- Generic


IPC
===

The agent is multi-threaded and provides a simple IPC for all plugins.

The IPC uses a socket to communicate, and a plugin has to register itself
with any other plugin during initialization (which is done in a single
thread). Please read and understand include/ipc.h for details.

Modules
=======

"Everything" is a module. See src/modules/ for a complete list, but here
are some of them:

vadmin
------

This module executes commands on the varnishadm interface. It also
auto-configures itself based on the shm-log.

logd
----

Used for logging. Use the logger(...) macro instead of ipc_run directly
(this provides function names, line numbers etc).

httpd
-----

Provides the HTTP listener, using libmicrohttpd.

Provides a register-function for other modules to register callbacks that
will be executed to handle given requests. The only actual resource
provided by the httpd plugin itself is the /-resource with some basic help.

echo
----

Test-plugin listening to /echo that reads POST and PUT-data and spits it
back at the user.

html
----

File-access for /html/. The /html/ build-directory (I know) is made
available through /html/.

vcl
---

Provides mechanisms for uploading, downloading, discarding and using VCL.

GET /vcl/ gives a list.
GET /vcl/foo gives the foo-named VCL.
POST /vcl/ uploads a new VCL with a dynamic name (e.g: timestamp).
PUT /vcl/name uploads a new VCL with a specified name.
PUT /vcldeploy/name issues vcl.use on the named VCL.
GET /vclhelp prints help.

status
------

Provides PUT /start, PUT /stop and GET /status, which should be self
explanatory.

pingd
-----

Test-plugin (so far) that polls varnishadm, looks at the data and the
discards it. Might be responsible for pushing at a later date.


HTML PoC
========

The /html/ directory (either relative to the source dir, or web service)
contains a PoC client-implementation using bootstrap.

It is intended to show-case and test most functionality if possible.
