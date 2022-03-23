lib_LIBRARIES=libfrec.a
libfrec_a_SOURCES = boyer-moore-exec.c boyer-moore-prep.c \
                    compile.c convert.c hashtable.c heuristic.c \
                    interface.c match-utils.c match.c matchold.c \
                    regex-parser.c type-unification.c wu-manber.c
libfrec_a_CPPFLAGS=-I/usr/local/include -I../include
AM_LDFLAGS=-L/usr/local/lib -ltre
AM_CFLAGS=-ggdb
