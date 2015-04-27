/*-
 * Copyright (C) 2012 Gabor Kovesdan <gabor@FreeBSD.org>
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

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "config.h"
#include "frec.h"

static int	fastcmp(const fastmatch_t *fg, const void *data,
		    int type);

/*
 * Clean up if pattern compilation fails.
 */
#define FAIL_COMP(errcode)						\
do {									\
	if (fg->pattern)						\
		free(fg->pattern);					\
	if (fg->wpattern)						\
		free(fg->wpattern);					\
	if (fg->qsBc_table)						\
		hashtable_free(fg->qsBc_table);				\
	fg = NULL;							\
	return (errcode);						\
} while(0);

/*
 * Skips n characters in the input string and assigns the start
 * address to startptr. Note: as per IEEE Std 1003.1-2008
 * matching is based on bit pattern not character representations
 * so we can handle MB strings as byte sequences just like
 * SB strings.
 */
#define SKIP_CHARS(n)							\
switch (type) {								\
case STR_WIDE:								\
	startptr = str_wide + n;					\
	break;								\
default:								\
	startptr = str_byte + n;					\
}

/*
 * Converts the wide string pattern to SB/MB string and stores
 * it in fg->pattern. Sets fg->len to the byte length of the
 * converted string.
 */
#define STORE_MBS_PAT							\
do {									\
	size_t siz;							\
									\
	siz = wcstombs(NULL, fg->wpattern, 0);				\
	if (siz == (size_t)-1)						\
		return (REG_BADPAT);					\
	fg->len = siz;							\
	fg->pattern = malloc(siz + 1);					\
	if (fg->pattern == NULL)					\
		return (REG_ESPACE);					\
	wcstombs(fg->pattern, fg->wpattern, siz);			\
	fg->pattern[siz] = '\0';					\
} while (0);

#define IS_OUT_OF_BOUNDS						\
((type == STR_WIDE) ? ((j + fg->wlen) > len) : ((j + fg->len) > len))

/*
 * Checks whether the new position after shifting in the input string
 * is out of the bounds and break out from the loop if so.
 */
#define CHECKBOUNDS							\
if (IS_OUT_OF_BOUNDS)							\
	break;

/*
 * Shifts in the input string after a mismatch. The position of the
 * mismatch is stored in the mismatch variable.
 */
#define SHIFT								\
CHECKBOUNDS;								\
									\
{									\
	int r = -1;							\
	unsigned int bc = 0, gs = 0, ts;				\
									\
	switch (type)							\
	{								\
	case STR_WIDE:							\
		if (u != 0 && (unsigned)mismatch == fg->wlen - 1 - shift)	\
		    mismatch -= u;					\
		v = fg->wlen - 1 - mismatch;				\
		r = hashtable_get(fg->qsBc_table,			\
		    &str_wide[(size_t)j + fg->wlen], &bc);		\
		gs = fg->bmGs[mismatch];				\
		bc = (r == HASH_OK) ? bc : fg->defBc;			\
		break;							\
	default:							\
		if (u != 0 && (unsigned)mismatch == fg->len - 1 - shift)	\
		    mismatch -= u;					\
		v = fg->len - 1 - mismatch;				\
		gs = fg->sbmGs[mismatch];				\
		bc = fg->qsBc[((const unsigned char *)str_byte)		\
		    [(size_t)j + fg->len]];				\
	}								\
									\
	ts = (u >= v) ? (u - v) : 0;					\
	shift = MAX(ts, bc);						\
	shift = MAX(shift, gs);						\
	if (shift == gs)						\
		u = MIN((type == STR_WIDE ? fg->wlen : fg->len) - shift, v);	\
	else {								\
		if (ts < bc)						\
			shift = MAX(shift, u + 1);			\
		u = 0;							\
	}								\
	j = j + shift;							\
}

#define FILL_QSBC							\
for (unsigned int i = 0; i <= UCHAR_MAX; i++)				\
	fg->qsBc[i] = fg->len;						\
for (unsigned int i = 1; i < fg->len; i++) {				\
	fg->qsBc[(unsigned char)fg->pattern[i]] = fg->len - i;		\
	if (fg->icase) {						\
		char c = islower((unsigned char)fg->pattern[i]) ?	\
		    toupper((unsigned char)fg->pattern[i]) :		\
		    tolower((unsigned char)fg->pattern[i]);		\
		fg->qsBc[(unsigned char)c] = fg->len - i;		\
	}								\
}

