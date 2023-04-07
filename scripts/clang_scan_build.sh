#!/bin/bash

set -e
set -u

export CFLAGS="-pipe -O0"
export LDFLAGS="-pipe"
export MAKEFLAGS="-e -f Makefile.dev -j $(nproc)"

make clean-dev clean-lib
make libs

scan-build -V make
