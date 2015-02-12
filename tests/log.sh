#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
. util.sh
init_misc
start_agent

test_it_fail GET status "" "Varnishd disconnected"
test_it_long_fail GET log "" "Error in opening shmlog"
start_varnish
is_running
test_json log
test_it_fail GET log/a "" "Not a number"
test_json log/2
GET http://localhost:${VARNISH_PORT}/foobar > /dev/null
test_it_long GET log/1/ReqURL "" "\"tag\":"
test_it_long GET log/1/ReqURL/foobar "" "/foobar"
exit $ret