/*
 * Fills in the bad character shifts into a hastable for wide strings.
 * With wide characters it is not possible any more to use a normal
 * array because there are too many characters and we could not
 * provide enough memory. Fortunately, we only have to store distinct
 * values for so many characters as the number of distinct characters
 * in the pattern, so we can store them in a hashtable and store a
 * default shift value for the rest.
 */

#define FILL_QSBC_WIDE							\
fg->defBc = fg->wlen;							\
									\
/* Preprocess pattern. */						\
fg->qsBc_table = hashtable_init(fg->wlen * (fg->icase ? 8 : 4),		\
    sizeof(wchar_t), sizeof(int));					\
if (!fg->qsBc_table)							\
	FAIL_COMP(REG_ESPACE);						\
for (unsigned int i = 1; i < fg->wlen; i++) {				\
	int k = fg->wlen - i;						\
	int r;								\
									\
	r = hashtable_put(fg->qsBc_table, &fg->wpattern[i], &k);	\
	if ((r == HASH_FAIL) || (r == HASH_FULL))			\
		FAIL_COMP(REG_ESPACE);					\
	if (fg->icase) {						\
		wchar_t wc = iswlower(fg->wpattern[i]) ?		\
		    towupper(fg->wpattern[i]) : towlower(fg->wpattern[i]);	\
		r = hashtable_put(fg->qsBc_table, &wc, &k);		\
		if ((r == HASH_FAIL) || (r == HASH_FULL))		\
		    FAIL_COMP(REG_ESPACE);				\
	}								\
}

/*
 * Fills in the good suffix table for SB/MB strings.
 */
#define FILL_BMGS							\
do {									\
	fg->sbmGs = malloc(fg->len * sizeof(int));			\
	if (!fg->sbmGs)							\
		return (REG_ESPACE);					\
	if (fg->len == 1)						\
		fg->sbmGs[0] = 1;					\
	else								\
		_FILL_BMGS(fg->sbmGs, fg->pattern, fg->len, false);	\
} while (0);

/*
 * Fills in the good suffix table for wide strings.
 */
#define FILL_BMGS_WIDE							\
do {									\
	fg->bmGs = malloc(fg->wlen * sizeof(int));			\
	if (!fg->bmGs)							\
		return (REG_ESPACE);					\
	if (fg->wlen == 1)						\
		fg->bmGs[0] = 1;					\
	else								\
		_FILL_BMGS(fg->bmGs, fg->wpattern, fg->wlen, true);	\
} while (0);

#define _FILL_BMGS(arr, pat, plen, wide)				\
do {									\
	char *p;							\
	wchar_t *wp;							\
									\
	if (wide) {							\
		if (fg->icase) {					\
			wp = malloc(plen * sizeof(wchar_t));		\
			if (wp == NULL)					\
				return (REG_ESPACE);			\
			for (unsigned int i = 0; i < plen; i++)		\
				wp[i] = towlower(pat[i]);		\
			_CALC_BMGS(arr, wp, plen);			\
			free(wp);					\
		} else							\
			_CALC_BMGS(arr, pat, plen);			\
	} else {							\
		if (fg->icase) {					\
			p = malloc(plen);				\
		if (p == NULL)						\
			return (REG_ESPACE);				\
		for (unsigned int i = 0; i < plen; i++)			\
			p[i] = tolower(pat[i]);				\
		_CALC_BMGS(arr, p, plen);				\
		free(p);						\
		} else							\
			_CALC_BMGS(arr, pat, plen);			\
	}								\
} while (0);

#define _CALC_BMGS(arr, pat, plen)					\
do {									\
	int f = 0, g;							\
									\
	int *suff = malloc(plen * sizeof(int));				\
	if (suff == NULL)						\
		return (REG_ESPACE);					\
									\
	suff[plen - 1] = plen;						\
	g = plen - 1;							\
	for (int i = plen - 2; i >= 0; i--) {				\
		if (i > g && suff[i + plen - 1 - f] < i - g)		\
			suff[i] = suff[i + plen - 1 - f];		\
		else {							\
			if (i < g)					\
				g = i;					\
			f = i;						\
			while (g >= 0 && pat[g] == pat[g + plen - 1 - f])	\
				g--;					\
			suff[i] = f - g;				\
		}							\
	}								\
									\
	for (unsigned int i = 0; i < plen; i++)				\
		arr[i] = plen;						\
	g = 0;								\
	for (int i = plen - 1; i >= 0; i--)				\
		if (suff[i] == i + 1)					\
	for(; (unsigned long)g < plen - 1 - i; g++)			\
		if (arr[g] == plen)					\
			arr[g] = plen - 1 - i;				\
	for (unsigned int i = 0; i <= plen - 2; i++)			\
		arr[plen - 1 - suff[i]] = plen - 1 - i;			\
									\
    free(suff);								\
} while(0);

