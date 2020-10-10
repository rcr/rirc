#!/bin/bash

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
	if (eof()) {
		$cov = ($lc / $lt) * 100.0;
		printf("~\n");
		printf("~ total %21d/%d %7.2f%%\n", $lc, $lt, $cov);
	} elsif ($p) {
		chomp $file;
		chomp $_;
		$file =~ s/'//g;
		my @s1 = split / /, $file;
		my @s2 = split /:/, $_;
		my @s3 = split / /, $s2[1];
		chop($s3[0]);
		printf("%-30s%4s %7s%%\n", $s1[1], $s3[2], $s3[0]);
		$lt = $lt + $s3[2];
		$lc = $lc + $s3[2] * ($s3[0] / 100.0);
		$p = 0;
	}
	$p++ if /^File.*src.*c'/;
	$file = $_;
}
EOF
)

echo "~ Coverage:"

gcov -pr $CDIR/*.gcno | perl -ne "$FILTER" | sort

find . -name "*gperf*.gcov" -print0 | xargs -0 -I % rm %
find . -name "*test#*.gcov" -print0 | xargs -0 -I % rm %

if [ -x "$(command -v gcovr)" ]; then
	gcovr -r . --html --html-details --filter "src.*c$" -o $CDIR/index.html
fi
