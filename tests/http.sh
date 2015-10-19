#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
. util.sh

init_all

test_it_long GET html/index.html "" ""
test_it_code GET html/index.html "" "200"
test_it_code GET html/ "" "200"
test_it_code GET html "" "301"
#test_it POST echo "Foobar" "Foobar"
#test_it POST echo "" ""
exit $ret
