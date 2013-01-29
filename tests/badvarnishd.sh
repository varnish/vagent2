#!/bin/bash

. util.sh
SRCDIR=${SRCDIR:-"."}
VARNISH_PID=$TMPDIR/varnish.pid
PHASE=1
VARNISH_PORT=$(( 1024 + ( $RANDOM % 48000 ) ))

function phase()
{
	now=$(date +%s)
	echo
	echo "$now Phase $PHASE: $*"
	PHASE=$(( $PHASE + 1 ))
}


function start_varnish_no_t()
{
	echo -e "\tStarting varnish with no -T"
	sleep 1
	VARNISH_PORT=$(( 1024 + ( $RANDOM % 48000 ) ))
	echo -e "\tVarnish port: $VARNISH_PORT"
	varnishd -f "${SRCDIR}/data/boot.vcl" \
	    -P "$VARNISH_PID" \
	    -n "$TMPDIR" \
	    -p auto_restart=off \
	    -a 127.0.0.1:$VARNISH_PORT \
	    -s malloc,50m
	varnishpid="$(cat "$VARNISH_PID")"
	sleep 5
	echo -e "\tStarted varnish. Pid $varnishpid"
	if [ -z "$varnishpid" ]; then
		fail "NO VARNISHPID? Bad stuff..."
		exit 1;
	fi
	sleep 1
	if kill -0 $varnishpid; then
		echo -e "\tVarnish started ok, we think.";
	else
		fail "Varnish not started correctly? FAIL";
		exit 1;
	fi
}

function stop_varnish()
{
	varnishpid="$(cat "$VARNISH_PID")"
	if [ -z "$varnishpid" ]; then
		fail "NO VARNISHPID? Bad stuff..."
		exit 1;
	fi
	echo -e "\tStopping varnish($varnishpid)"
	kill $varnishpid
	sleep 5
}

function start_agent()
{
	echo -e "\tStarting varnish-agent"
	sleep 1
	AGENT_PORT=$(( 1024 + ( $RANDOM % 48000 ) ))
	echo -e "\tAgent port: $AGENT_PORT"
	OUTPUT="$( ${ORIGPWD}/../src/varnish-agent -n ${TMPDIR} -p ${TMPDIR} -P ${TMPDIR}/agent.pid -c "$AGENT_PORT" 2>&1)"
	echo -en "\t\t"
	echo $OUTPUT
	sleep 1
}
	
function stop_agent()
{
	agentpid=$(cat ${TMPDIR}/agent.pid)
	if [ -z $agentpid ]; then
		fail "Stopping agent but no agent pid found. BORK"
		exit 1;
	fi
	echo -e "\tStopping agent($agentpid)"
	kill $agentpid
	sleep 1
}


function do_echo_test()
{
	INDENT="\t\t"
	export VARNISH_PORT AGENT_PORT NOSTATUS
	echo -e "\tEcho tests:"
	test_it GET echo "" ""
	test_it POST echo "Foobar" "Foobar"
	test_it POST echo "" ""
}

function do_vlog_test()
{
	INDENT="\t\t"
	test_it_long GET log "" "\"tag\":"
	test_it_fail GET log/0 "" "Not a number"
	test_it_long GET log/2 "" "\"tag\":"
	GET http://localhost:${VARNISH_PORT}/foobar > /dev/null
	test_it_long GET log/1/RxURL "" "\"tag\":"
	test_it_long GET log/1/RxURL/foobar "" "/foobar"
	test_json stats
	test_json log/100/RxURL
}

function uptime_1()
{
	UPTIME1=$(lwp-request -m GET http://localhost:$AGENT_PORT/stats | grep uptime)
	echo -e "\tUptime string: $UPTIME1"
}
function uptime_2()
{
	echo -e "\tComparing uptime"
	UPTIME2=$(lwp-request -m GET http://localhost:$AGENT_PORT/stats | grep uptime)
	if [ "x$?" != "x0" ]; then fail; else pass; fi
	inc
	echo -e "\tUptime2: $UPTIME2"
	if [ "x$UPTIME" != "x$UPTIME2" ]; then pass; else fail "$OUT vs $OUT2"; fi
	inc
}

phase "No -T"
start_varnish_no_t
start_agent

NOSTATUS=1
do_echo_test
do_vlog_test
uptime_1

phase "No -T, varnishd restart"
stop_varnish
start_varnish_no_t

do_echo_test
do_vlog_test

uptime_2
phase "Agent with no shmlog"

echo -e "\tRemoving shmlog"
rm $TMPDIR/_.vsm
stop_agent
start_agent

do_echo_test

echo -e "\tTesting shmlogfailure:"
test_it_fail GET stats "" "Couldn't open shmlog"
do_echo_test

phase "Shmlog suddenly re-appears"
stop_varnish
start_varnish

echo -e "\tWaiting, then testing state"
sleep 3
NOSTATUS=0
is_running
test_it GET status "" "Child in state running"
echo -e "\tAnd again"
stop_varnish
start_varnish

echo -e "\tWaiting, then testing state"
sleep 3
NOSTATUS=0
# First will fail, as that's what tells us to re-read the shmlog.
test_it_fail GET status "" "Varnishd disconnected"
is_running
test_it PUT stop "" ""
test_it GET status "" "Child in state stopped"


phase "Cleanup"
stop_agent
stop_varnish
exit $ret
