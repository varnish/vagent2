#!/bin/bash

. util.sh

init_all

test_json vcljson/
test_json paramjson/
test_json paramjson/acceptor_sleep_decay
test_json stats
test_json log/100/ReqURL

test_it_long GET vcl/ "" "active"
test_json log/100

GET http://localhost:${VARNISH_PORT}/meh > /dev/null
test_json log/100/ReqURL

exit $ret
