#!/bin/bash

FOO=$(lwp-request -m GET http://localhost:6085/echo || exit 1)
if [ "x$FOO" = "x" ]; then echo "Passed 1"; else echo "Failed 1"; fi

FOO=$(lwp-request -m POST http://localhost:6085/echo <<<"" || exit 1)
if [ "x$FOO" = "x" ]; then echo "Passed 2"; else echo "Failed 2"; fi

FOO=$(lwp-request -m POST http://localhost:6085/echo <<<"foobar" || exit 1)
if [ "x$FOO" = "xfoobar" ]; then echo "Passed 3"; else echo "Failed 3"; fi

