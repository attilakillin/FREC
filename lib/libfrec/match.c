/*-
 * Copyright (C) 2012, 2015, 2017 Gabor Kovesdan <gabor@FreeBSD.org>
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

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "boyer-moore.h"
#include "config.h"
#include "match.h"
#include "mregex.h"
#include "regexec.h"
#include "frec.h"
#include "wu-manber.h"

typedef struct matcher_state {
	regex_t *preg;
	heur_t *heur;
	const void *str;
	size_t len;
	int type;
	size_t nmatch;
	frec_match_t *pmatch;
	int eflags;

	const char *data_byte;
	const char *data_wide;
	size_t start;
	size_t rem;
	int ret;
} matcher_state;

inline static void init_state(matcher_state *state, regex_t *preg,
    heur_t *heur, const void *str, size_t len, int type, size_t nmatch,
    frec_match_t pmatch[], int eflags) {

	state->preg = preg;
	state->heur = heur;
	state->str = str;
	state->len = len;
	state->type = type;
	state->nmatch = nmatch;
	state->pmatch = pmatch;
	state->eflags = eflags;

	state->data_byte = str;
	state->data_wide = str;
	state->start = 0;
	state->rem = 0;
	state->ret = 0;
}

inline static void fix_offsets(matcher_state *state, size_t off) {

	for (size_t i = 0; i < state->nmatch; i++) {
		state->pmatch[i].m.rm_so += off;
		state->pmatch[i].m.rm_eo += off;
	}
}

inline static void seek_to(matcher_state *state, size_t off) {

	state->str = (state->type == STR_WIDE)
	    ? (const void *)&state->data_wide[off]
	    : (const void *)&state->data_byte[off];
}

inline static size_t search_lf_backward(matcher_state *state, size_t off) {

	for (size_t so = state->start + off; ; so--)
		if ((state->type == STR_WIDE) ? (state->data_wide[so] ==
		    L'\n') : (state->data_byte[so] == '\n') || (so == 0))
			return so;

}

inline static size_t search_lf_forward(matcher_state *state, size_t off) {
	size_t eo;

	for (eo = state->start + off; state->start + off < state->len; eo++)
		if ((state->type == STR_WIDE) ? (state->data_wide[eo] ==
		    L'\n') : (state->data_byte[eo] == '\n'))
			break;
	return eo;
}

inline static int call_regexec(const regex_t *preg, const void *str, size_t len,
    int type, size_t nmatch, frec_match_t pmatch[], int eflags) {
	regmatch_t *pm;
	int ret;

	pm = malloc(sizeof(regmatch_t) * nmatch);
	if (pm == NULL)
		return (REG_ESPACE);

	for (size_t i = 0; i < nmatch; i++)
		pm[i] = pmatch[i].m;

	ret = ((type == STR_WIDE) ?
	    (_dist_regwnexec(preg, str, len, nmatch, pm, eflags)) :
	    (_dist_regnexec(preg, str, len, nmatch, pm, eflags)));

	if (ret == REG_OK)
		for (size_t i = 0; i < nmatch; i++)
			pmatch[i].m = pm[i];
	free(pm);

	return ret;
}

int
frec_match_heur(regex_t *preg, heur_t *heur, const void *str,
    size_t len, int type, size_t nmatch, frec_match_t pmatch[], int eflags)
{
	int ret;
	matcher_state state;

	init_state(&state, preg, heur, str, len, type, nmatch, pmatch, eflags);

	/*
	 * REG_NEWLINE: looking for the longest fragment and then
	 * isolate the line and run the automaton.
	 */
	if (heur->type == HEUR_LONGEST) {
		DEBUG_PRINT("using HEUR_LONGEST strategy");
		while (state.start < state.len) {
			size_t eo, so;

			seek_to(&state, state.start);

			/* Match for heuristic */
			ret = (type == STR_WIDE)
				// TODO pmatch is wrong type
				? bm_execute_wide(pmatch, nmatch,
					heur->heur, str, len - state.start, eflags)
				: bm_execute_stnd(pmatch, nmatch,
					heur->heur, str, len - state.start, eflags);
			if (ret != REG_OK)
				return ret;

			/*
			 * If we do not know the length of the possibly matching part,
			 * look for newlines.
			 */
			if (heur->tlen == -1) {
				so = search_lf_backward(&state, pmatch[0].m.rm_so);
				eo = search_lf_forward(&state, pmatch[0].m.rm_eo);
			}

			/*
			 * If we know the possible match length, just check the narrowest
			 * context that we can, without looking for explicit newlines.
			 */
			else {
				state.rem = heur->tlen - (pmatch[0].m.rm_eo -
				    pmatch[0].m.rm_so);

				so = state.start + pmatch[0].m.rm_so <= state.rem ? 0 :
				    state.start + pmatch[0].m.rm_so - state.rem;
				eo = state.start + pmatch[0].m.rm_eo + state.rem >= state.len ? state.len :
				    state.start + pmatch[0].m.rm_eo + state.rem;
			}

			seek_to(&state, so);
			ret = call_regexec(state.preg, state.str, eo - so, state.type, state.nmatch,
			    state.pmatch, state.eflags);
			if (ret == REG_NOMATCH) {
				state.start = eo;
				continue;
			}
			fix_offsets(&state, so);
			return ret;
		}
	        return (REG_NOMATCH);
	}

	/*
	 * General case when REG_NEWLINE is not set.  Look for prefix,
	 * intermediate and suffix heuristics is available, to determine
	 * the context where the automaton will be invoked.  The start
	 * of the context is st and the relative end offset from st is
	 * stored in n.
	 */
	else {
		while (state.start < state.len) {
			seek_to(&state, state.start);

			/* Find beginning */
			DEBUG_PRINT("matching for prefix heuristics");
			ret = (type == STR_WIDE)
				// TODO pmatch is wrong type
				? bm_execute_wide(state.pmatch, state.nmatch,
					state.heur->heur, state.str, state.len - state.start, state.eflags)
				: bm_execute_stnd(state.pmatch, state.nmatch,
					state.heur->heur, state.str, state.len - state.start, state.eflags);
			if (ret != REG_OK)
				return (ret);

			state.start += state.pmatch[0].m.rm_so;
			size_t new_len = state.len - state.start;

			if (new_len > state.len - state.start)
				return (REG_NOMATCH);

			seek_to(&state, state.start);
			ret = call_regexec(state.preg, state.str, new_len,
			    state.type, state.nmatch, state.pmatch,
			    state.eflags);

			if (ret == REG_OK)
				fix_offsets(&state, state.start);
			return ret;
		}
		return (REG_NOMATCH);
	}
}

