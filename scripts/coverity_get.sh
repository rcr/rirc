#!/bin/bash

set -e

fail() { >&2 printf "%s\n" "$*"; exit 1; }

if [[ -z $1 ]]; then
	fail "Usage: '$0 dir'"
fi

if [[ -z "${COVERITY_TOKEN}" ]]; then
	fail "missing env COVERITY_TOKEN"
fi

COVERITY_MD5="$1/coverity_tool.md5"
COVERITY_TGZ="$1/coverity_tool.tgz"

mkdir "$1"

echo "curl https://scan.coverity.com/download/linux64 ..."

curl -fs --show-error https://scan.coverity.com/download/linux64 -o "$COVERITY_TGZ" --data "token=$COVERITY_TOKEN&project=rcr%2Frirc"
curl -fs --show-error https://scan.coverity.com/download/linux64 -o "$COVERITY_MD5" --data "token=$COVERITY_TOKEN&project=rcr%2Frirc&md5=1"

printf "%s\t$COVERITY_TGZ" "$(cat "$COVERITY_MD5")" | md5sum -c -

tar xzf "$COVERITY_TGZ" -C "$1" --strip-components 1
