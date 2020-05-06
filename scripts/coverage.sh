#!/bin/sh

set -e

CDIR="coverage"

export CC=gcc
export CC_EXT="-fprofile-arcs -ftest-coverage"
export LD_EXT="-fprofile-arcs"

make -e clean test

rm -rf $CDIR
mkdir -p $CDIR

find . -name "*.gcno" -print0 | xargs -0 -I % mv % $CDIR
find . -name "*.gcda" -print0 | xargs -0 -I % mv % $CDIR

FILTER=$(cat <<'EOF'
{
	if ($p) {
		chomp $file;
		chomp $_;
		my @s1 = split / /, $file;
		my @s2 = split /:/, $_;
		my @s3 = split / /, $s2[1];
		printf("%-28s%5s: %7s\n", $s1[1], $s3[2], $s3[0]);
		$p = 0;
	}
	$p++ if /^File.*src/;
	$file = $_;
}
EOF
)

gcov -pr $CDIR/*.gcno | perl -ne "$FILTER" | grep -v 'gperf' | sort

find . -name "*gperf*.gcov" -print0 | xargs -0 -I % rm %
find . -name "*test#*.gcov" -print0 | xargs -0 -I % rm %
