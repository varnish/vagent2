#!/bin/bash


N=1

function inc()
{
	N=$(( ${N} + 1 ))
}

function test_it()
{
	FOO=$(lwp-request -m $1 http://localhost:6085/$2 <<<"$3")
	if [ "x$?" = "x0" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; echo $FOO; fi
	inc
	if [ "x$FOO" = "x$4" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; echo $FOO; fi
	inc
}

function test_it_no_content()
{
	FOO=$(lwp-request -m $1 http://localhost:6085/$2 </dev/null)
	if [ "x$?" = "x0" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; echo $FOO; fi
	inc
	if [ "x$FOO" = "x$4" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; echo $FOO; fi
	inc
}

function test_it_fail()
{
	FOO=$(lwp-request -m $1 http://localhost:6085/$2 <<<"$3")
	if [ "x$?" != "x0" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; echo $FOO; fi
	inc
	if [ "x$FOO" = "x$4" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; echo $FOO; fi
	inc
}

function test_it_long()
{
	FOO=$(lwp-request -m $1 http://localhost:6085/$2 <<<"$3")
	if [ "x$?" = "x0" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; echo $FOO; fi
	inc
	if echo $FOO | grep -q "$4"; then echo "Passed ${N}"; else echo "Failed ${N}"; echo $FOO; fi
	inc
}

function test_it_long_content_fail()
{
	FOO=$(lwp-request -m $1 http://localhost:6085/$2 <<<"$3")
	if [ "x$?" = "x0" ]; then echo "Passed ${N}"; else echo "Failed ${N}"; echo $FOO; fi
	inc
	if echo $FOO | grep -q "$4"; then echo "Failed ${N}"; echo $FOO; else echo "Passed ${N}"; fi
	inc
}

function is_running()
{
	test_it GET status "" "Child in state running"
}

rm -r tmp
mkdir -p tmp
