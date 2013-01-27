#!/bin/bash 

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory";
	exit 1;
fi

. util.sh
init_all


function test_vcl()
{
	VCL="$1"
	OUT="${TMPDIR}/$(basename $VCL).tmp"
	lwp-request -m PUT http://localhost:${AGENT_PORT}/vcl/test3 <$VCL > /dev/null
	if [ "x$?" = "x0" ]; then pass; else fail "VCL step 1: $OUT"; fi
	inc
	lwp-request -m GET http://localhost:${AGENT_PORT}/vcl/test3 >$OUT
	if [ "x$?" = "x0" ]; then pass; else fail "VCL step 2: $OUT"; fi
	inc
	if diff -q $VCL $OUT > /dev/null; then pass; else fail "VCL step 3: $OUT"; fi
	inc
	FOO=$(lwp-request -m DELETE http://localhost:${AGENT_PORT}/vcl/test3 || exit 1)
	if [ "x$?" = "x0" ]; then pass; else fail "VCL step 4: $OUT"; fi
	inc
}

is_running
test_it_fail GET vcl "" "Failed"
test_it_long GET vcl/ "" "active"
test_it_long GET vcl/boot "" "backend"

test_vcl data/smallvcl
test_vcl data/longvcl
test_it_long POST vcl/ "backend foo { .host =\"kly.no\"; }" "VCL compiled."
sleep 1
test_it_long POST vcl/ "backend foo { .host =\"kly.no\"; }" "VCL compiled."


exit $ret
