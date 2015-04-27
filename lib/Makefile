SHLIB=	frec
SHLIB_MAJOR= 1

CC=	clang
WARNS?=	6

NO_PROFILE=

SRCS+=boyer-moore.c
SRCS+=compile.c
SRCS+=convert.c
SRCS+=hashtable.c
SRCS+=heuristic.c
SRCS+=interface.c
SRCS+=match.c
SRCS+=wu-manber.c

LDADD=	-ltre
DPADD=	/usr/local/lib/libtre.a

CFLAGS+= -I/usr/local/include
#CFLAGS+= -DWITH_DEBUG
LDFLAGS+= -L/usr/local/lib

.include <bsd.lib.mk>