/*
 * Copies the pattern pat having lenght n to p and stores
 * the size in l.
 */
#define SAVE_PATTERN(src, srclen, dst, dstlen, l)			\
dstlen = srclen;							\
dst = malloc((dstlen + 1) * sizeof(l));					\
if (dst == NULL)							\
	return (REG_ESPACE);						\
if (dstlen > 0)								\
	memcpy(dst, src, dstlen * sizeof(l));				\
dst[dstlen] = L'\0';

/*
 * Initializes pattern compiling.
 */
#define INIT_COMP							\
/* Initialize. */							\
memset(fg, 0, sizeof(*fg));						\
fg->icase = (cflags & REG_ICASE);					\
fg->word = (cflags & REG_WORD);						\
fg->newline = (cflags & REG_NEWLINE);					\
fg->nosub = (cflags & REG_NOSUB);					\
									\
/* Cannot handle REG_ICASE with MB string */				\
if (fg->icase && (MB_CUR_MAX > 1) && n > 0)				\
	return (REG_BADPAT);

/*
 * Checks whether we have a 0-length pattern that will match
 * anything. If literal is set to false, the EOL anchor is also
 * taken into account.
 */
#define CHECK_MATCHALL(literal)						\
if (!literal && n == 1 && pat[0] == L'$') {				\
	n--;								\
	fg->eol = true;							\
}									\
									\
if (n == 0) {								\
	fg->matchall = true;						\
	fg->pattern = malloc(sizeof(char));				\
	if (!fg->pattern)						\
		FAIL_COMP(REG_ESPACE);					\
	fg->pattern[0] = '\0';						\
	fg->wpattern = malloc(sizeof(wchar_t));				\
	if (!fg->wpattern)						\
		FAIL_COMP(REG_ESPACE);					\
	fg->wpattern[0] = L'\0';					\
	return (REG_OK);						\
}

/*
 * Returns: REG_OK on success, error code otherwise
 */
int
frec_proc_literal(fastmatch_t *fg, const wchar_t *wpat, size_t wn,
    const char *pat, size_t n, int cflags)
{

	DEBUG_PRINT("enter");

	INIT_COMP;
	CHECK_MATCHALL(true);

	/* Cannot handle word boundaries with MB string */
	if (fg->word && (MB_CUR_MAX > 1)) {
		DEBUG_PRINT("cannot handle MBS with REG_WORD, failing...");
		return (REG_BADPAT);
	}

	SAVE_PATTERN(wpat, wn, fg->wpattern, fg->wlen, wchar_t);
	SAVE_PATTERN(pat, n, fg->pattern, fg->len, char);


	FILL_QSBC;
	FILL_BMGS;
	FILL_QSBC_WIDE;
	FILL_BMGS_WIDE;

	DEBUG_PRINTF("compiled pattern %s", pat);

	return (REG_OK);
}

/*
 * Returns: REG_OK on success, error code otherwise
 */
