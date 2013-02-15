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
exit $ret
