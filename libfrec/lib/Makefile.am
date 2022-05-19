lib_LIBRARIES=libfrec.a
libfrec_a_SOURCES = bm-comp.c bm-exec.c bm-type.c \
                    compile.c hashtable.c heuristic.c \
                    interface.c interface-types.c match-utils.c match.c \
                    regex-parser.c string-type.c wm-comp.c wm-type.c
libfrec_a_CPPFLAGS=-I/usr/local/include -I../include
AM_LDFLAGS=-L/usr/local/lib -ltre
AM_CFLAGS=-ggdb
