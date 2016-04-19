#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
. util.sh

init_all

test_it_long GET html/index.html "" ""
test_it_code GET html/index.html "" "200"
test_it_code GET html/ "" "200"
test_it_code GET html "" "301"
FOO=$(lwp-request -USsed -C ${PASS} http://localhost:$AGENT_PORT/html)
if [ "x$?" = "x0" ]; then pass; else fail "$*: $FOO"; fi
inc
if echo -e "$FOO" | grep -q "200 OK"; then pass; else fail "Redirect failed to yield 200 OK"; fi
inc
#In centos6 the Location header is not present in lwp-request. removing this test. https://github.com/varnish/vagent2/issues/155 . Hugo Cruz. 19042016
# if echo -e "$FOO" | grep -q "Location: http://localhost:$AGENT_PORT/html/"; then pass; else fail "Location: header incorrect in redirect:" $(echo -e "$FOO" | grep Location); fi
# inc
exit $ret
