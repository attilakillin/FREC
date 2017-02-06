/*-
 * Copyright (C) 2012, 2017 Gabor Kovesdan <gabor@FreeBSD.org>
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

typedef struct bmcomp_t {
	const char *pat;
	size_t len;
	const wchar_t *wpat;
	size_t wlen;
	int cflags;

	wchar_t *tmp;
	size_t pos;
	bool escaped;

	fastmatch_t *fg;
} bmcomp_t;

/*
 * Clean up if pattern compilation fails.
 */
inline static void free_comp(bmcomp_t *state)
{

	if (state->tmp != NULL)
		free(state->tmp);
	state->tmp = NULL;
	if (state->fg->pattern != NULL)
		free(state->fg->pattern);
	if (state->fg->wpattern != NULL)
		free(state->fg->wpattern);
	if (state->fg->qsBc_table != NULL)
		hashtable_free(state->fg->qsBc_table);
	state->fg = NULL;
}

/*
 * Converts the wide string pattern to SB/MB string and stores
 * it in fg->pattern. Sets fg->len to the byte length of the
 * converted string.
 */
inline static int store_mbs_pattern(bmcomp_t *state)
{
	size_t siz;

	siz = wcstombs(NULL, state->fg->wpattern, 0);
	if (siz == (size_t)-1)
		return (REG_BADPAT);
	state->fg->len = siz;
	state->fg->pattern = malloc(siz + 1);
	if (state->fg->pattern == NULL)
		return (REG_ESPACE);
	wcstombs(state->fg->pattern, state->fg->wpattern, siz);
	state->fg->pattern[siz] = '\0';
	return (REG_OK);
}

