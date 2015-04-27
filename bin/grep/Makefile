#	$NetBSD: Makefile,v 1.4 2011/02/16 01:31:33 joerg Exp $
#	$FreeBSD: user/gabor/tre-integration/usr.bin/grep/Makefile 232109 2012-02-24 13:05:10Z gabor $
#	$OpenBSD: Makefile,v 1.6 2003/06/25 15:00:04 millert Exp $

PROG=	grep
SRCS=	file.c grep.c queue.c util.c

CC=	clang
WARNS?=	6
CFLAGS+=-I${.CURDIR}/../../lib -I/usr/local/include -ggdb
LDFLAGS+=-L${.CURDIR}/../../lib

LDADD=	-lz -llzma -lfrec
DPADD=	${LIBZ} ${LIBLZMA}

.if !defined(WITHOUT_BZIP2)
LDADD+=	-lbz2
DPADD+=	${LIBBZ2}
.else
CFLAGS+= -DWITHOUT_BZIP2
.endif

.if !defined(WITHOUT_NLS)
.include "${.CURDIR}/nls/Makefile.inc"
.else
CFLAGS+= -DWITHOUT_NLS
.endif

.include <bsd.prog.mk>
