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
./core.sh &
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
stop_agent > /dev/null

# test 11, 12, 13 & 14 - url update.
# expected results: 
export ARGS=""
start_agent > /dev/null
test_it_fail POST vac_register "" "VAC url is not supplied. Please do so with the -z argument."
test_it POST vac_register "http://localhost:${DUMMY_PORT}/" "OK"
stop_agent > /dev/null

# test 15 - github issue #108
# https://github.com/varnish/vagent2/issues/108 
# expected results: PUT request back to vac
#
# setting up the test 
VAC_PORT=$(( 1024 + ( $DUMMY_PORT % 48000 ) ))
RESULTFILE="$TMPDIR/result"
echo dummy port = $DUMMY_PORT
echo VAC port = $VAC_PORT
echo result file = $RESULTFILE
nc -l $VAC_PORT > $RESULTFILE &
sleep 1
# ok run the test
export ARGS="-z http://localhost:${VAC_PORT}/"
start_agent
sleep 2
cat $RESULTFILE
stop_agent > /dev/null
# check results
result=0
result=$(grep "PUT" $RESULTFILE | wc -l)
if [[ $result -eq 1 ]] 
    then
        pass;
    else
        fail "expected PUT method";
fi
inc

kill $dummypid
