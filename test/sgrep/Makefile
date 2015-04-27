#	$FreeBSD$

PROG=	sgrep
SRCS=	sgrep.c

NO_MAN=	yes

CFLAGS+= -DREG_MULTI
LDADD=	-lfrec

WARNS?=	6
CFLAGS+=-I../../lib -I/usr/local/include
LDFLAGS+=-L../../lib

.include <bsd.prog.mk>
