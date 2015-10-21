#!/bin/bash


N=1
ret=0

AGENT_PORT="${AGENT_PORT:-6085}"
VARNISH_PORT="${VARNISH_PORT:-80}"
TMPDIR="${TMPDIR:-$(mktemp -d -t vagent.XXXXXXXX)}"
SRCDIR="${SRCDIR:-"."}"
ORIGPWD="${ORIGPWD:-"."}"
VARNISH_PID="${TMPDIR}/varnish.pid"
BACKEND_PID="${TMPDIR}/backend.pid"
BACKEND_LOG="${TMPDIR}/backend.log"
AGENT_STDOUT="${TMPDIR}/agent.log"
PASS="${PASS:-agent:test}"
DEBUG_TEST_START=$(date +%s)
inc() {
    N=$(( ${N} + 1 ))
}

fail() {
    echo -en "${INDENT}Test ${N}: \033[31mFailed\033[0m: "
    echo " $*"
    if [ "x$KNOWN_FAIL" = "x1" ]; then
        echo "Known failure. Ignoring."
    else
        ret=$(( ${ret} + 1 ))
    fi
}

pass() {
    echo -en "${INDENT}Test ${N}: \033[32mOK\033[0m"
    echo "$*"
}

debug_out() {
	if [ ! -z "$DEBUG_TESTS" ]; then
		echo "$*" >&2
	fi
}

time_it() {
	time $*
	debug_out "Timed: $*"
	debug_out "-------"
}

stop_backend() {
    backendpid="$(cat "$BACKEND_PID")"
    if [ -z "$backendpid" ]; then
        echo "NO BACKEND_PID? Damn..."
    else
        echo -ne " backend($backendpid)"
        kill $backendpid >/dev/null 2>&1
        pidwaitinverse $backendpid
    fi
}

stop_varnish() {
    varnishpid="$(cat "$VARNISH_PID")"
    if [ -z "$varnishpid" ]; then
        echo "NO VARNISH_PID? Bad stuff..."
    else
        echo -ne " varnish($varnishpid)"
        kill $varnishpid
        time_it pidwaitinverse $varnishpid
    fi
}

stop_agent() {
    agentpid=$(cat ${TMPDIR}/agent.pid)
    if [ -z "$agentpid" ]; then
        echo "Stopping agent but no agent pid found. BORK"
    else
        echo -ne " agent($agentpid)"
        kill $agentpid
        pidwaitinverse $agentpid
    fi
}

cleanup() {
    echo -n "Stopping: "
    time_it stop_agent
    time_it stop_varnish
    time_it stop_backend
    echo
    echo Cleaning up
    rm -rf ${TMPDIR}
    if [ ! -z "$DEBUG_TESTS" ]; then
	    echo "Timing: Tests took total: $(( $(date +%s) - $DEBUG_TEST_START )) seconds" >&2
    fi
}

init_password() {
    echo $PASS > ${TMPDIR}/agent-secret
}

init_misc() {
    if [ ! -f "$(which lwp-request)" ]; then
        echo "lwp-request not found in PATH, is libwww-perl installed?"
        exit 1
    fi

    if [ ! -f "$(which jsonlint)" ]; then
        echo "jsonlint not found in PATH, is python-demjson installed?"
        exit 1
    fi

    trap 'cleanup' EXIT
    mkdir -p ${TMPDIR}/vcl
    cp ${SRCDIR}/data/boot.vcl ${TMPDIR}/boot.vcl
    mkdir -p ${TMPDIR}/html
    echo "Rosebud" > ${TMPDIR}/html/index.html
    echo "vcl 4.0; backend default { .host = \"localhost:$backendport\"; }" >$TMPDIR/boot.vcl
    chmod -R 777 ${TMPDIR}
    init_password
}

