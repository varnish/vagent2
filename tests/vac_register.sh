#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
. util.sh

init_misc
start_varnish

# test 1 & 2 - correct URL
# expected results: OK
export ARGS="-z http://localhost:8080/api/rest/cache/addCache?ip=172.16.108.128&port=6082&cliPort=6083&secret=test123&agentId=VAGENT2"
start_agent
test_it GET vac_register "" "OK"
stop_agent

# test 3 & 4 - incorrect URL
# expected results: Something went wrong. Incorrect URL?
export ARGS="-z http://localhost:12345/does/not/exist"
start_agent
test_it_fail GET vac_register "" "Something went wrong. Incorrect URL?"
stop_agent

# test 5 & 6 - URL with no http:// prefix
# expected results: OK
export ARGS="-z localhost:8080/api/rest/cache/addCache?ip=172.16.108.128&port=6082&cliPort=6083&secret=test123&agentId=VAGENT2"
start_agent
test_it GET vac_register "" "OK"
stop_agent

# test 7 & 8 - malformed URL
# expected results: Something went wrong. Incorrect URL? 
export ARGS="-z abcde"
start_agent
test_it_fail GET vac_register "" "Something went wrong. Incorrect URL?"
