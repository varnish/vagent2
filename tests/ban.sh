#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
. util.sh
init_all

is_running
test_it_long GET ban "" ".*"
GET http://localhost:${VARNISH_PORT}/meh > /dev/null
sleep 1
test_it_long_content_fail GET ban "" "/meh"
test_it POST ban "req.url ~ /meh" ""
test_it_long GET ban "" "/meh"
GET http://localhost:${VARNISH_PORT}/meh > /dev/null
sleep 1
test_it_long_content_fail GET ban "" "/meh"
test_it_no_content POST ban/meh ""
test_it_long GET ban "" "/meh"
GET http://localhost:${VARNISH_PORT}/meh > /dev/null
sleep 1
test_it_long_content_fail GET ban "" "/meh"
exit $ret
