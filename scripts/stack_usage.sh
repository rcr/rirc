#!/bin/sh

set -e

ENV=""
ENV="$ENV CC=\"gcc\""
ENV="$ENV CC_EXT=\"-fstack-usage\""

eval $ENV make -e clean debug

find -name "*.su" -exec cat "{}" ";" | sort -n -k2 | column -t
