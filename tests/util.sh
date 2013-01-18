#!/bin/bash


N=1
ret=0

AGENT_PORT="${AGENT_PORT:-6085}"
VARNISH_PORT="${VARNISH_PORT:-80}"

function inc()
{
	N=$(( ${N} + 1 ))
}

function fail()
{
	echo "Failed ${N}: $*"
	if [ "x$KNOWN_FAIL" = "x1" ]; then
		echo "Known failure. Ignoring.";
	else
		ret=$(( ${ret} + 1 ))
	fi
}

function pass()
{
	echo "Passed ${N} $*"
}

function test_it()
{
	FOO=$(lwp-request -m $1 http://localhost:$AGENT_PORT/$2 <<<"$3")
	if [ "x$?" = "x0" ]; then pass; else fail "$*: $FOO"; fi
	inc
	if [ "x$FOO" = "x$4" ]; then pass; else fail "$*: $FOO"; fi
	inc
}

function test_it_no_content()
{
	FOO=$(lwp-request -m $1 http://localhost:${AGENT_PORT}/$2 </dev/null)
	if [ "x$?" = "x0" ]; then pass; else fail "$*: $FOO"; fi
	inc
	if [ "x$FOO" = "x$4" ]; then pass; else fail "$*: $FOO"; fi
	inc
}

function test_it_fail()
{
	FOO=$(lwp-request -m $1 http://localhost:${AGENT_PORT}/$2 <<<"$3")
	if [ "x$?" != "x0" ]; then pass; else fail "$*: $FOO"; fi
	inc
	if [ "x$FOO" = "x$4" ]; then pass; else fail "$*: $FOO"; fi
	inc
}

function test_it_long()
{
	FOO=$(lwp-request -m $1 http://localhost:${AGENT_PORT}/$2 <<<"$3")
	if [ "x$?" = "x0" ]; then pass; else fail "$*: $FOO"; fi
	inc
	if echo $FOO | grep -q "$4";then pass; else fail "$*: $FOO"; fi
	inc
}

function test_it_long_content_fail()
{
	FOO=$(lwp-request -m $1 http://localhost:${AGENT_PORT}/$2 <<<"$3")
	if [ "x$?" = "x0" ]; then pass; else fail "$*: $FOO"; fi
	inc
	if echo $FOO | grep -q "$4"; then fail "$*: $FOO"; else pass; fi
	inc
}

function is_running()
{
	test_it GET status "" "Child in state running"
}
