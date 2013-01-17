#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory";
	exit 1;
fi
. util.sh

test_it GET status "" "Child in state running"
test_it PUT stop "" ""
test_it GET status "" "Child in state stopped"
test_it_fail PUT stop "" "Child in state stopped"
test_it PUT start "" ""
test_it_fail PUT start "" "Child in state running"
test_it GET status "" "Child in state running"
test_it_fail GET panic "" "Child has not panicked or panic has been cleared"
test_it_fail DELETE panic "" "No panic to clear"
varnishadm "debug.panic.worker" > /dev/null 2>&1
test_it_long GET panic "" "asked for it"
test_it DELETE panic "" ""
test_it_fail GET panic "" "Child has not panicked or panic has been cleared"
test_it_fail DELETE panic "" "No panic to clear"
