#!/bin/bash

set -e

export CC=gcc
export CC_EXT="-fstack-usage"

make -e clean debug

find . -name "*.su" -exec cat "{}" ";" | sort -n -k2 | column -t
