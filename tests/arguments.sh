#!/bin/bash

. util.sh

ARGS=$(grep '= getopt(' $ORIGPWD/../src/main.c | sed -e 's/[^"]*"//; s/"[^"]*$//; s/://g; s/\(.\)/\1 /g' )

for a in $ARGS; do
	$ORIGPWD/../src/varnish-agent -h  2>&1 | egrep -q "^-$a";
	if [ $? -eq "0" ]; then pass;
	else fail "$a missing in -h"
	fi
	inc
done


for a in $ARGS; do
	egrep -q "^-$a" $ORIGPWD/../README.rst;
	if [ $? -eq "0" ]; then pass;
	else fail "$a missing in README.rst"
	fi
	inc
done

