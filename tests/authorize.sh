#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
PASS="agent:test"
. util.sh
init_all

test_it GET status "" "Child in state running"
test_it PUT stop "" ""
test_it GET status "" "Child in state stopped"
test_it PUT start "" ""
test_it GET status "" "Child in state running"

PASS="wrong:password"
test_it_fail GET status "" "Authorize, please"
test_it_fail PUT start "" "Authorize, please"
test_it_fail GET "" "" "Authorize, please"
test_it_fail POST vcl "" "Authorize, please"
test_it_fail DELETE vcl/boot "" "Authorize, please"
exit $ret
