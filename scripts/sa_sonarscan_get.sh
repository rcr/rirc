#!/bin/bash

set -e

fail() { >&2 printf "%s\n" "$*"; exit 1; }

if [[ -z $1 ]]; then
	fail "Usage: '$0 dir'"
fi

SONAR_VER="4.6.2.2472"

BUILD_ZIP="$1/build-wrapper.zip"
SONAR_ZIP="$1/sonar-scanner.zip"
SONAR_MD5="$1/sonar-scanner.md5"

BUILD_ZIP_URL="https://sonarcloud.io/static/cpp/build-wrapper-linux-x86.zip"
SONAR_ZIP_URL="https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-$SONAR_VER-linux.zip"
SONAR_MD5_URL="https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-$SONAR_VER-linux.zip.md5"

mkdir -p "$1"

echo "*" > "$1/.gitignore"

curl -fsS "$BUILD_ZIP_URL" -o "$BUILD_ZIP"
curl -fsS "$SONAR_ZIP_URL" -o "$SONAR_ZIP"
curl -fsS "$SONAR_MD5_URL" -o "$SONAR_MD5"

printf "%s  %s" "$(cat "$SONAR_MD5")" "$SONAR_ZIP" | md5sum --quiet -c -

unzip -qq "$BUILD_ZIP" -d "$1"
unzip -qq "$SONAR_ZIP" -d "$1"
