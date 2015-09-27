#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
. util.sh

ARGS="-r"
init_all

test_it GET echo "" ""
test_it_no_content_fail POST echo "Foobar" "Read-only mode"
test_it_no_content_fail POST echo "" "Read-only mode"
exit $ret