pidwait() {
    I=1
    for a in x x x x x; do
        pid="$(cat "${TMPDIR}/${1}.pid")"
        if [ -n "$pid" ]; then
            # pid is established, check port
            if [ -n "$2" ]; then
                # wait for the port also
                port_check="$(netstat -nlpt 2>/dev/null | awk '{print $4;}' | egrep ":${2}$" | wc -l)";
                if [ $port_check -eq 1 ]; then
                    break
                fi
            else
                break
            fi
        fi
        sleep 0.5
        I=$(( $I + 1 ))
    done
    debug_out "Pidwait for ${1} took $I iterations"
    echo -e " Started $1. Pid $pid"
    if [ -z "$pid" ]; then
        fail "No $1 pid? Bad stuff..."
        exit 1
    fi
    if ! kill -0 $pid; then
        fail "$1 not started correctly? FAIL"
        exit 1
    fi
}

pidwaitinverse() {
    if [ -z "$1" ]; then
        echo "pidwaitinverse failed because there is no pid";
        return
    fi
    I=1
    for a in {1..20}; do
        if ! kill -0 $1 >/dev/null 2>&1 ; then
            break
        fi
        sleep 0.1;
        I=$(( $I + 1 ))
    done
    debug_out "Pidwaitinverse for ${1} took $I iterations"
}

start_backend() {
       echo -n "Starting backend: "
       python -u empty_response_backend.py 0 >$BACKEND_LOG 2>&1 &
       backendpid=$(jobs -p %+)
       disown %+
       echo $backendpid >$BACKEND_PID
       echo -n "pid $backendpid. "
       for i in x x x x x x x x x x; do
              sleep 0.2
              backendport=$(grep -F 'Serving HTTP' $BACKEND_LOG | awk '{print $6}')
              [ -n "$backendport" ] && break
       done
       if [ -z "$backendport" ]; then
               echo -e "\tWarning: python backend failed to bind in a timely fashion."
       fi
       echo -e " port $backendport."
}

start_varnish() {
	head -c 16 /dev/urandom > "$TMPDIR/secret"
	printf "${INDENT}Starting varnishd: "
	varnishd -f "${TMPDIR}/boot.vcl" \
	    -P "$VARNISH_PID" \
	    -n "$TMPDIR" \
	    -p auto_restart=off \
	    -p ban_lurker_sleep=10 \
	    -a 127.0.0.1:0 \
	    -T 127.0.0.1:0 \
	    -s malloc,50m \
	    -S "$TMPDIR/secret" 

	FOO=""
	for i in x x x x x x x x x x; do
	    FOO=$(varnishadm -n "$TMPDIR" status 2>/dev/null)
	    if echo $FOO | grep -q "running"; then
		break
	    fi
	    sleep 0.1
	done

	if ! echo $FOO | grep -q "running"; then
	    echo "Unable to start Varnish."
	    exit 1
	fi

	pidwait varnish
	VARNISH_PORT=$(varnishadm -n "$TMPDIR" debug.listen_address | cut -d\  -f 2)
	export VARNISH_PORT AGENT_PORT
	export N_ARG="-n ${TMPDIR}"
}

start_agent() {
    AGENT_PORT=$(( 1024 + ( $RANDOM % 48000 ) ))
    printf "Starting agent on port $AGENT_PORT: "
    ARGS="$ARGS
	-K ${TMPDIR}/agent-secret
	-n ${TMPDIR}
	-p ${TMPDIR}/vcl/
	-H ${TMPDIR}/html/
	-P ${TMPDIR}/agent.pid
	-c $AGENT_PORT"
    echo -e "$ARGS" > $TMPDIR/agent-arguments
    debug_out "Agent args: $ARGS"
    $ORIGPWD/../src/varnish-agent ${ARGS} >$AGENT_STDOUT
    pidwait agent $AGENT_PORT
}

init_all() {
    if [ -z "$(which varnishd)" ]; then
            echo 1>&2 "No varnishd binary found";
            exit 1
    fi
    echo "Temp directory: $TMPDIR"
    echo "Path to varnishd: $(which varnishd)"
    time_it init_misc
    time_it start_backend
    time_it start_varnish
    time_it start_agent
}

