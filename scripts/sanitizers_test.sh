#!/bin/bash

set -e

export CC=clang
export CC_EXT="-fsanitize=address,undefined -fno-omit-frame-pointer"
export LD_EXT="-fsanitize=address,undefined -fuse-ld=lld"

# for core dumps:
# export ASAN_OPTIONS="abort_on_error=1:disable_coredump=0"

make -e clean check
