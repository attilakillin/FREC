#!/bin/sh

do_benchmark() {
	case $1 in
	multi)
		PROG="env LD_LIBRARY_PATH=../lib/libfrec ./sgrep/sgrep"
		;;
	libc)
		PROG="./sgrep-libc/sgrep-libc"
		;;
	tre)
		PROG="./sgrep-tre/sgrep-tre"
		;;
	esac

	date1=`date +"%s"`
	${PROG} -e $2 work/hugefile >/dev/null
	date2=`date +"%s"`
	diff=$(($date2-$date1))
	mins=$(($diff / 60))
	secs=$(($diff % 60))
	echo ${1} ${2} ${mins}m${secs}s
}

do_benchmark libc $1
do_benchmark tre $1
do_benchmark multi $1
