#!/bin/bash

# Development build script.
#
#  Usage, e.g.:
#
#   $ ./scripts/build.sh 'check rirc.debug; gdb ./rirc.debug'

set -e

export CC=clang
export CFLAGS_DEBUG="-Wshadow"
export LDFLAGS="-flto -fuse-ld=lld"
export LDFLAGS_DEBUG="-fuse-ld=lld"

make clean

if [ -x "$(command -v bear)" ]; then
	make build
	BEAR="bear --append --output ./build/compile_commands.json --"
fi

if [ -x "$(command -v entr)" ]; then
	find -name '*.c' \
	  -o -name '*.h' \
	  -o -name Makefile | grep -v './lib/' | entr -cs "$BEAR make -j $(nproc) $*"
else
	eval "$BEAR make -j $(nproc) $*"
fi
