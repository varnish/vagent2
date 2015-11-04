#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
. util.sh
init_misc
echo "dummmmmmmm" > ${TMPDIR}/sec-fail
ARGS="-S ${TMPDIR}/sec-fail"
STRACE_AGENT="y"
start_agent
test_it_fail GET status "" "Varnishd disconnected"
test_strace EBADF
start_varnish
test_it_fail GET status "" "Varnishd disconnected"
test_strace EBADF
stop_agent
ARGS=""
start_agent
is_running
test_strace EBADF
exit $ret
