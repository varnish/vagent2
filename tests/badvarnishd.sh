#!/bin/bash

. util.sh
SRCDIR=${SRCDIR:-"."}
VARNISH_PID=$TMPDIR/varnish.pid
PHASE=1
VARNISH_PORT=$(( 1024 + ( $RANDOM % 48000 ) ))

init_password
start_backend
phase() {
	now=$(date +%s)
	echo
	echo "$now Phase $PHASE: $*"
	PHASE=$(( $PHASE + 1 ))
}

start_varnish_no_t() {
	echo -e "\tStarting varnish with no -T"
	VARNISH_PORT=$(( 1024 + ( $RANDOM % 48000 ) ))
	echo -e "\tVarnish port: $VARNISH_PORT"
	varnishd -f "${SRCDIR}/data/boot.vcl" \
	    -P "$VARNISH_PID" \
	    -n "$TMPDIR" \
	    -p auto_restart=off \
	    -a 127.0.0.1:$VARNISH_PORT \
	    -s malloc,50m
	
	pidwait varnish
	N_ARG="-n $TMPDIR"
	export N_ARG
}

do_echo_test() {
	INDENT="\t\t"
	export VARNISH_PORT AGENT_PORT NOSTATUS
	echo -e "\tEcho tests:"
	test_it GET echo "" ""
	test_it POST echo "Foobar" "Foobar"
	test_it POST echo "" ""
}

do_vlog_test() {
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

uptime_1() {
	UPTIME1=$(lwp-request -m GET http://${PASS}@localhost:$AGENT_PORT/stats | grep uptime)
	echo -e "\tUptime string: $UPTIME1"
}
uptime_2() {
	echo -e "\tComparing uptime"
	UPTIME2=$(lwp-request -m GET http://${PASS}@localhost:$AGENT_PORT/stats | grep uptime)
	if [ "x$?" != "x0" ]; then fail; else pass; fi
	inc
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
sleep 1 # Varnish can take a moment to die. Give it time.
start_varnish

echo -e "\tWaiting, then testing state"
sleep 1
NOSTATUS=0
is_running
test_it GET status "" "Child in state running"
echo -e "\tAnd again"
stop_varnish
sleep 1 # Varnish can take a moment to die. Give it time.
start_varnish

echo -e "\tWaiting, then testing state"
sleep 1
NOSTATUS=0
# First will fail, as that's what tells us to re-read the shmlog.
test_it_fail GET status "" "Varnishd disconnected"
is_running
test_it PUT stop "" ""
test_it GET status "" "Child in state stopped"


phase "Cleanup"
stop_agent
stop_varnish
stop_backend
rm -fr $TMPDIR
exit $ret
