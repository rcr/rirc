#!/bin/sh

set -x
set -e

rm -rf coverage
mkdir coverage

ENV=
ENV="$ENV CC=\"gcc\""
ENV="$ENV CC_EXT=\"-fprofile-arcs -ftest-coverage -fprofile-abs-path\""
ENV="$ENV LD_EXT=\"-fprofile-arcs\""

eval $ENV make clean debug test
find . -name "*.gcno" -print0 | xargs -r0 gcov -prs $(pwd)/src 2>/dev/null
find . -name "*.gcov" -print0 | xargs -r0 mv -t coverage
