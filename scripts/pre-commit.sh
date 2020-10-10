#!/bin/bash
#
# pre-commit testing hook, setup:
#   ln -s ../../scripts/pre-commit.sh .git/hooks/pre-commit

echo "Running pre-commit hook..."

RESULTS=$(make test)

if [[ "$RESULTS" == *"failure"* ]];
then
	echo -e "$RESULTS"
	exit 1
fi
