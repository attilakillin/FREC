lib_LIBRARIES=libfrec.a
libfrec_a_SOURCES = bm-comp.c bm-exec.c bm-type.c \
                    compile.c convert.c hashtable.c heuristic.c \
                    interface.c match-utils.c match.c matchold.c \
                    regex-parser.c string-type.c wu-manber.c
libfrec_a_CPPFLAGS=-I/usr/local/include -I../include
AM_LDFLAGS=-L/usr/local/lib -ltre
AM_CFLAGS=-ggdb
