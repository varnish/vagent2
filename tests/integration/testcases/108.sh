#!/bin/bash

# Yves Hwang
# yveshwang@gmail.com
# 16.04.2014

# test case addressing
# https://github.com/varnish/vagent2/issues/108

echo "==============================="
echo "Test case for github issue #108"
pidfile=$(pwd)/varnish-agent.pid
resultfile=$(pwd)/108.result

# setup
echo "Firing up nc on port 8080"
nc -4 -l 8080 > $resultfile &
echo "sleep for 3"
sleep 3
echo "Fireup varnish-agent real quick, pidfile lives in $pidfile"
varnish-agent -z http://localhost:8080 -P $pidfile&
sleep 3
cat $resultfile
proc=$(cat $pidfile)
kill -9 $proc

# check results
result=0
result=$(grep "HEAD" $resultfile | wc -l)
echo $result

if [[ $result -eq 0 ]] 
    then
        echo "Test passed"
    else
        echo "Test failed"
fi


# clean up
rm -f $pidfile
rm -f $resultfile

echo "Done"
echo "==============================="
exit $result

