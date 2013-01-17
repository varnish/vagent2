#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory";
	exit 1;
fi

. util.sh

function test_vcl()
{
	OUT=tmp/$(basename $VCL1).tmp
	lwp-request -m PUT http://localhost:6085/vcl/test3 <$VCL1 > /dev/null
	if [ "x$?" = "x0" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; echo $FOO; fi
	inc
	lwp-request -m GET http://localhost:6085/vcl/test3 >$OUT
	if [ "x$?" = "x0" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; fi
	inc
	if diff -q $VCL1 $OUT > /dev/null; then echo "Passed ${N}"; else echo "Failed ${N}"; fi
	inc
	FOO=$(lwp-request -m DELETE http://localhost:6085/vcl/test3 || exit 1)
	if [ "x$?" = "x0" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; fi
	inc
}

is_running
test_it_fail GET vcl "" "Failed"
test_it_long GET vcl/ "" "active"
test_it_long GET vcl/boot "" "backend"

VCL1=data/smallvcl
test_vcl
VCL1=data/longvcl
test_vcl
test_it POST vcl/ "backend foo { .host =\"kly.no\"; }" "VCL compiled."
sleep 1
test_it POST vcl/ "backend foo { .host =\"kly.no\"; }" "VCL compiled."