int
frec_proc_fast(fastmatch_t *fg, const wchar_t *wpat, size_t wn,
    const char *pat, size_t n, int cflags)
{
	wchar_t *tmp;
	size_t pos = 0;
	bool escaped = false;

	DEBUG_PRINT("enter");

	INIT_COMP;

	/* Remove beginning-of-line character ('^'). */
	if (wpat[0] == L'^') {
		fg->bol = true;
		wn--;
		wpat++;
	}

	CHECK_MATCHALL(false);

	/* Handle word-boundary matching when GNU extensions are enabled */
	if ((cflags & REG_GNU) && (wn >= 14) &&
	    (memcmp(wpat, L"[[:<:]]", 7 * sizeof(wchar_t)) == 0) &&
	    (memcmp(wpat + wn - 7, L"[[:>:]]",
	    7 * sizeof(wchar_t)) == 0)) {
		wn -= 14;
		wpat += 7;
		fg->word = true;
	}

	/* Cannot handle word boundaries with MB string */
	if (fg->word && (MB_CUR_MAX > 1)) {
		DEBUG_PRINT("cannot handle MBS with REG_WORD, failing...");
		return (REG_BADPAT);
	}

	tmp = malloc((wn + 1) * sizeof(wchar_t));
	if (tmp == NULL)
		return (REG_ESPACE);

/* Copies the char into the stored pattern and skips to the next char. */
#define STORE_CHAR							\
	do {								\
		tmp[pos++] = wpat[i];					\
		escaped = false;					\
		continue;						\
	} while (0)

	/* Traverse the input pattern for processing */
	for (unsigned int i = 0; i < wn; i++) {
		switch (wpat[i]) {
		case L'\\':
			if (escaped)
				STORE_CHAR;
			else
				escaped = true;
			continue;
		case L'[':
			if (escaped)
				STORE_CHAR;
			else
				goto badpat;
			continue;
		case L'*':
			if (escaped || (!(cflags & REG_EXTENDED) && (i == 0)))
				STORE_CHAR;
			else
				goto badpat;
			continue;
		case L'+':
		case L'?':
			if ((cflags & REG_EXTENDED) && (i == 0))
				continue;
			else if ((cflags & REG_EXTENDED) ^ !escaped)
				STORE_CHAR;
			else
				goto badpat;
			continue;
		case L'.':
			if (escaped)
				STORE_CHAR;
			else
				goto badpat;
			continue;
		case L'^':
			STORE_CHAR;
			continue;
		case L'$':
			if (!escaped && (i == n - 1))
				fg->eol = true;
			else
				STORE_CHAR;
			continue;
		case L'(':
			if ((cflags & REG_EXTENDED) ^ escaped)
				goto badpat;
			else
				STORE_CHAR;
			continue;
		case L'{':
			if (!(cflags & REG_EXTENDED) ^ escaped)
				STORE_CHAR;
			else if (i == 0)
				STORE_CHAR;
		else
			goto badpat;
			continue;
		case L'|':
			if ((cflags & REG_EXTENDED) ^ escaped)
				goto badpat;
			else
				STORE_CHAR;
			continue;
		default:
			if (escaped)
				goto badpat;
			else
				STORE_CHAR;
			continue;
		}
		continue;
badpat:
		free(tmp);
		return (REG_BADPAT);
	}

	/*
	 * The pattern has been processed and copied to tmp as a literal string
	 * with escapes, anchors (^$) and the word boundary match character
	 * classes stripped out.
	 */
	DEBUG_PRINTF("compiled literal pattern %ls", tmp);
	SAVE_PATTERN(tmp, pos, fg->wpattern, fg->wlen, wchar_t);

	/* Convert back to MBS instead of processing again */
	STORE_MBS_PAT;

	free(tmp);

	FILL_QSBC;
	FILL_BMGS;
	FILL_QSBC_WIDE;
	FILL_BMGS_WIDE;

	return (REG_OK);
}

#define _SHIFT_ONE							\
do {									\
	shift = 1;							\
	j = j + shift;							\
	continue;							\
} while(0);

#define _BOL_COND							\
((j == 0) || ((type == STR_WIDE) ? (str_wide[j - 1] == L'\n')		\
    : (str_byte[j - 1] == '\n')))

/*
 * Checks BOL anchor and shifts one if match is not on a
 * boundary.
 */
#define CHECK_BOL_ANCHOR						\
if (!_BOL_COND)								\
	_SHIFT_ONE;

#define _EOL_COND							\
((type == STR_WIDE)							\
    ? ((j + fg->wlen == len) ||						\
	(str_wide[j + fg->wlen] == L'\n'))				\
    : ((j + fg->len == len) || (str_byte[j + fg->wlen] == '\n')))

/*
 * Checks EOL anchor and shifts one if match is not on a
 * boundary.
 */
#define CHECK_EOL_ANCHOR						\
if (!_EOL_COND)								\
	_SHIFT_ONE;


/*
 * Executes matching of the precompiled pattern on the input string.
 * Returns REG_OK or REG_NOMATCH depending on if we find a match or not.
 */
