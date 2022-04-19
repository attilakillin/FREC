#!/bin/sh

make_executables() {
    echo "> Cleaning and rebuilding executables...";
    (cd ./wrappers && (make clean && make all) > /dev/null);
}

time_run() {
    st_date=`date +"%s"`;
    ./wrappers/grep-$1 -e $2 $3 > /dev/null;
    en_date=`date +"%s"`;

    diff=$(($en_date-$st_date));
	mins=$(($diff / 60));
	secs=$(($diff % 60));
	echo "  $1 - ${mins}m${secs}s";
}

benchmark_all() {
    echo "> Running benchmarks: | $1 | $2 |";

    time_run frec $1 $2;
    time_run posix $1 $2;
    time_run tre $1 $2;
}

make_executables;

benchmark_all "start" ./wrappers/grep-single.c;
