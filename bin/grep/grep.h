/*	$NetBSD: grep.h,v 1.5 2011/02/27 17:33:37 joerg Exp $	*/
/*	$OpenBSD: grep.h,v 1.15 2010/04/05 03:03:55 tedu Exp $	*/
/*	$FreeBSD: user/gabor/tre-integration/usr.bin/grep/grep.h 232109 2012-02-24 13:05:10Z gabor $	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
 * Copyright (c) 2008-2009 Gabor Kovesdan <gabor@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <limits.h>
#include <frec.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef WITHOUT_GZIP
#include <zlib.h>
#endif

#ifndef WITHOUT_BZIP2
#include <bzlib.h>
#endif

#ifdef WITHOUT_NLS
#define getstr(n)	 errstr[n]
#else
#include <nl_types.h>

extern nl_catd		 catalog;
#define getstr(n)	 catgets(catalog, 1, n, errstr[n])
#endif

#ifndef OFF_MAX
#define OFF_MAX (((((off_t) 1 << (sizeof(off_t) * 8 - 1)) - 1) << 1) + 1)
#endif

#ifndef MAP_NOCORE
#define MAP_NOCORE	0
#endif

#ifndef MAP_NOSYNC
#define MAP_NOSYNC	0
#endif

extern const char		*errstr[];

#define VERSION		"2.5.1-FreeBSD"

#define GREP_FIXED	0
#define GREP_BASIC	1
#define GREP_EXTENDED	2

#define BINFILE_BIN	0
#define BINFILE_SKIP	1
#define BINFILE_TEXT	2

#define FILE_STDIO	0
#define FILE_MMAP	1
#define FILE_GZIP	2
#define FILE_BZIP	3
#define FILE_XZ		4
#define FILE_LZMA	5

#define DIR_READ	0
#define DIR_SKIP	1
#define DIR_RECURSE	2

#define DEV_READ	0
#define DEV_SKIP	1

#define LINK_READ	0
#define LINK_EXPLICIT	1
#define LINK_SKIP	2

#define EXCL_PAT	0
#define INCL_PAT	1

#define MAX_LINE_MATCHES	32

struct file {
	int		 fd;
	bool		 binary;
};

struct str {
	off_t		 off;
	size_t		 len;
	char		*dat;
	char		*file;
	int		 line_no;
};

struct epat {
	char		*pat;
	int		 mode;
};

extern char **pats;
extern size_t *lens;

/* Flags passed to regcomp() and regexec() */
extern int	 cflags, eflags;

/* Command line flags */
extern bool	 Eflag, Fflag, Gflag, Hflag, Lflag,
		 bflag, cflag, hflag, iflag, lflag, mflag, nflag, oflag,
		 qflag, sflag, vflag, xflag;
extern bool	 dexclude, dinclude, fexclude, finclude, lbflag, nullflag;
extern unsigned long long Aflag, Bflag;
extern long long mcount;
extern char	*label;
extern const char *color;
extern int	 binbehave, devbehave, dirbehave, filebehave, grepbehave, linkbehave;

extern bool	 file_err, first, matchall, prev;
extern int	 tail;
extern unsigned int dpatterns, fpatterns, patterns;
extern struct pat *pattern;
extern struct epat *dpattern, *fpattern;
extern mfrec_t preg;

/* For regex errors  */
#define RE_ERROR_BUF	512
extern char	 re_error[RE_ERROR_BUF + 1];	/* Seems big enough */

/* util.c */
bool	 file_matching(const char *fname);
int	 procfile(const char *fn);
int	 grep_tree(char **argv);
void	*grep_malloc(size_t size);
void	*grep_calloc(size_t nmemb, size_t size);
void	*grep_realloc(void *ptr, size_t size);
char	*grep_strdup(const char *str);
void	 printline(struct str *line, int sep, frec_match_t *matches, int m);

/* queue.c */
void	 enqueue(struct str *x);
void	 printqueue(void);
void	 clearqueue(void);

/* file.c */
void		 grep_close(struct file *f);
struct file	*grep_open(const char *path);
char		*grep_fgetln(struct file *f, size_t *len);
