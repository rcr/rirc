#!/bin/bash

# FIXME: undo the long live branch pattern in 
# https://sonarcloud.io/project/branches?id=rirc

set -e

fail() { >&2 printf "%s\n" "$*"; exit 1; }

if [[ -z $1 ]]; then
	fail "Usage: '$0 dir'"
fi

SONAR_VER="4.5.0.2216"

BUILD_WRAPPER_BIN="$1/build-wrapper-linux-x86/build-wrapper-linux-x86-64"
SONAR_SCANNER_BIN="$1/sonar-scanner-$SONAR_VER-linux/bin/sonar-scanner"

BUILD_WRAPPER_OUT="$1/bw-out"

SONAR_SCANNER_CONF="sonar-project.properties"

if [[ ! -f "$BUILD_WRAPPER_BIN" ]]; then
	fail "missing build-wrapper binary"
fi

if [[ ! -f "$SONAR_SCANNER_BIN" ]]; then
	fail "missing sonar-scanner binary"
fi

# FIXME:
# "the branch or pull request parameter is missing"

# FIXME: just forget about coverage until it works properly with llvm or something
#        ive wasted enough time on this...
#        add test coverage to README or something? or use something else like coveralls?

cat << EOF >> "$SONAR_SCANNER_CONF"
# Server
sonar.host.url = https://sonarcloud.io

# Project
sonar.organization   = rirc
sonar.projectKey     = rirc
sonar.projectName    = rirc
sonar.projectVersion = $(git rev-parse --short HEAD)
sonar.branch.name    = $(git rev-parse --symbolic-full-name HEAD)
sonar.links.homepage = https://rcr.io/rirc/
sonar.links.scm      = https://git.sr.ht/~rcr/rirc/
sonar.links.ci       = https://builds.sr.ht/~rcr/rirc/

# Source
sonar.sources = src,test

# C
sonar.cfamily.build-wrapper-output = $BUILD_WRAPPER_OUT
sonar.cfamily.cache.enabled        = false
sonar.cfamily.threads              = $(nproc)
EOF

make clean

eval "$BUILD_WRAPPER_BIN --out-dir $BUILD_WRAPPER_OUT make all check"
eval "$SONAR_SCANNER_BIN"

rm -f "$SONAR_SCANNER_CONF"
