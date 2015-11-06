#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
. util.sh
init_all

test_it PUT "push/url/stats" "http://pathfinder.kly.no/" "Url stored"
is_running
test_it PUT "push/test/stats" "" "Stats pushed"
test_it PUT "push/url/stats" "http://localhost:${AGENT_PORT}/echo" "Url stored"
test_it_fail PUT "push/test/stats" "" "Stats pushing failed"
test_it PUT "push/url/stats" "http://localhost:${AGENT_PORT}/push/url/stats" "Url stored"
test_it_fail PUT "push/test/stats" "" "Stats pushing failed"
exit $ret