test_it() {
    FOO=$(lwp-request -m $1 http://${PASS}@localhost:$AGENT_PORT/$2 <<<"$3")
    if [ "x$?" = "x0" ]; then pass; else fail "$*: $FOO"; fi
    inc
    if [ "x$FOO" = "x$4" ]; then pass; else fail "$*: $FOO"; fi
    inc
}

test_it_code() {
    FOO=$(lwp-request -Ssd -C ${PASS} -m $1 http://${PASS}@localhost:$AGENT_PORT/$2 <<<"$3")
    if [ "x$?" = "x0" ]; then pass; else fail "$*: $FOO"; fi
    inc
    CODE=$(echo -e "$FOO" | grep -v "^$1" | head -n1 | cut -f1 -d' ')
    if [ "x$CODE" = "x$4" ]; then pass; else fail "$*: $FOO"; fi
    inc
}

test_it_no_content() {
    FOO=$(lwp-request -f -m $1 http://${PASS}@localhost:${AGENT_PORT}/$2 </dev/null)
    if [ "x$?" = "x0" ]; then pass; else fail "$*: $FOO"; fi
    inc
    if [ "x$FOO" = "x$4" ]; then pass; else fail "$*: $FOO"; fi
    inc
}

test_it_no_content_fail() {
    FOO=$(lwp-request -f -m $1 http://${PASS}@localhost:${AGENT_PORT}/$2 )
    if [ "x$?" != "x0" ]; then pass; else fail "$*: $FOO"; fi
    inc
    if [ "x$FOO" = "x$4" ]; then pass; else fail "$*: $FOO"; fi
    inc
}

test_it_fail() {
    FOO=$(lwp-request -m $1 http://${PASS}@localhost:${AGENT_PORT}/$2 <<<"$3")
    if [ "x$?" != "x0" ]; then pass; else fail "$*: $FOO"; fi
    inc
    if [ "x$FOO" = "x$4" ]; then pass; else fail "$*: $FOO"; fi
    inc
}

test_it_first_line_fail() {
    FOO=$( (lwp-request -m $1 http://${PASS}@localhost:${AGENT_PORT}/$2 <<<"$3"; rc=$?) | head -1)
    if [ "x$rc" != "x0" ]; then pass; else fail "$*: $FOO"; fi
    inc
    if [ "x$FOO" = "x$4" ]; then pass; else fail "$*: $FOO"; fi
    inc
}

test_it_long() {
    FOO=$(lwp-request -m $1 http://${PASS}@localhost:${AGENT_PORT}/$2 <<<"$3")
    if [ "x$?" = "x0" ]; then pass; else fail "$*: $FOO"; fi
    inc
    if echo $FOO | grep -q "$4";then pass; else fail "$*: $FOO"; fi
    inc
}

test_it_long_fail() {
    FOO=$(lwp-request -m $1 http://${PASS}@localhost:${AGENT_PORT}/$2 <<<"$3")
    if [ "x$?" != "x0" ]; then pass; else fail "$*: $FOO"; fi
    inc
    if echo $FOO | grep -q "$4";then pass; else fail "$*: $FOO"; fi
    inc
}

test_it_long_file() {
    FOO=$(lwp-request -m $1 http://${PASS}@localhost:${AGENT_PORT}/$2 <"$3")
    if [ "x$?" = "x0" ]; then pass; else fail "$*: $FOO"; fi
    inc
    if echo $FOO | grep -q "$4";then pass; else fail "$*: $FOO"; fi
    inc
}

test_it_long_file_fail() {
    FOO=$(lwp-request -m $1 http://${PASS}@localhost:${AGENT_PORT}/$2 <"$3")
    if [ "x$?" = "x1" ]; then pass; else fail "$*: $FOO"; fi
    inc
    if echo $FOO | egrep -q "$4";then pass; else fail "$*: $FOO"; fi
    inc
}

test_it_long_content_fail() {
    FOO=$(lwp-request -m $1 http://${PASS}@localhost:${AGENT_PORT}/$2 <<<"$3")
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
    lwp-request -m GET "http://${PASS}@localhost:${AGENT_PORT}/$1" > $NAME
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
    DIFF=$(echo -e "$FOO" | tail -n1 | sed 's/^.*:/:/')
    if [ "x$DIFF" = "x: ok" ]; then pass; else fail "json failed to validate: $1: $DIFF"; fi
    lwp-request -m GET -ed "http://${PASS}@localhost:${AGENT_PORT}/$1" | grep -q 'Content-Type: application/json'
    if [ "x$?" = "x0" ]; then pass; else fail "json failed content-type check: $1 failed"; fi
}
