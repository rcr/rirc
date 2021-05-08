#!/bin/bash

set -e

fail() { >&2 printf "%s\n" "$*"; exit 1; }

if [[ -z $1 ]]; then
	fail "Usage: '$0 dir'"
fi

if [[ -z "${COVERITY_TOKEN}" ]]; then
	fail "missing env COVERITY_TOKEN"
fi

DIR="$1"

COVERITY_MD5="$DIR/coverity_tool.md5"
COVERITY_TGZ="$DIR/coverity_tool.tgz"

mkdir -p "$DIR"

echo "*" > "$DIR/.gitignore"

curl -fsS https://scan.coverity.com/download/linux64 -o "$COVERITY_MD5" --data "token=$COVERITY_TOKEN&project=rcr%2Frirc&md5=1"
curl -fsS https://scan.coverity.com/download/linux64 -o "$COVERITY_TGZ" --data "token=$COVERITY_TOKEN&project=rcr%2Frirc"

printf "%s  %s" "$(cat "$COVERITY_MD5")" "$COVERITY_TGZ" | md5sum --quiet -c -

tar -xzf "$COVERITY_TGZ" -C "$DIR" --strip-components 1