int
frec_match_fast(const fastmatch_t *fg, const void *data, size_t len,
    int type, int nmatch, frec_match_t pmatch[], int eflags)
{
	unsigned int shift, u = 0, v = 0;
	ssize_t j = 0;
	int ret = REG_NOMATCH;
	int mismatch;
	const char *str_byte = data;
	const void *startptr = NULL;
	const wchar_t *str_wide = data;

	DEBUG_PRINT("enter");
	#define FORMAT_BOOL(b) ((b) ? ('y') : ('n'))
	DEBUG_PRINTF("matchall: %c, nosub: %c, bol: %c, eol: %c",
		FORMAT_BOOL(fg->matchall), FORMAT_BOOL(fg->nosub),
		FORMAT_BOOL(fg->bol), FORMAT_BOOL(fg->eol));

	/* Calculate length if unspecified. */
	if (len == (size_t)-1)
		switch (type) {
		case STR_WIDE:
			len = wcslen(str_wide);
			break;
		default:
			len = strlen(str_byte);
			break;
		}

	// XXX: EOL/BOL
	/* Shortcut for empty pattern */
	if (fg->matchall) {
		if (!fg->nosub && nmatch >= 1) {
			pmatch[0].m.rm_so = 0;
			pmatch[0].m.rm_eo = len;
		}		
		DEBUG_PRINT("matchall shortcut");
		return (REG_OK);
	}

	/* No point in going farther if we do not have enough data. */
	switch (type) {
	case STR_WIDE:
		if (len < fg->wlen) {
			DEBUG_PRINT("too few data remained");
			return (ret);
		}
		shift = fg->wlen;
		break;
	default:
		if (len < fg->len) {
			DEBUG_PRINT("too few data remained");
			return (ret);
		}
		shift = fg->len;
	}

	/*
	 * REG_NOTBOL means not anchoring ^ to the beginning of the line, so we
	 * can shift one because there can't be a match at the beginning.
	 */
	if (fg->bol && (eflags & REG_NOTBOL))
		j = 1;

	/*
	 * Like above, we cannot have a match at the very end when anchoring to
	 * the end and REG_NOTEOL is specified.
	 */
	if (fg->eol && (eflags & REG_NOTEOL))
		len--;

	/* Only try once at the beginning or ending of the line. */
	if ((fg->bol || fg->eol) && !fg->newline && !(eflags & REG_NOTBOL) &&
	    !(eflags & REG_NOTEOL)) {
		/* Simple text comparison. */
		if (!((fg->bol && fg->eol) &&
		    (type == STR_WIDE ? (len != fg->wlen) : (len != fg->len)))) {
			/* Determine where in data to start search at. */
			j = fg->eol ? len - (type == STR_WIDE ? fg->wlen : fg->len) : 0;
			SKIP_CHARS(j);
			mismatch = fastcmp(fg, startptr, type);
			if (mismatch == REG_OK) {
				if (!fg->nosub && nmatch >= 1) {
					pmatch[0].m.rm_so = j;
					pmatch[0].m.rm_eo = j + (type == STR_WIDE ?
					    fg->wlen : fg->len);
				}
				return (REG_OK);
			}
		}
	} else {
		/* Quick Search / Turbo Boyer-Moore algorithm. */
		do {
			SKIP_CHARS(j);
			mismatch = fastcmp(fg, startptr, type);
			if (mismatch == REG_OK) {
				if (fg->bol)
					CHECK_BOL_ANCHOR;
				if (fg->eol)
					CHECK_EOL_ANCHOR;
				if (!fg->nosub && nmatch >= 1) {
					pmatch[0].m.rm_so = j;
					pmatch[0].m.rm_eo = j + ((type == STR_WIDE) ?
					    fg->wlen : fg->len);
				}
				return (REG_OK);
			} else if (mismatch > 0)
				return (mismatch);
			mismatch = -mismatch - 1;
			SHIFT;
		} while (!IS_OUT_OF_BOUNDS);
	}
	return (ret);
}

/*
 * Frees the resources that were allocated when the pattern was compiled.
 */
void
frec_free_fast(fastmatch_t *fg)
{

	DEBUG_PRINT("enter");

	hashtable_free(fg->qsBc_table);
	free(fg->bmGs);
	free(fg->wpattern);
	free(fg->sbmGs);
	free(fg->pattern);
}

/*
 * Returns:	-(i + 1) on failure (position that it failed with minus sign)
 *		error code on error
 *		REG_OK on success
 */

static inline int
fastcmp(const fastmatch_t *fg, const void *data, int type)
{
	const char *str_byte = data;
	const char *pat_byte = fg->pattern;
	const wchar_t *str_wide = data;
	const wchar_t *pat_wide = fg->wpattern;
	size_t len = (type == STR_WIDE) ? fg->wlen : fg->len;
	int ret = REG_OK;

	/* Compare the pattern and the input char-by-char from the last position. */
	for (int i = len - 1; i >= 0; i--) {
		switch (type) {
		case STR_WIDE:
			/* Compare */
			if (fg->icase ? (towlower(pat_wide[i]) == towlower(str_wide[i]))
			    : (pat_wide[i] == str_wide[i]))
				continue;
			break;
		default:
			/* Compare */
			if (fg->icase ? (tolower(pat_byte[i]) == tolower(str_byte[i]))
			    : (pat_byte[i] == str_byte[i]))
			continue;
		}
		ret = -(i + 1);
		break;
	}
	return (ret);
}
