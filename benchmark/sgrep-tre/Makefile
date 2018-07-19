#	$FreeBSD$

.PATH: ${.CURDIR}/../sgrep

PROG=	sgrep-tre
SRCS=	sgrep.c

NO_MAN=	yes

LDADD=	-ltre
CFLAGS+=-DREG_TRE

WARNS?=	6
CFLAGS+=-I../../lib -I/usr/local/include
LDFLAGS+=-L../../lib -L/usr/local/lib

.include <bsd.prog.mk>