inline static int fill_qsbc(bmcomp_t *state)
{

	for (size_t i = 0; i <= UCHAR_MAX; i++)
		state->fg->qsBc[i] = state->fg->len;
	for (size_t i = 1; i < state->fg->len; i++) {
		state->fg->qsBc[(unsigned char)state->fg->pattern[i]] = state->fg->len - i;
		if (state->fg->icase) {
			char c = islower((unsigned char)state->fg->pattern[i]) ?
			    toupper((unsigned char)state->fg->pattern[i]) :
			    tolower((unsigned char)state->fg->pattern[i]);
			state->fg->qsBc[(unsigned char)c] = state->fg->len - i;
		}
	}
	return (REG_OK);
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
inline static int fill_qsbc_wide(bmcomp_t *state)
{

	state->fg->defBc = state->fg->wlen;

	/* Preprocess pattern. */
	state->fg->qsBc_table = hashtable_init(state->fg->wlen * (state->fg->icase ? 8 : 4),
	    sizeof(wchar_t), sizeof(int));
	if (state->fg->qsBc_table == NULL) {
		free_comp(state);
		return (REG_ESPACE);
	}
	for (size_t i = 1; i < state->fg->wlen; i++) {
		int k = state->fg->wlen - i;
		int r;

		r = hashtable_put(state->fg->qsBc_table, &state->fg->wpattern[i], &k);
		if ((r == HASH_FAIL) || (r == HASH_FULL)) {
			free_comp(state);
			return (REG_ESPACE);
		}
		if (state->fg->icase) {
			wchar_t wc = iswlower(state->fg->wpattern[i]) ?
			    towupper(state->fg->wpattern[i]) : towlower(state->fg->wpattern[i]);
			r = hashtable_put(state->fg->qsBc_table, &wc, &k);
			if ((r == HASH_FAIL) || (r == HASH_FULL)) {
				free_comp(state);
				return (REG_ESPACE);
			}
		}
	}
	return (REG_OK);
}

#define _CALC_BMGS(arr, pat, plen)                                      \
do {                                                                    \
        int f = 0, g;                                                   \
                                                                        \
        int *suff = malloc(plen * sizeof(int));                         \
        if (suff == NULL)                                               \
                return (REG_ESPACE);                                    \
                                                                        \
        suff[plen - 1] = plen;                                          \
        g = plen - 1;                                                   \
        for (int i = plen - 2; i >= 0; i--) {                           \
                if (i > g && suff[i + plen - 1 - f] < i - g)            \
                        suff[i] = suff[i + plen - 1 - f];               \
                else {                                                  \
                        if (i < g)                                      \
                                g = i;                                  \
                        f = i;                                          \
                        while (g >= 0 && pat[g] == pat[g + plen - 1 - f])       \
                                g--;                                    \
                        suff[i] = f - g;                                \
                }                                                       \
        }                                                               \
                                                                        \
        for (size_t i = 0; i < plen; i++)                               \
                arr[i] = plen;                                          \
        g = 0;                                                          \
        for (int i = plen - 1; i >= 0; i--)                             \
                if (suff[i] == i + 1)                                   \
        for(; (unsigned long)g < plen - 1 - i; g++)                     \
                if (arr[g] == plen)                                     \
                        arr[g] = plen - 1 - i;                          \
        for (size_t i = 0; i <= plen - 2; i++)                          \
                arr[plen - 1 - suff[i]] = plen - 1 - i;                 \
                                                                        \
    free(suff);                                                         \
} while(0);

/*
 * Fills in the good suffix table.
 */
inline static int fill_bmgs(bmcomp_t *state, bool wide)
{
	char *p;
	wchar_t *wp;

	if (wide) {
		state->fg->sbmGs = malloc(state->fg->len * sizeof(int));
		if (state->fg->sbmGs == NULL)
			return (REG_ESPACE);
		if (state->fg->len == 1)
			state->fg->sbmGs[0] = 1;
		else if (state->fg->icase) {
			wp = malloc(state->fg->wlen * sizeof(wchar_t));
			if (wp == NULL)
				return (REG_ESPACE);
			for (size_t i = 0; i < state->fg->wlen; i++)
				wp[i] = towlower(state->pat[i]);
			_CALC_BMGS(state->fg->sbmGs, wp, state->fg->wlen);
			free(wp);
		} else
			_CALC_BMGS(state->fg->sbmGs, state->fg->wpattern, state->fg->wlen);
	} else {
		state->fg->bmGs = malloc(state->fg->wlen * sizeof(int));
		if (state->fg->bmGs == NULL)
			return (REG_ESPACE);
		if (state->fg->wlen == 1)
			state->fg->bmGs[0] = 1;
		else if (state->fg->icase) {
			p = malloc(state->fg->len);
		if (p == NULL)
			return (REG_ESPACE);
		for (size_t i = 0; i < state->fg->len; i++)
			p[i] = tolower(state->pat[i]);
		_CALC_BMGS(state->fg->bmGs, p, state->fg->len);
		free(p);
		} else
			_CALC_BMGS(state->fg->bmGs, state->fg->pattern, state->fg->len);
	}

	return (REG_OK);
}

/*
 * Initializes pattern compiling.
 */
inline static int init_comp(bmcomp_t *state, fastmatch_t *fg,
    const char *pat, size_t len, const wchar_t *wpat, size_t wlen, int cflags)
{

	memset(state, 0, sizeof(bmcomp_t));
	state->pat = pat;
	state->len = len;
	state->wpat = wpat;
	state->wlen = wlen;
	state->cflags = cflags;
	state->fg = fg;

	/* Initialize. */
	memset(state->fg, 0, sizeof(fastmatch_t));
	state->fg->icase = (cflags & REG_ICASE);
	state->fg->word = (cflags & REG_WORD);
	state->fg->newline = (cflags & REG_NEWLINE);
	state->fg->nosub = (cflags & REG_NOSUB);

	/* Cannot handle REG_ICASE with MB string */
	if (state->fg->icase && (MB_CUR_MAX > 1) && state->len > 0)
		return (REG_BADPAT);

	return (REG_OK);
}

inline static int use_pattern(bmcomp_t *state, const char *pat, size_t len)
{

	state->fg->len = len;
	state->fg->pattern = malloc((state->fg->len + 1) * sizeof(char));
	if (state->fg->pattern == NULL)
		return (REG_ESPACE);
	if (state->fg->len > 0)
		memcpy(state->fg->pattern, pat, state->fg->len * sizeof(char));
	state->fg->pattern[state->fg->len] = '\0';

	return (REG_OK);
}

inline static int use_pattern_wide(bmcomp_t *state, const wchar_t *wpat,
    size_t wlen) {

	state->fg->wlen = wlen;
	state->fg->wpattern = malloc((state->fg->wlen + 1) * sizeof(wchar_t));
	if (state->fg->wpattern == NULL)
		return (REG_ESPACE);
	if (state->fg->wlen > 0)
		memcpy(state->fg->wpattern, wpat, state->fg->wlen * sizeof(wchar_t));
	state->fg->wpattern[state->fg->wlen] = L'\0';

	return (REG_OK);
}

/*
 * Checks whether we have a 0-length pattern that will match
 * anything. If literal is set to false, the EOL anchor is also
 * taken into account.
 */
#define REG_MATCHALL -1
inline static int check_matchall(bmcomp_t *state, bool literal)
{

	if (!literal && state->len == 1 && state->pat[0] == L'$') {
		state->len--;
		state->fg->eol = true;
	}

	if (state->len == 0) {
		state->fg->matchall = true;
		state->fg->pattern = malloc(sizeof(char));
		if (state->fg->pattern == NULL) {
			free_comp(state);
			return (REG_ESPACE);
		}
		state->fg->pattern[0] = '\0';
		state->fg->wpattern = malloc(sizeof(wchar_t));
		if (state->fg->wpattern == NULL) {
			free_comp(state);
			return (REG_ESPACE);
		}
		state->fg->wpattern[0] = L'\0';
		return (REG_MATCHALL);
	}
	return (REG_OK);
}

/*
 * Returns: REG_OK on success, error code otherwise
 */
int
frec_proc_literal(fastmatch_t *fg, const wchar_t *wpat, size_t wn,
    const char *pat, size_t n, int cflags)
{
	bmcomp_t state;

	DEBUG_PRINT("enter");

	int ret = init_comp(&state, fg, pat, n, wpat, wn, cflags);
	if (ret != REG_OK)
		goto err;

	/* Return early if a matchall case detected */
	ret = check_matchall(&state, true);
	if (ret == REG_MATCHALL)
		return (REG_OK);

	else if (ret != REG_OK)
		goto err;

	/* Cannot handle word boundaries with MB string */
	if (state.fg->word && (MB_CUR_MAX > 1)) {
		DEBUG_PRINT("cannot handle MBS with REG_WORD, failing...");
		ret = REG_BADPAT;
		goto err;
	}

	ret = use_pattern(&state, state.pat, state.len);
	if (ret != REG_OK)
		goto err;
	ret = use_pattern_wide(&state, state.wpat, state.wlen);
	if (ret != REG_OK)
		goto err;

	ret = fill_qsbc(&state);
	if (ret != REG_OK)
		goto err;
	ret = fill_bmgs(&state, false);
	if (ret != REG_OK)
		goto err;
	ret = fill_qsbc_wide(&state);
	if (ret != REG_OK)
		goto err;
	ret = fill_bmgs(&state, true);
	if (ret != REG_OK)
		goto err;

	DEBUG_PRINTF("compiled pattern %s", pat);

	return (REG_OK);

err:
	free_comp(&state);
	return ret;
}

inline static void store_char(bmcomp_t *state, size_t idx)
{

	state->tmp[state->pos++] = state->wpat[idx];
	state->escaped = false;
}

/*
 * Returns: REG_OK on success, error code otherwise
 */
int
frec_proc_fast(fastmatch_t *fg, const wchar_t *wpat, size_t wn,
    const char *pat, size_t n, int cflags)
{
	bmcomp_t state;

	DEBUG_PRINT("enter");

	int ret = init_comp(&state, fg, pat, n, wpat, wn, cflags);
	if (ret != REG_OK)
		goto err;

	/* Remove beginning-of-line character ('^'). */
	if (state.wpat[0] == L'^') {
		state.fg->bol = true;
		state.wlen--;
		state.wpat++;
	}

	/* Return early if a matchall case detected */
	ret = check_matchall(&state, false);
	if (ret == REG_MATCHALL)
		return (REG_OK);

	else if (ret != REG_OK)
		goto err;

	/* Handle word-boundary matching when GNU extensions are enabled */
	if ((cflags & REG_GNU) && (state.wlen >= 14) &&
	    (memcmp(state.wpat, L"[[:<:]]", 7 * sizeof(wchar_t)) == 0) &&
	    (memcmp(state.wpat + state.wlen - 7, L"[[:>:]]",
	    7 * sizeof(wchar_t)) == 0)) {
		state.wlen -= 14;
		state.wpat += 7;
		state.fg->word = true;
	}

	/* Cannot handle word boundaries with MB string */
	if (state.fg->word && (MB_CUR_MAX > 1)) {
		DEBUG_PRINT("cannot handle MBS with REG_WORD, failing...");
		return (REG_BADPAT);
	}

	state.tmp = malloc((state.wlen + 1) * sizeof(wchar_t));
	if (state.tmp == NULL)
		return (REG_ESPACE);

	/* Traverse the input pattern for processing */
	for (size_t i = 0; i < state.wlen; i++) {
		switch (state.wpat[i]) {
		case L'\\':
			if (state.escaped)
				store_char(&state, i);
			else
				state.escaped = true;
			continue;
		case L'[':
			if (state.escaped)
				store_char(&state, i);
			else
				goto badpat;
			continue;
		case L'*':
			if (state.escaped || (!(state.cflags & REG_EXTENDED) && (i == 0)))
				store_char(&state, i);
			else
				goto badpat;
			continue;
		case L'+':
		case L'?':
			if ((state.cflags & REG_EXTENDED) && (i == 0))
				continue;
			else if ((state.cflags & REG_EXTENDED) ^ !state.escaped)
				store_char(&state, i);
			else
				goto badpat;
			continue;
		case L'.':
			if (state.escaped)
				store_char(&state, i);
			else
				goto badpat;
			continue;
		case L'^':
			store_char(&state, i);
			continue;
		case L'$':
			if (!state.escaped && (i == state.wlen - 1))
				state.fg->eol = true;
			else
				store_char(&state, i);
			continue;
		case L'(':
			if ((state.cflags & REG_EXTENDED) ^ state.escaped)
				goto badpat;
			else
				store_char(&state, i);
			continue;
		case L'{':
			if (!(state.cflags & REG_EXTENDED) ^ state.escaped)
				store_char(&state, i);
			else if (i == 0)
				store_char(&state, i);
		else
			goto badpat;
			continue;
		case L'|':
			if ((state.cflags & REG_EXTENDED) ^ state.escaped)
				goto badpat;
			else
				store_char(&state, i);
			continue;
		default:
			if (state.escaped)
				goto badpat;
			else
				store_char(&state, i);
			continue;
		}
		continue;
	}

	/*
	 * The pattern has been processed and copied to tmp as a literal string
	 * with escapes, anchors (^$) and the word boundary match character
	 * classes stripped out.
	 */
	DEBUG_PRINTF("compiled literal pattern %ls", state.tmp);

	ret = use_pattern_wide(&state, state.tmp, state.pos);
	if (ret != REG_OK)
		goto err;

	if (state.tmp != NULL)
		free(state.tmp);
	state.tmp = NULL;

	/* Convert back to MBS instead of processing again */
	ret = store_mbs_pattern(&state);
	if (ret != REG_OK)
		goto err;

	ret = fill_qsbc(&state);
	if (ret != REG_OK)
		goto err;
	ret = fill_bmgs(&state, false);
	if (ret != REG_OK)
		goto err;
	ret = fill_qsbc_wide(&state);
	if (ret != REG_OK)
		goto err;
	ret = fill_bmgs(&state, true);
	if (ret != REG_OK)
		goto err;

	return (REG_OK);

badpat:
	ret = REG_BADPAT;
err:
	free_comp(&state);
	return ret;
}

typedef struct bmexec_t {
	const fastmatch_t *fg;
	int type;
	const char *str;
	size_t strlen;
	const wchar_t *wstr;
	size_t wstrlen;
	const void *startptr;

	int mismatch;
	off_t shift;
	size_t j; //?
	unsigned u,v; //?
} bmexec_t;

inline static void init_bmexec(bmexec_t *state, const fastmatch_t *fg, int type,
    const void *data, size_t len)
{
	memset(state, 0, sizeof(bmexec_t));

	state->fg = fg;
	state->type = type;
	state->startptr = data;
	if (type == STR_WIDE) {
		state->wstr = data;
		state->wstrlen = len;
	} else {
		state->str = data;
		state->strlen = len;
	}
}

/*
 * Skips n characters in the input string and assigns the start
 * address to startptr. Note: as per IEEE Std 1003.1-2008
 * matching is based on bit pattern not character representations
 * so we can handle MB strings as byte sequences just like
 * SB strings.
 */
inline static void skip_chars(bmexec_t *state, size_t n)
{

	if (state->type == STR_WIDE)
		state->startptr = state->wstr + n;
	else
		state->startptr = state->str + n;
}

inline static void shift_one(bmexec_t *state)
{

	state->shift = 1;
	state->j += state->shift;
}

inline static bool bol_match(bmexec_t *state)
{

	return ((state->j == 0)
	    || ((state->type == STR_WIDE) ? (state->wstr[state->j - 1] == L'\n')
	    : (state->str[state->j - 1] == '\n')));
}

inline static bool eol_match(bmexec_t *state)
{
	return ((state->type == STR_WIDE)
	    ? ((state->j + state->fg->wlen == state->wstrlen) || (state->wstr[state->j + state->fg->wlen] == L'\n'))
	    : ((state->j + state->fg->len == state->strlen) || (state->str[state->j + state->fg->wlen] == '\n')));
}

inline static bool out_of_bounds(bmexec_t *state)
{

	return ((state->type == STR_WIDE)
	    ? ((state->j + state->fg->wlen) > state->wstrlen)
	    : ((state->j + state->fg->len) > state->strlen));
}

/*
 * Shifts in the input string after a mismatch. The position of the
 * mismatch is stored in the mismatch variable.
 */
inline static void shift(bmexec_t *state)
{
	int r = -1;
	unsigned int bc = 0, gs = 0, ts;

	// checkbounds

	if (state->type == STR_WIDE) {
		if (state->u != 0 && (unsigned)state->mismatch == state->fg->wlen - 1 - state->shift)
			state->mismatch -= state->u;
		state->v = state->fg->wlen - 1 - state->mismatch;
		r = hashtable_get(state->fg->qsBc_table,
		    &state->wstr[(size_t)state->j + state->fg->wlen], &bc);
		gs = state->fg->bmGs[state->mismatch];
		bc = (r == HASH_OK) ? bc : state->fg->defBc;
	} else {
		if (state->u != 0 && (unsigned)state->mismatch == state->fg->len - 1 - state->shift)
			state->mismatch -= state->u;
		state->v = state->fg->len - 1 - state->mismatch;
		gs = state->fg->sbmGs[state->mismatch];
		bc = state->fg->qsBc[((const unsigned char *)state->str)
		    [(size_t)state->j + state->fg->len]];
	}

	ts = (state->u >= state->v) ? (state->u - state->v) : 0;
	state->shift = MAX(ts, bc);
	state->shift = MAX(state->shift, gs);
	if (state->shift == gs)
		state->u = MIN((state->type == STR_WIDE ? state->fg->wlen : state->fg->len) - state->shift, state->v);
	else {
		if (ts < bc)
			state->shift = MAX(state->shift, state->u + 1);
		state->u = 0;
	}
	state->j = state->j + state->shift;
}

/*
 * Returns:	-(i + 1) on failure (position that it failed with minus sign)
 *		error code on error
 *		REG_OK on success
 */

static inline int
compare_from_behind(bmexec_t *state)
{
	const char *str_byte = state->startptr;
	const char *pat_byte = state->fg->pattern;
	const wchar_t *str_wide = state->startptr;
	const wchar_t *pat_wide = state->fg->wpattern;
	size_t len = (state->type == STR_WIDE) ? state->fg->wlen : state->fg->len;
	int ret = REG_OK;

	/* Compare the pattern and the input char-by-char from the last position. */
	for (int i = len - 1; i >= 0; i--) {
		if (state->type == STR_WIDE) {
			if (state->fg->icase ? (towlower(pat_wide[i]) == towlower(str_wide[i]))
			    : (pat_wide[i] == str_wide[i]))
				continue;
		} else {
			if (state->fg->icase ? (tolower(pat_byte[i]) == tolower(str_byte[i]))
			    : (pat_byte[i] == str_byte[i]))
				continue;
		}
		ret = -(i + 1);
		break;
	}
	return (ret);
}

/*
 * Executes matching of the precompiled pattern on the input string.
 * Returns REG_OK or REG_NOMATCH depending on if we find a match or not.
 */
int
frec_match_fast(const fastmatch_t *fg, const void *data, size_t len,
    int type, int nmatch, frec_match_t pmatch[], int eflags)
{
	bmexec_t state;
	int ret = REG_NOMATCH;

	DEBUG_PRINT("enter");
	#define FORMAT_BOOL(b) ((b) ? ('y') : ('n'))
	DEBUG_PRINTF("matchall: %c, nosub: %c, bol: %c, eol: %c",
		FORMAT_BOOL(fg->matchall), FORMAT_BOOL(fg->nosub),
		FORMAT_BOOL(fg->bol), FORMAT_BOOL(fg->eol));

	/* Calculate length if unspecified. */
	if (len == (size_t)-1)
		switch (type) {
		case STR_WIDE:
			len = wcslen(state.wstr);
			break;
		default:
			len = strlen(state.str);
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
		state.shift = fg->wlen;
		break;
	default:
		if (len < fg->len) {
			DEBUG_PRINT("too few data remained");
			return (ret);
		}
		state.shift = fg->len;
	}

	/*
	 * REG_NOTBOL means not anchoring ^ to the beginning of the line, so we
	 * can shift one because there can't be a match at the beginning.
	 */
	if (fg->bol && (eflags & REG_NOTBOL))
		state.j = 1;

	/*
	 * Like above, we cannot have a match at the very end when anchoring to
	 * the end and REG_NOTEOL is specified.
	 */
	if (fg->eol && (eflags & REG_NOTEOL))
		len--;

	init_bmexec(&state, fg, type, data, len);

	/* Only try once at the beginning or ending of the line. */
	if ((fg->bol || fg->eol) && !fg->newline && !(eflags & REG_NOTBOL) &&
	    !(eflags & REG_NOTEOL)) {
		/* Simple text comparison. */
		if (!((fg->bol && fg->eol) &&
		    (type == STR_WIDE ? (len != fg->wlen) : (len != fg->len)))) {
			/* Determine where in data to start search at. */
			state.j = fg->eol ? len - (type == STR_WIDE ? fg->wlen : fg->len) : 0;
			skip_chars(&state, state.j);
			state.mismatch = compare_from_behind(&state);
			if (state.mismatch == REG_OK) {
				if (!fg->nosub && nmatch >= 1) {
					pmatch[0].m.rm_so = state.j;
					pmatch[0].m.rm_eo = state.j + (type == STR_WIDE ?
					    fg->wlen : fg->len);
					DEBUG_PRINTF("offsets: %d %d", pmatch[0].m.rm_so,
					    pmatch[0].m.rm_eo);
				}
				return (REG_OK);
			}
		}
	} else {
		/* Quick Search / Turbo Boyer-Moore algorithm. */
		do {
			skip_chars(&state, state.j);
			state.mismatch = compare_from_behind(&state);
			if (state.mismatch == REG_OK) {
				if (fg->bol) {
					if (!bol_match(&state)) {
						shift_one(&state);
						continue;
					}
				}
				if (fg->eol) {
					if (!eol_match(&state)) {
						shift_one(&state);
						continue;
					}
				}
				if (!fg->nosub && nmatch >= 1) {
					pmatch[0].m.rm_so = state.j;
					pmatch[0].m.rm_eo = state.j + ((type == STR_WIDE) ?
					    fg->wlen : fg->len);
					DEBUG_PRINTF("offsets: %d %d", pmatch[0].m.rm_so,
					    pmatch[0].m.rm_eo);
				}
				return (REG_OK);
			} else if (state.mismatch > 0)
				return (state.mismatch);
			state.mismatch = -state.mismatch - 1;
			shift(&state);
		} while (!out_of_bounds(&state));
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
