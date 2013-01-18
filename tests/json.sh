#!/bin/bash

. util.sh

function test_json()
{
	NAME="${TMPDIR}/jsontest$N.json"
	lwp-request -m GET http://localhost:${AGENT_PORT}/$1 > $NAME
	if [ "x$?" = "x0" ]; then pass; else fail "json failed: $1 failed"; fi
	inc
	FOO=$(jsonlint -v $NAME)
	if [ "x$?" = "x0" ]; then
	    pass
	else
	    fail "json failed: $1 failed: $FOO"
	    cat "$NAME"
	fi
	inc

}

test_json vcljson/
test_json paramjson/
test_json stats
test_json log/100/RxURL

test_it_long GET vcl/ "" "active"
test_json log/100/CLI
exit $ret
