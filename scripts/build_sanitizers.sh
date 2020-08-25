#!/bin/bash

set -e

export CC=clang

export CC_EXT="-fsanitize=address,undefined"
export LD_EXT="-fsanitize=address,undefined"

make -e clean debug

mv rirc.debug rirc.debug.address

export CC_EXT="-fsanitize=thread,undefined"
export LD_EXT="-fsanitize=thread,undefined"

make -e clean debug

mv rirc.debug rirc.debug.thread
