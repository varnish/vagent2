#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
. util.sh
init_all

test_it POST direct "status" "Child in state running"
test_it POST direct "stop" ""
test_it POST direct "status" "Child in state stopped"
test_it_fail POST direct "stop" "Child in state stopped"
test_it POST direct "start" ""
test_it POST direct "" ""
test_it_long_fail POST direct "staph" "Unknown request"
test_it_long POST direct "stop\nstart" ""
test_it POST direct "status" "Child in state stopped"
test_it_long POST direct "\t" ""
test_it_long POST direct "vcl.inline foo \"vcl 4.0;backend foo { .host = \\\"127.0.0.1\\\"; }\"" ""
test_it_long_fail POST direct "vcl.inline foo2 \"vcl 4.0;\nbackend foo { .host = \\\"127.0.0.1\\\"; }\"" "Missing '\"'"
test_it_long POST direct "vcl.inline foi__o \"vcl 4.0;backend foo { .host = \\\"127.0.0.1\\\"; }\"" ""
test_it_fail POST direct "quit" "Closing CLI connection"
test_it_fail POST direct "status" "Varnishd disconnected"
test_it POST direct "status" "Child in state stopped"
exit $ret
