#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
        echo "Must run tests from tests/ directory"
        exit 1
fi
. util.sh
init_all
is_running
test_json backendjson/
GET http://localhost:${VARNISH_PORT}/meh > /dev/null
test_json log/100/ReqURL
test_it_long GET backendjson/ "" ".*"
GET http://localhost:${VARNISH_PORT}/meh > /dev/null
test_it PUT /backends/default "sick" ""
exit $ret
