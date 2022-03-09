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

int frec_proc_literal(fastmatch_t *fg, const wchar_t *wpat, size_t wn,
    const char *pat, size_t n, int cflags) {}
int frec_proc_fast(fastmatch_t *fg, const wchar_t *wpat, size_t wn,
    const char *pat, size_t n, int cflags) {}


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
	size_t startpos;
	unsigned prev_factor, curr_factor; // Turbo Boyer-Moore
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
inline static void seek(bmexec_t *state)
{

	if (state->type == STR_WIDE)
		state->startptr = state->wstr + state->startpos;
	else
		state->startptr = state->str + state->startpos;
}

inline static void shift_one(bmexec_t *state)
{

	state->shift = 1;
	state->startpos += state->shift;
}

inline static bool bol_match(bmexec_t *state)
{

	return ((state->startpos == 0)
	    || ((state->type == STR_WIDE) ? (state->wstr[state->startpos - 1] == L'\n')
	    : (state->str[state->startpos - 1] == '\n')));
}

inline static bool eol_match(bmexec_t *state)
{
	return ((state->type == STR_WIDE)
	    ? ((state->startpos + state->fg->wlen == state->wstrlen) || (state->wstr[state->startpos + state->fg->wlen] == L'\n'))
	    : ((state->startpos + state->fg->len == state->strlen) || (state->str[state->startpos + state->fg->wlen] == '\n')));
}

inline static bool out_of_bounds(bmexec_t *state)
{

	return ((state->type == STR_WIDE)
	    ? ((state->startpos + state->fg->wlen) > state->wstrlen)
	    : ((state->startpos + state->fg->len) > state->strlen));
}

/*
 * Shifts in the input string after a mismatch. The position of the
 * mismatch is stored in the mismatch variable.
 */
inline static void shift(bmexec_t *state)
{
	int r = -1;
	unsigned int bc = 0, gs = 0, ts;

	if (state->type == STR_WIDE) {
		if (state->prev_factor != 0 && (unsigned)state->mismatch == state->fg->wlen - 1 - state->shift)
			state->mismatch -= state->prev_factor;
		state->curr_factor = state->fg->wlen - 1 - state->mismatch;
		r = hashtable_get(state->fg->qsBc_table,
		    &state->wstr[(size_t)state->startpos + state->fg->wlen], &bc);
		gs = state->fg->bmGs[state->mismatch];
		bc = (r == HASH_OK) ? bc : state->fg->defBc;
	} else {
		if (state->prev_factor != 0 && (unsigned)state->mismatch == state->fg->len - 1 - state->shift)
			state->mismatch -= state->prev_factor;
		state->curr_factor = state->fg->len - 1 - state->mismatch;
		gs = state->fg->sbmGs[state->mismatch];
		bc = state->fg->qsBc[((const unsigned char *)state->str)
		    [(size_t)state->startpos + state->fg->len]];
	}

	ts = (state->prev_factor >= state->curr_factor) ? (state->prev_factor - state->curr_factor) : 0;
	state->shift = MAX(ts, bc);
	state->shift = MAX(state->shift, gs);
	if (state->shift == gs)
		state->prev_factor = MIN((state->type == STR_WIDE ? state->fg->wlen : state->fg->len) - state->shift, state->curr_factor);
	else {
		if (ts < bc)
			state->shift = MAX(state->shift, state->prev_factor + 1);
		state->prev_factor = 0;
	}
	state->startpos = state->startpos + state->shift;
	seek(state);
}

static inline int
compare_from_behind(bmexec_t *state)
{
	const char *str_byte = state->startptr;
	const char *pat_byte = state->fg->pattern;
	const wchar_t *str_wide = state->startptr;
	const wchar_t *pat_wide = state->fg->wpattern;
	size_t len = (state->type == STR_WIDE) ? state->fg->wlen : state->fg->len;

	/* Compare the pattern and the input char-by-char from the last position. */
	for (state->mismatch = len - 1; state->mismatch >= 0; state->mismatch--) {
		if (state->type == STR_WIDE) {
			if (state->fg->icase) {
				if (towlower(pat_wide[state->mismatch]) == towlower(str_wide[state->mismatch]))
					continue;
			} else {
				if (pat_wide[state->mismatch] == str_wide[state->mismatch])
					continue;
			}
		} else {
			if (state->fg->icase) {
				if (tolower(pat_byte[state->mismatch]) == tolower(str_byte[state->mismatch]))
					continue;
			} else {
				if (pat_byte[state->mismatch] == str_byte[state->mismatch])
					continue;
			}
		}
		return (REG_NOMATCH);
	}
	return (REG_OK);
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
	DEBUG_PRINTF("len: %zu, matchall: %c, nosub: %c, bol: %c, eol: %c, icase: %c",
		len, FORMAT_BOOL(fg->matchall), FORMAT_BOOL(fg->nosub),
		FORMAT_BOOL(fg->bol), FORMAT_BOOL(fg->eol),
		FORMAT_BOOL(fg->icase));

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
		state.startpos = 1;

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
			state.startpos = fg->eol ? len - (type == STR_WIDE ? fg->wlen : fg->len) : 0;
			seek(&state);
			ret = compare_from_behind(&state);
			if (ret == REG_OK) {
				if (!fg->nosub && nmatch >= 1) {
					pmatch[0].m.rm_so = state.startpos;
					pmatch[0].m.rm_eo = state.startpos + (type == STR_WIDE ?
					    fg->wlen : fg->len);
					DEBUG_PRINTF("offsets: %d %d", pmatch[0].m.rm_so,
					    pmatch[0].m.rm_eo);
				}
			}
			return ret;
		}
	} else {
		/* Quick Search / Turbo Boyer-Moore algorithm. */
		seek(&state);
		while (!out_of_bounds(&state)) {
			ret = compare_from_behind(&state);
			if (ret == REG_OK) {
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
					pmatch[0].m.rm_so = state.startpos;
					pmatch[0].m.rm_eo = state.startpos + ((type == STR_WIDE) ?
					    fg->wlen : fg->len);
					DEBUG_PRINTF("offsets: %d %d", pmatch[0].m.rm_so,
					    pmatch[0].m.rm_eo);
				}
				return (REG_OK);
			}
			shift(&state);
		}
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
