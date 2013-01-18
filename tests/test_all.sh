#!/bin/bash

SRCDIR=$(dirname $0)
cd $SRCDIR
ret=0
echo "Echo:"
./echo.sh
ret=$(( $ret + $? ))
echo "Bans:"
./ban.sh
ret=$(( $ret + $? ))

echo "Log:"
./log.sh
ret=$(( $ret + $? ))

echo "Status:"
./status.sh
ret=$(( $ret + $? ))

echo "VCL:"
./vcl.sh
ret=$(( $ret + $? ))

echo "Json:"
./json.sh
ret=$(( $ret + $? ))

exit $ret
