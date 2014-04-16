#!/bin/bash

# Yves Hwang
# yveshwang@gmail.com
# 16.04.2014

echo "Executing all integration test cases."
for s in $(pwd)/testcases/*;do [ -x $s ] && $s || : ;done
