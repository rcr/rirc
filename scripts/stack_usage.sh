#!/bin/bash

set -e

export CC=clang
export CFLAGS="-fstack-usage"
export LDFLAGS="-fuse-ld=lld"

make clean rirc

find build -name '*.su' | xargs cat | sort -n -k2 | column -t
