
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
