#!/usr/bin/env python

import fileinput
import demjson

lines=""
for line in fileinput.input():
	lines = lines + line

demjson.decode(lines, strict=True)
