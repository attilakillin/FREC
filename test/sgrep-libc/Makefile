#	$FreeBSD$

.PATH: ${.CURDIR}/../sgrep

PROG=	sgrep-libc
SRCS=	sgrep.c

NO_MAN=	yes

CFLAGS+=-DREG_POSIX

WARNS?=	6
CFLAGS+=-I../../lib -I/usr/local/include
LDFLAGS+=-L../../lib

.include <bsd.prog.mk>
