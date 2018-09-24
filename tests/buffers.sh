#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
. util.sh
init_all

test_it_long_file PUT echo data/large "200"
test_it_long_file PUT echo data/1.3M "200"
exit $ret
