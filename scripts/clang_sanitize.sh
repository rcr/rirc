#!/bin/bash

# Build and check with clang sanitizers:
#
#   $ ./scripts/clang-sanitize.sh [--build] [--check]
#
# for core dumps:
#   $ export ASAN_OPTIONS="abort_on_error=1:disable_coredump=0

set -u

BUILD=
CHECK=

for arg in "$@"; do
	[[ "$arg" == "--build" ]] && eval "BUILD=true"
	[[ "$arg" == "--check" ]] && eval "CHECK=true"
done

if [ -z "$BUILD" ] && [ -z "$CHECK" ]; then
	exit
fi

CFLAGS_BASE="-pipe -O1 -g -fno-omit-frame-pointer"
LDFLAGS_BASE="-pipe"

export CC="clang"
export CFLAGS="$CFLAGS_BASE"
export LDFLAGS="$LDFLAGS_BASE"
export MAKEFLAGS="-e -f Makefile.dev -j $(nproc)"

export CFLAGS="$CFLAGS_BASE -fsanitize=address,undefined"
export LDFLAGS="$LDFLAGS_BASE -fsanitize=address,undefined"

if [ ! -z "$BUILD" ]; then
	rm -f rirc.debug.address
	make clean-dev clean-lib
	make rirc.debug && mv rirc.debug rirc.debug.address
fi

if [ ! -z "$CHECK" ]; then
	make clean-dev clean-lib
	make check
fi

export CFLAGS="$CFLAGS_BASE -fsanitize=memory,undefined"
export LDFLAGS="$LDFLAGS_BASE -fsanitize=memory,undefined"

if [ ! -z "$BUILD" ]; then
	rm -f rirc.debug.memory
	make clean-dev clean-lib
	make rirc.debug && mv rirc.debug rirc.debug.memory
fi

if [ ! -z "$CHECK" ]; then
	make clean-dev clean-lib
	make check
fi

export CFLAGS="$CFLAGS_BASE -fsanitize=thread,undefined"
export LDFLAGS="$LDFLAGS_BASE -fsanitize=thread,undefined"

if [ ! -z "$BUILD" ]; then
	rm -f rirc.debug.thread
	make clean-dev clean-lib
	make rirc.debug && mv rirc.debug rirc.debug.thread
fi
