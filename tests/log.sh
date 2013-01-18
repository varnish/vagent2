#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory";
	exit 1;
fi
. util.sh

is_running
test_it_long GET log "" "\"tag\":"
test_it_fail GET log/0 "" "Not a number"
test_it_long GET log/2 "" "\"tag\":"
GET http://localhost:${VARNISH_PORT}/foobar > /dev/null
test_it_long GET log/1/RxURL "" "\"tag\":"
test_it_long GET log/1/RxURL/foobar "" "/foobar"
exit $ret
