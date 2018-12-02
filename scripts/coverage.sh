#!/bin/sh

set -x
set -e

CDIR="coverage"

rm -rf $CDIR
mkdir $CDIR

ENV=""
ENV="$ENV CC=\"gcc\""
ENV="$ENV CC_EXT=\"-fprofile-arcs -ftest-coverage\""
ENV="$ENV LD_EXT=\"-fprofile-arcs\""

eval $ENV make clean test

find . -name "*.gcda" -print0 | xargs -r0 mv -t $CDIR
find . -name "*.gcno" -print0 | xargs -r0 mv -t $CDIR

gcov -pr $CDIR/*.gcno
