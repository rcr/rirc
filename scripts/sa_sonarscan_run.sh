#!/bin/bash

set -e

fail() { >&2 printf "%s\n" "$*"; exit 1; }

if [[ -z $1 ]]; then
	fail "Usage: '$0 dir'"
fi

DIR="$1"

SONAR_VER="4.6.2.2472"

SONAR_CONFIG="$DIR/sonar-project.properties"

SONAR_SCANNER_BIN="$DIR/sonar-scanner-$SONAR_VER-linux/bin/sonar-scanner"
BUILD_WRAPPER_BIN="$DIR/build-wrapper-linux-x86/build-wrapper-linux-x86-64"
BUILD_WRAPPER_OUT="$DIR/bw-out"

if [[ ! -f "$BUILD_WRAPPER_BIN" ]]; then
	fail "missing build-wrapper binary"
fi

if [[ ! -f "$SONAR_SCANNER_BIN" ]]; then
	fail "missing sonar-scanner binary"
fi

cat << EOF >> "$SONAR_CONFIG"
# Server
sonar.host.url = https://sonarcloud.io

# Project
sonar.organization   = rirc
sonar.projectKey     = rirc
sonar.projectName    = rirc
sonar.projectVersion = $(git rev-parse --short HEAD)
sonar.branch.name    = $(git name-rev --name-only HEAD)
sonar.links.homepage = https://rcr.io/rirc/
sonar.links.scm      = https://git.sr.ht/~rcr/rirc/
sonar.links.ci       = https://builds.sr.ht/~rcr/rirc/

# C, Sources
sonar.cfamily.build-wrapper-output = $BUILD_WRAPPER_OUT
sonar.sources                      = src

# Output
sonar.working.directory = $DIR/scannerwork
EOF

make clean
make libs

eval "$BUILD_WRAPPER_BIN --out-dir $BUILD_WRAPPER_OUT make all check"
eval "$SONAR_SCANNER_BIN --define project.settings=$SONAR_CONFIG"
