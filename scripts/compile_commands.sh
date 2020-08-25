#!/bin/sh

set -e

rm -f compile_commands.json

export CC=clang
export CC_EXT="-Wno-empty-translation-unit"

bear make clean debug
