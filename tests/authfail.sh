#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
strace_check()
{
	! grep -q "EBADF" $TMPDIR/agent-strace
	if [ "x$?" = "x0" ]; then pass; else fail "Agent using bad filedescriptors"; fi
	inc
}
. util.sh
init_misc
echo "dummmmmmmm" > ${TMPDIR}/sec-fail
ARGS="-S ${TMPDIR}/sec-fail"
STRACE_AGENT="y"
start_agent
test_it_fail GET status "" "Varnishd disconnected"
strace_check
start_varnish
test_it_fail GET status "" "Varnishd disconnected"
strace_check
stop_agent
ARGS=""
start_agent
is_running
strace_check
exit $ret
