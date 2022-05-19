bin_PROGRAMS=grep
grep_SOURCES=file.c grep.c queue.c util.c
grep_CPPFLAGS=-I../../libfrec/include
grep_LDADD=-lfrec -ltre
grep_LDFLAGS=-L../../libfrec/lib

if WITH_GZIP
grep_LDADD+=-lz
else
grep_CPPFLAGS+=-DWITHOUT_GZIP
endif

if WITH_BZIP2
grep_LDADD+=-lbz2
else
grep_CPPFLAGS+=-DWITHOUT_BZIP2
endif

if WITH_LZMA
grep_LDADD+=-llzma
else
grep_CPPFLAGS+=-DWITHOUT_LZMA
endif

if !HAVE_FGETLN
grep_SOURCES+=fgetln.c
grep_CPPFLAGS+=-DWITHOUT_FGETLN
endif

if HAVE_GETPROGNAME
grep_CPPFLAGS+=-DHAVE_GETPROGNAME
endif

AM_CFLAGS=-ggdb
