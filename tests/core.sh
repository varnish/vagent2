#!/bin/bash

SERVER=./server.sh
HOSTPORT=${DUMMY_PORT:-8133}

while $SERVER | nc -l $HOSTPORT
	do date
done