int
frec_match(const frec_t *preg, const void *str, size_t len,
    int type, size_t nmatch, frec_match_t pmatch[], int eflags)
{
	bm_preproc_t *shortcut = preg->shortcut;
	heur_t *heur = preg->heur;
	int ret = REG_NOMATCH;

	if (shortcut != NULL) {
		DEBUG_PRINT("matching with literal matcher");
		return (type == STR_WIDE)
			// TODO Incompatible pointer types
			? bm_execute_wide(pmatch, nmatch, shortcut, str, len, eflags)
			: bm_execute_stnd(pmatch, nmatch, shortcut, str, len, eflags);
	} else if (heur != NULL) {
		DEBUG_PRINT("matching with heuristic matcher");
		return (frec_match_heur(__DECONST(regex_t *, &preg->orig),
		    heur, str, len, type, nmatch, pmatch, eflags));
	}

	DEBUG_PRINT("matching with automaton matcher");
	ret = call_regexec(&preg->orig, str, len, type, nmatch, pmatch, eflags);
	return (ret);
}

int
frec_mmatch(const void *str, size_t len, int type, size_t nmatch,
    frec_match_t pmatch[], int eflags, const mregex_t *preg)
{
	const wchar_t *str_wide = str;
	const char *str_byte = str;
	int ret = REG_NOMATCH;
	bool need_offsets;

	need_offsets = !(preg->cflags & REG_NOSUB) && (nmatch > 0);

#define INPUT(pos) ((type == STR_WIDE) ? (const void *)&str_wide[pos] : \
    (const void *)&str_byte[pos])

	/*
	 * MHEUR_NONE: worst case; no way to speed up the match.
	 * There is no trivial best solution either, so just try
	 * matching for each of the patterns and return the earliest.
	 */
	if (preg->type == MHEUR_NONE) {
		DEBUG_PRINT("matching without multi-pattern heuristic");

		if (!need_offsets) {
			/* Run matches for each pattern until a match is found */
			for (size_t i = 0; i < preg->k; i++) {
				ret = frec_match(&preg->patterns[i], str, len, type,
					0, NULL, eflags);
				if (ret == REG_OK)
					return REG_OK;
				else if (ret != REG_NOMATCH)
					return ret;
			}
			return REG_NOMATCH;
		} else {
			frec_match_t **pm = NULL;
			size_t i;
			int first;
			bool matched = false;

			/* Alloc one frec_match_t for each pattern */
			pm = malloc(preg->k * sizeof(frec_match_t *));
			if (!pm)
				return (REG_ESPACE);
			for (i = 0; i < preg->k; i++) {
				pm[i] = malloc(nmatch * sizeof(frec_match_t));
				if (!pm[i])
					goto finish1;
			}

			/* Run matches for each pattern and save first matching offsets. */
			for (i = 0; i < preg->k; i++) {
				ret = frec_match(&preg->patterns[i], str, len, type,
					nmatch, pm[i], eflags);

				/* Mark if there is no match for the pattern. */
				if (ret == REG_OK)
					matched = true;
				else if (ret == REG_NOMATCH)
					pm[i][0].m.rm_so = -1;
				else
					goto finish1;
			}

			if (!matched) {
				ret = REG_NOMATCH;
				goto finish1;
			} else {
				/* If there are matches, find the first one. */
				first = 0;
				for (i = 1; i < preg->k; i++) {
					if (pm[i][0].m.rm_so == -1)
						continue;
					else if ((pm[i][0].m.rm_so < pm[first][0].m.rm_so) ||
					    ((pm[i][0].m.rm_so == pm[first][0].m.rm_so) && (pm[i][0].m.rm_eo > pm[first][0].m.rm_eo)))
						first = i;
				}
			}

			/* Fill in the offsets before returning. */
			for (i = 0; i < nmatch; i++) {
				pmatch[i].m.rm_so = pm[first][i].m.rm_so;
				pmatch[i].m.rm_eo = pm[first][i].m.rm_eo;
				pmatch[i].p = i;
				DEBUG_PRINTF("offsets %zu: %d %d", i, pmatch[i].m.rm_so, pmatch[i].m.rm_eo);
			}
			ret = REG_OK;

finish1:
			if (pm) {
				for (i = 0; i < preg->k; i++)
					if (pm[i])
						free(pm[i]);
				free(pm);
			}
			return ret;
		}
	}

	/*
	 * MHEUR_LONGEST: only searching the longest fragment of each
	 * pattern and then isolating the context and calling the
	 * automaton.
	 */
	else if (preg->type == MHEUR_LONGEST) {
		frec_match_t *pm = NULL;
		frec_match_t rpm;
		size_t st = 0, bl, el;

		DEBUG_PRINT("matching to the longest fragments and isolating lines");

		/* Alloc frec_match_t structures for results */
		if (need_offsets) {
			pm = malloc(nmatch * sizeof(frec_match_t *));
			if (!pm)
				return REG_ESPACE;
		}

		while (st < len) {
			/* Look for a possible match. */
			ret = frec_wmexec(preg->searchdata, INPUT(st), len, type,
			    1, &rpm, eflags);
			if (ret != REG_OK)
				goto finish2;

			/*
			 * First subcase; possibly matching pattern is
			 * fixed-length.
			 */
			if (preg->patterns[rpm.p].heur->tlen != -1) {
				size_t rem = preg->patterns[rpm.p].heur->tlen
				    - (rpm.m.rm_eo - rpm.m.rm_so);

				int so = st + rpm.m.rm_so <= rem ? 0 :
				    st + rpm.m.rm_so - rem;
				int eo = st + rpm.m.rm_eo + rem >= len ? len :
				    st + rpm.m.rm_eo + rem;

				ret = frec_match(&preg->patterns[rpm.p], INPUT(so), so - eo,
				    type, need_offsets ? nmatch : 0, pm, eflags);
			/*
			 * Second subcase; using line boundaries.
			 */
			} else {
				/* Look for the beginning of the line. */
				for (bl = st; bl > 0; bl--)
					if ((type == STR_WIDE) ? (str_wide[bl] == L'\n') :
					    (str_byte[bl] == '\n'))
						break;

				/* Look for the end of the line. */
				for (el = st; el < len; el++)
					if ((type == STR_WIDE) ? (str_wide[el] == L'\n') :
					    (str_byte[el] == '\n'))
						break;

				/* Try to match the pattern on the line. */
				ret = frec_match(&preg->patterns[rpm.p], INPUT(bl), el - bl,
				    type, need_offsets ? nmatch : 0, pm, eflags);
			}

			/* Need to start from here if matching failed. */
			st += rpm.m.rm_so + 1;

			/* Evaluate result. t*/
			if (ret == REG_NOMATCH)
				continue;
			else if (ret == REG_OK) {
				if (!need_offsets)
					return REG_OK;
				else {
					for (size_t i = 0; i < nmatch; i++) {
						pmatch[i].m.rm_so = pm[i].m.rm_so;
						pmatch[i].m.rm_eo = pm[i].m.rm_eo;
						pmatch[i].p = rpm.p;
						goto finish2;
					}
				}
			} else
				goto finish2;
		}
finish2:
		if (!pm)
			free(pm);
		return ret;
	}

	/*
	 * MHEUR_LITERAL: literal case; it is enough to search with Wu-Manber
	 * and immediately return the match.
	 */
	else if (preg->type == MHEUR_LITERAL) {

		DEBUG_PRINT("matching multiple literal patterns");

		return frec_wmexec(preg->searchdata, str, len, type, nmatch,
		    pmatch, eflags);
	}

	/*
	 * MHEUR_SINGLE: single pattern; call single matcher.
	 */
	else if (preg->type == MHEUR_SINGLE)
		return frec_match(&preg->patterns[0], str, len, type,
		    nmatch, pmatch, eflags);

	return ret;
}
