#!/bin/bash

set -e

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

COVERITY_OUT="cov-int"
COVERITY_TAR="cov-int.tgz"

VERSION=$(git rev-parse --short HEAD)

PATH=$(pwd)/$1/bin:$PATH cov-build --dir "$COVERITY_OUT" make clean rirc debug test

tar czf "$COVERITY_TAR" "$COVERITY_OUT"

curl \
	--form file=@"$COVERITY_TAR" \
	--form email="$COVERITY_EMAIL" \
	--form token="$COVERITY_TOKEN" \
	--form version="$VERSION" \
	https://scan.coverity.com/builds?project=rcr%2Frirc
