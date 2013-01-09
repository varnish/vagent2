#!/bin/bash

N=1
function test_vcl()
{
	lwp-request -m PUT http://localhost:6085/vcl/test3 <$VCL1 > /dev/null
	if [ "x$?" = "x0" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; echo $FOO; fi
	inc
	lwp-request -m GET http://localhost:6085/vcl/test3 >$VCL1.tmp
	if [ "x$?" = "x0" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; fi
	inc
	if diff -q $VCL1 $VCL1.tmp > /dev/null; then echo "Passed ${N}"; else echo "Failed ${N}"; fi
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
VCL1=smallvcl
test_vcl
VCL1=longvcl
test_vcl
