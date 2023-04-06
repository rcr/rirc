#!/bin/bash

set -e
set -u

fail() { >&2 printf "%s\n" "$*"; exit 1; }

if [[ -z $1 ]]; then
	fail "Usage: '$0 dir'"
fi

if [[ -z "${COVERITY_EMAIL}" ]]; then
	fail "missing env COVERITY_EMAIL"
fi

if [[ -z "${COVERITY_TOKEN}" ]]; then
	fail "missing env COVERITY_TOKEN"
fi

DIR="$1"

COVERITY_OUT="cov-int"
COVERITY_TAR="cov-int.tgz"

VERSION=$(git rev-parse --short HEAD)

export PATH="$PWD/$DIR/bin:$PATH"

export CFLAGS="-pipe -O0"
export LDFLAGS="-pipe"
export MAKEFLAGS="-e -f Makefile.dev -j $(nproc)"

make clean-dev clean-lib
make libs

cov-build --dir "$COVERITY_OUT" make rirc check

tar czf "$COVERITY_TAR" "$COVERITY_OUT"

curl https://scan.coverity.com/builds?project=rcr%2Frirc \
	--form description="$VERSION" \
	--form email="$COVERITY_EMAIL" \
	--form file=@"$COVERITY_TAR" \
	--form token="$COVERITY_TOKEN" \
	--form version="$VERSION"

mv $COVERITY_OUT "$DIR"
mv $COVERITY_TAR "$DIR"
