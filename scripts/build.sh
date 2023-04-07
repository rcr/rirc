#!/bin/bash

# Development build script.
#
#  Usage, e.g.:
#
#   $ ./scripts/build.sh 'mbedtls rirc.debug check; gdb ./rirc.debug'

set -e
set -u

export MAKEFLAGS="-f Makefile.dev -j $(nproc) --keep-going --shuffle --warn-undefined"

make build

if [ -x "$(command -v bear)" ]; then
	BEAR="bear --append --force-wrapper --output build/compile_commands.json --"
else
	BEAR=""
fi

if [ -x "$(command -v entr)" ]; then
	find -name '*.c' \
	  -o -name '*.h' \
	  -o -name 'Makefile.*' | grep -v './lib/' | entr -cs "$BEAR make $*"
else
	eval "$BEAR make $*"
fi
