#!/bin/bash

# Usage: entr.sh [make targets]
#   e.g.:
#     $ clang-entr.sh
#     $ clang-entr.sh check
#     $ clang-entr.sh all check

set -e

export MAKEFLAGS="-j $(nproc)"

make clean

find -name '*.c' \
  -o -name '*.h' \
  -o -name Makefile | grep -v './lib/' | entr -c make "$@"
