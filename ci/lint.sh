#!/bin/bash

CHIMEARA_ROOT=$1

echo "RUNNING CPPLINT"
cpplint --recursive \
"${CHIMEARA_ROOT}/src" "${CHIMEARA_ROOT}/include" "${CHIMEARA_ROOT}/test" \
--exclude="${CHIMEARA_ROOT}/test/unit/external"