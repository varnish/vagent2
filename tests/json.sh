#!/bin/bash

. util.sh

function test_json()
{
	NAME="tmp/jsontest$N.json"
	lwp-request -m GET http://localhost:6085/$1 > $NAME
	if [ "x$?" = "x0" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; echo "$1 failed"; fi
	inc
	FOO=$(jsonlint $NAME)
	if [ "x$?" = "x0" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; echo "$1 failed: $FOO"; fi
	inc

}

test_json vcljson/
test_json paramjson/
test_json stats
test_json log
