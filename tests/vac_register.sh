#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
. util.sh

init_misc
start_varnish
DUMMY_PORT=$(( 1024 + ( $RANDOM % 48000 ) ))
export DUMMY_PORT
./core.sh 2>/dev/null >/dev/null &
dummypid=$!

# test 1 & 2 - correct URL
# expected results: OK
export ARGS="-z http://localhost:${DUMMY_PORT}/"
start_agent
test_it POST vac_register "" "OK"
stop_agent > /dev/null

# test 3 & 4 - incorrect URL
# expected results: Something went wrong. Incorrect URL?
export ARGS="-z http://localhost:12345/does/not/exist"
start_agent > /dev/null
test_it_fail POST vac_register "" "Something went wrong. Incorrect URL?"
stop_agent > /dev/null

# test 5 & 6 - URL with no http:// prefix
# expected results: OK
export ARGS="-z localhost:${DUMMY_PORT}/"
start_agent > /dev/null
test_it POST vac_register "" "OK"
stop_agent > /dev/null

# test 7 & 8 - malformed URL
# expected results: Something went wrong. Incorrect URL? 
export ARGS="-z abcde"
start_agent > /dev/null
test_it_fail POST vac_register "" "Something went wrong. Incorrect URL?"
stop_agent > /dev/null

# test 9 & 10 - no url supplied
# expected results: VAC url is not supplied. Please do so with the -z argument.
export ARGS=""
start_agent > /dev/null
test_it_fail POST vac_register "" "VAC url is not supplied. Please do so with the -z argument."

kill $dummypid
