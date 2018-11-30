#!/bin/sh

set -x
set -e

ENV=
ENV+=" CC=\"gcc\""
ENV+=" CC_EXT=\"-fstack-usage\""

eval $ENV make clean debug

find -name "*.su" -exec cat "{}" ";" | sort -n -k2 | column -t
