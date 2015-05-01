/* 	$FreeBSD$	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
 * Copyright (C) 2015 Gabor Kovesdan <gabor@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <libgen.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef REG_POSIX
#ifndef REG_OK
#define REG_OK 0
#endif
#include <regex.h>
#endif

#ifdef REG_MULTI
#include <mregex.h>
#endif 

#ifdef REG_TRE
#include <tre/regex.h>
#endif

#define MAX_PATTERNS 256

/* Flags passed to regcomp() and regexec() */
static int		 cflags = REG_EXTENDED;
#ifndef REG_TRE
static int		 eflags = REG_STARTEND;
#else
static int		eflags = 0;
#endif

/* Searching patterns */
static unsigned int 	 patterns;
static char		**pats;
static size_t		*lens;
#ifdef REG_MULTI
static mregex_t	 preg;
#else
static regex_t  preg;
#endif

/* For regex errors  */
#define RE_ERROR_BUF	512
static char	 re_error[RE_ERROR_BUF + 1];

/*
 * Safe malloc() for internal use.
 */
static void *
grep_malloc(size_t size)
{
	void *ptr;

	if ((ptr = malloc(size)) == NULL)
		err(2, "malloc");
	return (ptr);
}

/*
 * Prints usage information and returns 2.
 */
static void
usage(void)
{
	printf("sgrep -e pattern input\n");
	exit(2);
}

static const char	*optstr = "e:l";


/*
 * Adds a searching pattern to the internal array.
 */
static void
add_pattern(char *pat, size_t len)
{

	if (len > 0 && pat[len - 1] == '\n')
		--len;
	/* pat may not be NUL-terminated */
	pats[patterns] = grep_malloc(len + 1);
	memcpy(pats[patterns], pat, len);
	pats[patterns][len] = '\0';
	lens[patterns] = len;
	++patterns;
}

int
main(int argc, char *argv[])
{
	const char **ptr;
	int c = 0;

	setlocale(LC_ALL, "");

	pats = grep_malloc(MAX_PATTERNS * sizeof(char *));
	lens = grep_malloc(MAX_PATTERNS * sizeof(size_t));

	int needpattern = 1;

	while (((c = getopt(argc, argv, optstr)) != -1)) {
		switch (c) {
		case 'e':
			add_pattern(optarg, strlen(optarg));
			needpattern = 0;
			break;
		case 'l':
			cflags |= REG_NEWLINE;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (needpattern || (argc == 0))
		usage();

	/* Compile patterns. */
	ptr = (const char **)pats;
#ifdef REG_MULTI
	c = frec_mregncomp(&preg, patterns, ptr, lens, cflags);
#endif

#ifdef REG_POSIX
	c = regcomp(&preg, pats[0], cflags);
#endif

#ifdef REG_TRE
	c = regncomp(&preg, pats[0], lens[0], cflags);
#endif
	if (c != 0) {
#ifdef REG_MULTI
		int no;

		frec_mregerror(c, &preg, &no, re_error, RE_ERROR_BUF);
		errx(2, "%s:%s", pats[no], re_error);
#else
		regerror(c, &preg, re_error, RE_ERROR_BUF);
		errx(2, "%s:%s", pats[0], re_error);
#endif
	}

	struct stat st;
	unsigned char *buffer;
	unsigned char *bufpos;
	size_t bufrem;

	if ((stat(*argv, &st) == -1) || (st.st_size > OFF_MAX) ||
		(!S_ISREG(st.st_mode))) {
                       return 2;
	} else {
#define MAX_MATCHES 1024
#ifdef REG_MULTI
		frec_match_t pmatch;
#else
		regmatch_t pmatch;
#endif
		int r;
		int flags = MAP_PRIVATE | MAP_NOCORE | MAP_NOSYNC;
		int fd;

		fd = open(*argv, O_RDONLY);
		if (fd < 0)
			errx(2, NULL);

		buffer = mmap(NULL, st.st_size, PROT_READ, flags,
			fd, (off_t)0);
		if (buffer == MAP_FAILED)
			errx(2, NULL);
		bufrem = st.st_size;
		bufpos = buffer;
		madvise(buffer, st.st_size, MADV_SEQUENTIAL);

		off_t so = 0;

		while (so < st.st_size) {
#ifdef REG_MULTI
			pmatch.m.rm_so = so;
			pmatch.m.rm_eo = st.st_size;
			r = frec_mregnexec(&preg, buffer, st.st_size, 1, &pmatch, eflags);
#else
			pmatch.rm_so = so;
			pmatch.rm_eo = st.st_size;

#ifdef REG_POSIX
			r = regexec(&preg, buffer, 1, &pmatch, eflags);
#endif

#ifdef REG_TRE
			r = regnexec(&preg, &buffer[pmatch.rm_so], st.st_size - pmatch.rm_so, 1, &pmatch, eflags);
#endif
#endif
			if (r == REG_NOMATCH)
				break;
			else if (r != REG_OK) {
#ifdef REG_MULTI
				frec_mregerror(r, &preg, NULL, re_error, RE_ERROR_BUF);
#else
				regerror(r, &preg, re_error, RE_ERROR_BUF);
#endif
				errx(2, "%s", re_error);
			} else {
				c++;
#ifdef REG_MULTI
				printf("(%d %d)\n", pmatch.m.rm_so, pmatch.m.rm_eo);
				so = pmatch.m.rm_eo;
#endif
#ifdef REG_POSIX
				printf("(%ld %ld)\n", pmatch.rm_so, pmatch.rm_eo);
				so = pmatch.rm_eo;
#endif
#ifdef REG_TRE
				printf("(%ld %ld)\n", so + pmatch.rm_so, so + pmatch.rm_eo);
				so += pmatch.rm_eo;
#endif
			}
		}
	}
	return !(c > 0);
}
