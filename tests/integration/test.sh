#!/bin/bash

# Yves Hwang
# yveshwang@gmail.com
# 16.04.2014

echo "Executing all integration test cases."
echo "... ensure this is executed from the root folder of vagent2"
pwd
for s in $(pwd)/tests/integration/testcases/*;do [ -x $s ] && $s || : ;done
