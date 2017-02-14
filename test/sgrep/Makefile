#	$FreeBSD$

PROG=	sgrep
SRCS=	sgrep.c

NO_MAN=	yes

CFLAGS+= -DREG_MULTI
LDADD=	-lfrec

WARNS?=	6
CFLAGS+=-I../../lib/libfrec -I/usr/local/include -ggdb
LDFLAGS+=-L../../lib/libfrec

.include <bsd.prog.mk>
