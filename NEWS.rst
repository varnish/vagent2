Version 4.1.0
=============

Released:

* Fix Varnish 4.1-compatibility (incompatible with 4.0)
* Fix syntax for /vcljson/ - 4.1.0 specific
* /paramjson/foo can now retrieve single parameters.
* Add javascript varnishstat implementation
* Add "-r" read-only mode
* Fix building of manual pages
* Fix several race conditions related to stats pushing
* Fix /push/test/stats - it now returns failure on ... failure

Version 4.0.1
=============

Released: ???

* Varnish 4 support. presumably


Version 2.2
===========

Released: 2013-02-22

* Add VAC callback module, using -z argument
* Add curl module
* Allow building with --with-rst2man (Thanks to Robson Peixoto
  <robsonpeixoto@gmail.com>)
* Log client IP for older microhttpd-versions
* Stats: Push stats on request
* Simplified plugin creation
* Add /vdirect for raw varnishadm commands
* Fix broken "file not found" handling for /html
* Handle -S option and file permissions better. Agent shouldn't need read
  permission on the secret file as long as it's started as root.
* Add basic authentication, storing username and password in
  /etc/varnish/agent_secret
* Add variable length varnishtop to frontend
* Reduce startup time drastically
* Add verbose- and quiet-mode
* Fix assert-error during errorhandling of too large requests.
* Numerous internal IPC-related fixes.

Version 2.1
===========

Released: 2013-01-31

* Fix memory leaks in vlog module
* Fix vlog json formatting
* Add privilege separation
* Varnishstat now detects varnishd restart
* Don't shait ourself if the shmlog is missing
* Re-read the shmlog on varnishadm reconnect, in case of -T change
* Fix and issue with /vcljson/ where adding anything to the url would
  trigger an assert.
* Add "site specific" javascript to the default index.html: /html/user.js
  is included and used if present.
* Logg assert()-messages to syslog
* Several stability, reliability and resilience fixes. The agent should be
  much better at handling odd setups and situations now.

Version 2.0
===========

Released: 2013-01-22

* Complete re-implementation of the Varnish Agent.
* REST-full HTTP interface instead of CLI-hijack
