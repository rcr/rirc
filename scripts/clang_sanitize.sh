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

CFLAGS_BASE="-pipe -O0 -g"
LDFLAGS_BASE="-pipe"

export CC="clang"
export CFLAGS="$CFLAGS_BASE"
export LDFLAGS="$LDFLAGS_BASE"
export MAKEFLAGS="-e -f Makefile.dev -j $(nproc)"

make clean-lib
make libs

export CFLAGS="$CFLAGS_BASE -fsanitize=address,undefined -fno-omit-frame-pointer"
export LDFLAGS="$LDFLAGS_BASE -fsanitize=address,undefined"

if [ ! -z "$BUILD" ]; then
	make clean-dev
	make rirc.debug && mv rirc.debug rirc.debug.address
fi

if [ ! -z "$CHECK" ]; then
	make clean-dev
	make check
fi

export CFLAGS="$CFLAGS_BASE -fsanitize=memory,undefined -fno-omit-frame-pointer"
export LDFLAGS="$LDFLAGS_BASE -fsanitize=memory,undefined"

if [ ! -z "$BUILD" ]; then
	make clean-dev
	make rirc.debug && mv rirc.debug rirc.debug.memory
fi

if [ ! -z "$CHECK" ]; then
	make clean-dev
	make check
fi

export CFLAGS="$CFLAGS_BASE -fsanitize=thread,undefined -fno-omit-frame-pointer"
export LDFLAGS="$LDFLAGS_BASE -fsanitize=thread,undefined"

if [ ! -z "$BUILD" ]; then
	make clean-dev
	make rirc.debug && mv rirc.debug rirc.debug.thread
fi
