#!/bin/bash
#
# pre-commit testing hook, setup:
#   ln -s $(readlink -f scripts/pre-commit.sh) .git/hooks/pre-commit

set -e
set -u

echo "Running pre-commit hook..."


export MAKEFLAGS="-f Makefile.dev -j $(nproc)"

make clean-dev

RESULTS=$(make rirc rirc.debug check)

if [[ "$RESULTS" == *"failure"* ]]; then
	echo -e "$RESULTS"
	exit 1
fi
