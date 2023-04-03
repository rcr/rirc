#!/bin/bash

set -e

export CC=gcc
export CFLAGS="-pipe -O0 -fstack-usage"
export LDFLAGS="-pipe"

export MAKEFLAGS="-e -f Makefile.dev -j $(nproc)"

make clean-dev
make rirc.debug check

find build -name '*.su' | xargs cat | sort -n -k2 | column -t | tail -n 50
