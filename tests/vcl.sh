#!/bin/bash 

if [ "$(basename $PWD)" != "tests" ]; then
	echo "Must run tests from tests/ directory"
	exit 1
fi

. util.sh
init_all


DUMMYVCL="$(cat data/smallvcl)"
test_vcl_file() {
	VCL="$1"
	OUT="${TMPDIR}/$(basename $VCL).tmp"
	lwp-request -m PUT "http://${PASS}@localhost:${AGENT_PORT}/vcl/test3" <$VCL > /dev/null
	if [ "x$?" = "x0" ]; then pass; else fail "VCL step 1: $OUT"; fi
	inc
	lwp-request -m GET "http://${PASS}@localhost:${AGENT_PORT}/vcl/test3" >$OUT
	if [ "x$?" = "x0" ]; then pass; else fail "VCL step 2: $OUT"; fi
	inc
	if diff -q $VCL $OUT > /dev/null; then pass; else fail "VCL step 3: $OUT"; fi
	inc
	if diff -q $VCL ${TMPDIR}/vcl/test3.auto.vcl; then pass; else fail "VCL step 4"; fi
	inc
	if [ ! -e ${TMPDIR}/vcl/boot.vcl ]; then pass; else fail "VCL boot.vcl exist before deploy"; fi
	inc
	test_it PUT vcldeploy/test3 "" ""
	if [ -e ${TMPDIR}/vcl/boot.vcl ]; then pass; else fail "boot.vcl not created"; fi	
	inc
	if diff -q ${TMPDIR}/vcl/boot.vcl $VCL; then pass; else fail "boot.vcl isn't $VCL"; fi
	inc
	test_it_fail DELETE vcl/test3 "" "Cannot discard active VCL program"
	test_it_fail PUT vcldeploy/boot "" "Deployed ok, but NOT PERSISTED."
	test_it DELETE vcl/test3 "" ""
	rm $TMPDIR/vcl/boot.vcl
}

is_running
test_it_fail GET vcl "" "Failed"
test_it_long GET vcl/ "" "active"
test_it_long GET vcl/boot "" "backend"
test_it_fail GET vcljson/foo "" "Invalid VCL-url."
test_it_long POST vcl/ "$DUMMYVCL" "VCL compiled."
TIME=$(date +%s)

test_vcl_file data/smallvcl
test_vcl_file data/longvcl
test_it_long PUT vcl/foo "$DUMMYVCL" ""
test_it_long_fail PUT vcl/foo "$DUMMYVCL" "Already a VCL program named foo"
test_it_long_fail PUT "vcl/foo-bar-baz" "$DUMMYVCL" "VCL name is not valid"
mkdir $TMPDIR/vcl/foo2.auto.vcl
test_it_long_fail PUT vcl/foo2 "$DUMMYVCL" "VCL stored in varnish OK, but persisting to disk failed."
test_it DELETE vcl/foo2 "" ""
rm -r $TMPDIR/vcl/foo2.auto.vcl
test_it_long PUT vcl/foo2 "$DUMMYVCL" "VCL compiled."
rm -rf $TMPDIR/vcl/boot.vcl
mkdir $TMPDIR/vcl/boot.vcl
test_it_fail PUT vcldeploy/foo2 "" "Deployed ok, but NOT PERSISTED."
rm -rf $TMPDIR/vcl/boot.vcl
test_it PUT vcldeploy/foo2 "" ""

if diff -q data/smallvcl $TMPDIR/vcl/boot.vcl; then pass; else fail "boot.vcl isnt data/smallvcl"; fi
inc

TIME2=$(date +%s)
if [ "x${TIME}" = "x${TIME2}" ]; then sleep 1; fi
test_it_long POST vcl/ "$DUMMYVCL" "VCL compiled."

exit $ret
