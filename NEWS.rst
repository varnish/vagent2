
Version 2.1
===========

Released: Unreleased.

* Fix memory leaks in vlog module
* Fix vlog json formatting
* Add privilege separation
* Varnishstat now detects varnishd restart
* Don't shait ourself if the shmlog is missing
* Re-read the shmlog on varnishadm reconnect, in case of -T change
* Fix and issue with /vcljson/ where adding anything to the url would
  trigger an assert.

Version 2.0
===========

Released: 2013-01-22

* Complete re-implementation of the Varnish Agent.
* REST-full HTTP interface instead of CLI-hijack
