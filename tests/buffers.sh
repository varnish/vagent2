#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
. util.sh
init_all

OUT=$(lwp-request -m PUT http://${PASS}@localhost:${AGENT_PORT}/echo/ < data/large 2>&1)
if [ $? -eq "1" ]; then pass; else fail; fi
inc
echo $OUT | grep -q "500 Server closed connection without sending any data back" && pass || fail

exit $ret
