#!/bin/sh

rm -f work/*.out

env LD_LIBRARY_PATH=../lib/libfrec ./sgrep/sgrep -e $1 work/hugefile >> work/sgrep.out
./sgrep-libc/sgrep-libc -e $1 work/hugefile >> work/sgrep-libc.out
./sgrep-tre/sgrep-tre -e $1 work/hugefile >> work/sgrep-tre.out

cmp work/sgrep.out work/sgrep-libc.out || $1 produces different output
cmp work/sgrep.out work/sgrep-tre.out || $1 produces different output
