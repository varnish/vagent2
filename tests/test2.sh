#!/bin/bash

N=1
if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory";
	exit 1;
fi
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
}

function inc()
{
	N=$(( ${N} + 1 ))
}

# Should fail
FOO=$(lwp-request -m GET http://localhost:6085/vcl || exit 1)
if [ "x$?" = "x1" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; fi
inc
FOO=$(lwp-request -m GET http://localhost:6085/vcl/ || exit 1)
if [ "x$?" = "x0" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; fi
inc
FOO=$(lwp-request -m GET http://localhost:6085/vcl/boot || exit 1)
if [ "x$?" = "x0" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; fi
inc
VCL1=data/smallvcl
test_vcl
VCL1=data/longvcl
test_vcl
