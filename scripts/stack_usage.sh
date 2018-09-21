#!/bin/sh

make -e D_EXT=-fstack-usage clean debug
find . -iname "*.su" -exec cat "{}" \; | sort -n -k2 | column -t
