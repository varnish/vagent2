#!/bin/bash


N=1
ret=0

AGENT_PORT="${AGENT_PORT:-6085}"
VARNISH_PORT="${VARNISH_PORT:-80}"
TMPDIR="${TMPDIR:-$(mktemp -d)}"
SRCDIR="${SRCDIR:-"."}"
ORIGPWD="${ORIGPWD:-"."}"
VARNISH_PID="${TMPDIR}/varnish.pid"

inc() {
	N=$(( ${N} + 1 ))
}

fail() {
	echo -en "${INDENT}Failed ${N}:"
	echo " $*"
	if [ "x$KNOWN_FAIL" = "x1" ]; then
		echo "Known failure. Ignoring."
	else
		ret=$(( ${ret} + 1 ))
	fi
}

pass() {
	echo -en "${INDENT}Passed ${N} "
	echo "$*"
}

stop_varnish() {
	varnishpid="$(cat "$VARNISH_PID")"
	if [ -z "$varnishpid" ]; then
		fail "NO VARNISHPID? Bad stuff..."
	fi
	echo -e "\tStopping varnish($varnishpid)"
	kill $varnishpid
}

stop_agent() {
	agentpid=$(cat ${TMPDIR}/agent.pid)
	if [ -z $agentpid ]; then
		fail "Stopping agent but no agent pid found. BORK"
	fi
	echo -e "\tStopping agent($agentpid)"
	kill $agentpid
}

cleanup() {
    echo Stopping varnishd and the agent
    stop_varnish
    stop_agent
    echo Cleaning up
    rm -rf ${TMPDIR}
}

init_misc() {
	trap 'cleanup' EXIT
	mkdir -p ${TMPDIR}/vcl
}


pidwait() {
	I=1
	for a in $(seq 1 5); do
		pid="$(cat "${TMPDIR}/${1}.pid")"
		if [ ! -z "$pid" ]; then
			break
		fi
		sleep 0.5
		I=$(( $I + 1 ))
	done
	echo -e "\tPidwait took $I iterations"
	echo -e "\tStarted $1. Pid $pid"
	if [ -z "$pid" ]; then
		fail "No $1 pid? Bad stuff..."
		exit 1
	fi
	if kill -0 $pid; then
		echo -e "\t$1 started ok, we think."
	else
		fail "$1 not started correctly? FAIL"
		exit 1
	fi
}

start_varnish() {
	head -c 16 /dev/urandom > "$TMPDIR/secret"
	printf "${INDENT}Starting varnishd\n\n"
	varnishd -f "${SRCDIR}/data/boot.vcl" \
	    -P "$VARNISH_PID" \
	    -n "$TMPDIR" \
	    -p auto_restart=off \
	    -a 127.0.0.1:0 \
	    -T 127.0.0.1:0 \
	    -s malloc,50m \
	    -S "$TMPDIR/secret"
	pidwait varnish
	VARNISH_PORT=$(varnishadm -n "$TMPDIR" debug.listen_address | cut -d\  -f 2)
	export VARNISH_PORT AGENT_PORT
	export N_ARG="-n ${TMPDIR}"
}

start_agent() {
	printf "Starting agent:\n\n"
	AGENT_PORT=$(( 1024 + ( $RANDOM % 48000 ) ))
	echo -e "\tAgent port: $AGENT_PORT"
	echo -e "\tAgent arguments:  ${N_ARG} -p ${TMPDIR}/vcl/ -P ${TMPDIR}/agent.pid -c $AGENT_PORT ${ARGS}"
	$ORIGPWD/../src/varnish-agent ${N_ARG} -p ${TMPDIR}/vcl/ -P ${TMPDIR}/agent.pid -c "$AGENT_PORT" ${ARGS}

	pidwait agent
}

init_all() {
	init_misc
	start_varnish
	start_agent
}

test_it() {
	FOO=$(lwp-request -m $1 http://localhost:$AGENT_PORT/$2 <<<"$3")
	if [ "x$?" = "x0" ]; then pass; else fail "$*: $FOO"; fi
	inc
	if [ "x$FOO" = "x$4" ]; then pass; else fail "$*: $FOO"; fi
	inc
}

test_it_no_content() {
	FOO=$(lwp-request -m $1 http://localhost:${AGENT_PORT}/$2 </dev/null)
	if [ "x$?" = "x0" ]; then pass; else fail "$*: $FOO"; fi
	inc
	if [ "x$FOO" = "x$4" ]; then pass; else fail "$*: $FOO"; fi
	inc
}

test_it_fail() {
	FOO=$(lwp-request -m $1 http://localhost:${AGENT_PORT}/$2 <<<"$3")
	if [ "x$?" != "x0" ]; then pass; else fail "$*: $FOO"; fi
	inc
	if [ "x$FOO" = "x$4" ]; then pass; else fail "$*: $FOO"; fi
	inc
}

test_it_long() {
	FOO=$(lwp-request -m $1 http://localhost:${AGENT_PORT}/$2 <<<"$3")
	if [ "x$?" = "x0" ]; then pass; else fail "$*: $FOO"; fi
	inc
	if echo $FOO | grep -q "$4";then pass; else fail "$*: $FOO"; fi
	inc
}

test_it_long_fail() {
	FOO=$(lwp-request -m $1 http://localhost:${AGENT_PORT}/$2 <<<"$3")
	if [ "x$?" != "x0" ]; then pass; else fail "$*: $FOO"; fi
	inc
	if echo $FOO | grep -q "$4";then pass; else fail "$*: $FOO"; fi
	inc
}

test_it_long_content_fail() {
	FOO=$(lwp-request -m $1 http://localhost:${AGENT_PORT}/$2 <<<"$3")
	if [ "x$?" = "x0" ]; then pass; else fail "$*: $FOO"; fi
	inc
	if echo $FOO | grep -q "$4"; then fail "$*: $FOO"; else pass; fi
	inc
}

is_running() {
	if [ "x$NOSTATUS" = "x1" ]; then
		return
	fi
	test_it GET status "" "Child in state running"
}

test_json() {
	NAME="${TMPDIR}/jsontest$N.json"
	lwp-request -m GET http://localhost:${AGENT_PORT}/$1 > $NAME
	if [ "x$?" = "x0" ]; then pass; else fail "json failed: $1 failed"; fi
	inc
	FOO=$(jsonlint -v $NAME)
	if [ "x$?" = "x0" ]; then
	    pass
	else
	    fail "json failed: $1 failed: $FOO"
	    cat "$NAME"
	fi
	inc

}
