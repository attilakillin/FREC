#	$FreeBSD$

PROG=	sgrep
SRCS=	sgrep.c

NO_MAN=	yes

CFLAGS+= -DREG_MULTI
LDADD=	-lfrec

WARNS?=	6
CFLAGS+=-I../../libfrec/lib -I/usr/local/include -ggdb
LDFLAGS+=-L../../libfrec/lib

.include <bsd.prog.mk>
