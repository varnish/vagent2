#!/bin/bash

. util.sh

init_all

test_json vcljson/
test_json paramjson/
test_json stats
test_json log/100/RxURL

test_it_long GET vcl/ "" "active"
test_json log/100/CLI

GET http://localhost:${VARNISH_PORT}/meh > /dev/null
test_json log/100/RxUrl

exit $ret
