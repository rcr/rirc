#!/bin/bash

set -e

export CC=gcc
export CFLAGS_DEBUG="-fstack-usage"

make -e clean rirc.debug

find bld -name "*.su" -exec cat "{}" ";" | sort -n -k2 | column -t
