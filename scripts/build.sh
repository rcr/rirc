#!/bin/bash

# Development build script.
#
#  Usage:
#
#   $ ./scripts/build.sh [make targets]

set -e

export CC=clang
export CFLAGS_DEBUG="-Wshadow"
export LDFLAGS="-flto -fuse-ld=lld"
export LDFLAGS_DEBUG="-fuse-ld=lld"

if [ -x "$(command -v entr)" ]; then
	ENTR="entr -c"
fi

if [ -x "$(command -v bear)" ]; then
	BEAR="bear --append --output ./build/compile_commands.json --"
fi

make clean
make build

find -name '*.c' \
  -o -name '*.h' \
  -o -name Makefile | grep -v './lib/' | $ENTR $BEAR make -j $(nproc) "$@"
