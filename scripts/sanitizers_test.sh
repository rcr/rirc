#!/bin/bash

set -e

export CC=clang
export CFLAGS_DEBUG="-fsanitize=address,undefined -fno-omit-frame-pointer"
export LDFLAGS="-fsanitize=address,undefined -fuse-ld=lld"

# for core dumps:
# export ASAN_OPTIONS="abort_on_error=1:disable_coredump=0"

make -e clean check
