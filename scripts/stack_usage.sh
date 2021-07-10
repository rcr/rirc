#!/bin/bash

set -e

export CC=gcc
export CFLAGS="-fstack-usage"

make clean rirc

find build -name "*.su" -exec cat "{}" ";" | sort -n -k2 | column -t
