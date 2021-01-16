#!/bin/bash

set -e

CDIR="coverage"

export CC=gcc
export CC_EXT="-fprofile-arcs -ftest-coverage -fprofile-abs-path"
export LD_EXT="-fprofile-arcs"

export MAKEFLAGS="-e -j $(nproc)"

rm -rf $CDIR && mkdir -p $CDIR

make clean
make check

GCNO=$(find bld -name '*.t.gcno')

FILTER=$(cat << 'EOF'
{
	use Cwd;
	@results;
	@result_ds;
	@result_fs;
	if (eof()) {
		print "- Coverage:";
		print "- ", "=" x 40;
		print "- $_", for sort(@result_fs);
		print "- $_", for sort(@result_ds);
		print "- ", "=" x 40;
		printf("- Total %20d/%d  %6.2f%%\n", $lc, $lt, (($lc / $lt) * 100.0));
	} elsif ($p) {
		chomp $file;
		chomp $_;
		$file =~ s/'//g;
		$file = substr($file, (length(getcwd()) + 6));
		@s1 = split /:/, $_;
		@s2 = split / /, $s1[1];
		$lt = $lt + $s2[2];
		$lc = $lc + $s2[2] * ($s2[0] / 100.0);
		$result = sprintf("%-25s  %4s  %7s", $file, $s2[2], $s2[0]);
		if ($file =~ m|src/.*/.*|) {
			push @result_fs, $result;
		} else {
			push @result_ds, $result;
		}
		$p = 0;
	}
	$p++ if /^File.*src.*c'/;
	$file = $_;
}
EOF
)

gcov --preserve-paths $GCNO | perl -lne "$FILTER"

mv *.gcov $CDIR

if [ -x "$(command -v gcovr)" ]; then
	gcovr -r . --html --html-details --filter ".*src.*c$" -o $CDIR/index.html
fi
