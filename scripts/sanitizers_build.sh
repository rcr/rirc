#!/bin/bash

set -e

export CC=clang

export CC_EXT="-fsanitize=address,undefined -fno-omit-frame-pointer"
export LD_EXT="-fsanitize=address,undefined"

make -e clean rirc.debug

mv rirc.debug rirc.debug.address

export CC_EXT="-fsanitize=thread,undefined -fno-omit-frame-pointer"
export LD_EXT="-fsanitize=thread,undefined"

make -e clean rirc.debug

mv rirc.debug rirc.debug.thread
