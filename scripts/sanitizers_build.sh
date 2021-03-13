#!/bin/bash

set -e

export CC=clang

export CFLAGS_DEBUG="-fsanitize=address,undefined -fno-omit-frame-pointer"
export LDFLAGS="-fsanitize=address,undefined"

make -e clean rirc.debug

mv rirc.debug rirc.debug.address

export CFLAGS_DEBUG="-fsanitize=thread,undefined -fno-omit-frame-pointer"
export LDFLAGS="-fsanitize=thread,undefined"

make -e clean rirc.debug

mv rirc.debug rirc.debug.thread
