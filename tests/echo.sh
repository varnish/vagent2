#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
. util.sh

init_all

test_it GET echo "" ""
test_it POST echo "Foobar" "Foobar"
test_it POST echo "" ""
exit $ret
