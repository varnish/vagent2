#!/bin/bash

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi
. util.sh
init_all

is_running
test_it_long GET backendjson  '"admin": "probe"'
test_it_long GET backendjson/ '"admin": "probe"'

test_it PUT backend/boot.default sick
test_it_long GET backendjson  '"admin": "sick"'

test_it PUT backend/boot.default healthy
test_it_long GET backendjson  '"admin": "healthy"'

test_it PUT backend/boot.default probe
test_it_long GET backendjson  '"admin": "probe"'

test_it_fail PUT backend/boot.default 1000 'Invalid state 1000'
test_it_long GET backendjson  '"admin": "probe"'
