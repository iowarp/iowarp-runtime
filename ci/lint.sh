#!/bin/bash

CHIMAERA_ROOT=$1

echo "RUNNING CPPLINT"
cpplint --recursive \
"${CHIMAERA_ROOT}/src" "${CHIMAERA_ROOT}/include" "${CHIMAERA_ROOT}/test" \
--exclude="${CHIMAERA_ROOT}/test/unit/external"