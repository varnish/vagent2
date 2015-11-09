#!/bin/bash

. util.sh

init_all

wrap_test()
{
	if [ -z "$REDIR_FD" ]; then
		fail
	fi
	A=$($*)
	if echo $A | grep -q "Fail"; then
		echo >$REDIR_FD 1
		echo >&2 $A
	else
		echo >$REDIR_FD 0
	fi
}

getrfile()
{
	echo $TMPDIR/fifotest_${RANDOM}_${RANDOM}_${RANDOM}
}

mkrandom()
{
	N=""
	while true; do
		N=$(getrfile)
		if [ ! -e $N ]; then
			break;
		fi
	done
	mkfifo $N
	echo $N
}

collect_res()
{
	res=0
	n=0
	for fd in $*; do
		read x < $fd
		res=$(( $res + $x ));
		n=$(( $n + 1 ))
		rm $fd
	done
	#echo Fail: $res
	#echo Read: $n
	ret=$(( $ret + $res ))
	return $res
}

repeatit()
{
	testfds=""
	for n in $(seq 1 $REPS); do
		export REDIR_FD=$(mkrandom)
		testfds="$testfds ${REDIR_FD}"
		$* &
	done
	collect_res $testfds
	if [ $? = 0 ]; then pass; else fail; fi
	inc
}

export REPS=10
repeatit wrap_test test_json vcljson/
repeatit wrap_test test_it_long GET vcl/ "" "active"
repeatit wrap_test test_json paramjson/
repeatit wrap_test test_json paramjson/acceptor_sleep_decay
repeatit wrap_test test_json stats
repeatit wrap_test test_json log/100/ReqURL
repeatit wrap_test test_it_long GET vcl/ "" "active"
repeatit wrap_test test_json log/100
repeatit wrap_test test_json log/100/ReqURL
exit $ret
