#!/bin/bash

. util.sh

ARGS=$(grep '= getopt(' $ORIGPWD/../src/main.c | sed -e 's/[^"]*"//; s/"[^"]*$//; s/://g; s/\(.\)/\1 /g' )
ARGS2=$(awk '/^OPTIONS/ { opt=1 }; opt==1 && /^\-/ {print $1}; /^VARNISH/ {opt=0}' $ORIGPWD/../README.rst)

for a in $ARGS; do
	$ORIGPWD/../src/varnish-agent -h  2>&1 | egrep -q "^[ ]*-$a";
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

for a in $ARGS2; do
	$ORIGPWD/../src/varnish-agent -h  2>&1 | egrep -q "^[ ]*$a";
	if [ $? -eq "0" ]; then pass;
	else fail "$a missing in -h"
	fi
	inc
done

for a in $ARGS2; do
	b=$(echo -- $a | sed 's/-//g; s/ //g;')
	echo $ARGS | grep -q $b
	if [ $? -eq "0" ]; then pass;
	else fail "$a missing in getopt()"
	fi
	inc

	$ORIGPWD/../src/varnish-agent -h  2>&1 | egrep -q "^[ ]*-$b";
	if [ $? -eq "0" ]; then pass;
	else fail "$b missing in -h"
	fi
	inc

done

exit $ret
