#!/bin/bash

set -e

export CC=clang
export CC_EXT="-Wno-empty-translation-unit"

rm -f compile_commands.json

bear make clean rirc.debug
