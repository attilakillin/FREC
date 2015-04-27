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

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "config.h"
#include "match.h"
#include "mregex.h"
#include "regexec.h"
#include "frec.h"
#include "wu-manber.h"

#define REGEXEC(preg, str, len, type, nmatch, pmatch, eflags)	\
{								\
	regmatch_t *pm;						\
								\
	pm = malloc(sizeof(regmatch_t) * nmatch);		\
	if (pm == NULL)						\
		return (REG_ESPACE);				\
								\
	for (i = 0; i < nmatch; i++)				\
		pm[i] = pmatch[i].m;				\
								\
	ret = ((type == STR_WIDE) ?					\
	    (_dist_regwnexec(preg, str, len, nmatch, pm, eflags)) :	\
	    (_dist_regnexec(preg, str, len, nmatch, pm, eflags)));	\
									\
	if (ret == REG_OK)						\
		for (i = 0; i < nmatch; i++)				\
			pmatch[i].m = pm[i];				\
	free(pm);							\
}

int
frec_match_heur(regex_t *preg, heur_t *heur, const void *str,
    size_t len, int type, size_t nmatch, frec_match_t pmatch[], int eflags)
{
	int ret;
	size_t st = 0, i = 1, n;
	const char *data_byte = str;
	const wchar_t *data_wide = str;

#define FIX_OFFSETS(adj)						\
	if (ret == REG_NOMATCH) {					\
		adj;							\
		continue;						\
	/* XXX: !REG_NOSUB */						\
	} else if (ret == REG_OK)					\
		for (i = 0; i < nmatch; i++) {				\
			pmatch[i].m.rm_so += st;			\
			pmatch[i].m.rm_eo += st;			\
		}							\
	return ret;

#define SEEK_TO(off)							\
	str = (type == STR_WIDE) ? (const void *)&data_wide[off] :	\
	    (const void *)&data_byte[off];

	/*
	 * REG_NEWLINE: looking for the longest fragment and then
	 * isolate the line and run the automaton.
	 */
	if (heur->type == HEUR_LONGEST) {
		DEBUG_PRINT("using HEUR_LONGEST strategy");
		while (st < len) {
			size_t eo, so;

			SEEK_TO(st);

			/* Match for heuristic */
			ret = frec_match_fast(heur->heurs[0], str, len - st,
			    type, nmatch, pmatch, eflags);
			if (ret != REG_OK)
				return ret;

			/*
			 * If we do not know the length of the possibly matching part,
			 * look for newlines.
			 */
			if (heur->tlen == -1) {
				for (so = st + pmatch[0].m.rm_so - 1; ; so--) {
					if ((type == STR_WIDE) ? (data_wide[so] ==
					    L'\n') : (data_byte[so] == '\n'))
						break;
					if (so == 0)
						break;
				}

				for (eo = st + pmatch[0].m.rm_eo; st + eo < len; eo++) {
					if ((type == STR_WIDE) ? (data_wide[eo] ==
					    L'\n') : (data_byte[eo] == '\n'))
						break;
				}
			}

			/*
			 * If we know the possible match length, just check the narrowest
			 * context that we can, without looking for explicit newlines.
			 */
			else {
				size_t rem = heur->tlen - (pmatch[0].m.rm_eo -
				    pmatch[0].m.rm_so);

				so = st + pmatch[0].m.rm_so <= rem ? 0 :
				    st + pmatch[0].m.rm_so - rem;
				eo = st + pmatch[0].m.rm_eo + rem >= len ? len :
				    st + pmatch[0].m.rm_eo + rem;
			}

			SEEK_TO(so);
			REGEXEC(preg, str, eo - so, type, nmatch,
			    pmatch, eflags);
			FIX_OFFSETS(st = eo);
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
		while (st < len) {
			SEEK_TO(st);

			/* Prefix heuristic */
			DEBUG_PRINT("matching for prefix heuristics");
			ret = frec_match_fast(heur->heurs[0], str, len - st,
			    type, nmatch, pmatch, eflags);
			if (ret != REG_OK)
				return (ret);
			st += pmatch[0].m.rm_so;
			n = pmatch[0].m.rm_eo - pmatch[0].m.rm_so;

			/* Intermediate heuristics (if any) */
			while (!(heur->heurs[i] == NULL) &&
			    ((heur->heurs[i + 1] != NULL) ||
			    ((heur->heurs[i + 1] == NULL) &&
			    (heur->type == HEUR_PREFIX_ARRAY)))) {
				SEEK_TO(st + n);
				if (len <= st + n)
					return (REG_NOMATCH);
				DEBUG_PRINT("matching for intermediate heuristics");
				ret = frec_match_fast(heur->heurs[i], str,
				    len - st - n, type, nmatch, pmatch,
				    eflags);
				if (ret != REG_OK)
					return (ret);
				n += pmatch[0].m.rm_eo;
				i++;
			}

			/* Suffix heuristic available */
			if ((heur->type == HEUR_ARRAY) &&
			    heur->heurs[i] != NULL) {
				DEBUG_PRINT("matching for suffix heuristics");
				SEEK_TO(st + n);
				if (len <= st + n)
					return (REG_NOMATCH);
				ret = frec_match_fast(heur->heurs[i], str,
				    len - st - n, type, nmatch, pmatch,
				    eflags);
				if (ret != REG_OK)
					return (ret);
				n += pmatch[0].m.rm_eo;

				SEEK_TO(st);
				REGEXEC(preg, str, n, type,
				    nmatch, pmatch, eflags);
				FIX_OFFSETS(st += n);
			}

			/* Suffix heuristic not available */
			else {
				DEBUG_PRINT("suffix heuristics not available");
				size_t l = (heur->tlen == -1) ? (len - st) :
				    (size_t)(heur->tlen);

				if (l > len - st)
					return (REG_NOMATCH);
				SEEK_TO(st);
				REGEXEC(preg, str, l, type, nmatch,
				    pmatch, eflags);
				FIX_OFFSETS(st += n);
			}
		}
		return (REG_NOMATCH);
	}
}

int
frec_match(const frec_t *preg, const void *str, size_t len,
    int type, size_t nmatch, frec_match_t pmatch[], int eflags)
{
	fastmatch_t *shortcut = preg->shortcut;
	heur_t *heur = preg->heur;
	size_t i;
	int ret = REG_NOMATCH;

	if (shortcut != NULL) {
		DEBUG_PRINT("matching with literal matcher");
		return (frec_match_fast(shortcut, str, len, type,
		    nmatch, pmatch, eflags));
	} else if (heur != NULL) {
		DEBUG_PRINT("matching with heuristic matcher");
		return (frec_match_heur(__DECONST(regex_t *, &preg->orig),
		    heur, str, len, type, nmatch, pmatch, eflags));
	}

	DEBUG_PRINT("matching with automaton matcher");
	REGEXEC(&preg->orig, str, len, type, nmatch, pmatch, eflags);
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

	need_offsets = (preg->searchdata &&
	    ((wmsearch_t *)(preg->searchdata))->cflags & REG_NOSUB) &&
	    (nmatch > 0);

#define INPUT(pos) ((type == STR_WIDE) ? (const void *)&str_wide[pos] : \
    (const void *)&str_byte[pos])

	/*
	 * Worst case: at least one pattern does not have a literal
	 * prefix so the Wu-Manber algorithm cannot be used to speed
	 * up the match.  There is no trivial best solution either,
	 * so just try matching for each of the patterns and return
	 * the earliest.
	 */
	if (preg->type == MHEUR_NONE) {
		frec_match_t **pm = NULL;
		size_t i;
		int first = -1;

		DEBUG_PRINT("matching without multi-pattern heuristic");

		/* Alloc one frec_match_t for each pattern */
		if (need_offsets) {
			pm = malloc(preg->k * sizeof(frec_match_t *));
			if (!pm)
				return (REG_ESPACE);
			for (i = 0; i < preg->k; i++) {
				pm[i] = malloc(nmatch * sizeof(frec_match_t));
				if (!pm[i])
					goto finish1;
			}
		}

		/* Run matches for each pattern and save first matching offsets. */
		for (i = 0; i < preg->k; i++) {
			ret = frec_match(&preg->patterns[i], str, len, type,
				need_offsets ? nmatch : 0, pm[i], eflags);

			/* Mark if there is no match for the pattern. */
				if (!need_offsets) {
				if (ret == REG_OK)
					return REG_OK;
			} else if (ret == REG_NOMATCH)
				pm[i][0].m.rm_so = -1;
			else if (ret != REG_OK)
				goto finish1;
		}

		if (!need_offsets)
			return REG_NOMATCH;

		/* Check whether there has been a match at all. */
		for (i = 0; i < preg->k; i++)
			if (pm[i][0].m.rm_so != -1) {
				first = i;
				break;
			}

		if (first == -1)
			ret = REG_NOMATCH;
		else {
			/* If there are matches, find the first one. */

			for (i = first + 1; i < preg->k; i++)
				if ((pm[i][0].m.rm_so < pm[first][0].m.rm_so) ||
				    (pm[i][0].m.rm_eo > pm[first][0].m.rm_eo)) {
					first = i;
				}
		}

		/* Fill int the offsets before returning. */
		for (i = 0; need_offsets && (i < nmatch); i++) {
			pmatch[i].m.rm_so = pm[first][i].m.rm_so;
			pmatch[i].m.rm_eo = pm[first][i].m.rm_eo;
			pmatch[i].p = i;
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

	/*
	 * REG_NEWLINE: only searching the longest fragment of each
	 * pattern and then isolating the line and calling the
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

			/* Need to start from here if this fails. */
			st += rpm.m.rm_so + 1;

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
	 * Literal case.  It is enough to search with Wu-Manber and immediately
	 * return the match.
	 */
	else if (preg->type == MHEUR_LITERAL) {

		DEBUG_PRINT("matching multiple literal patterns");

		return frec_wmexec(preg->searchdata, str, len, type, nmatch,
		    pmatch, eflags);
	}

	/*
	 * Single pattern.  Call single matcher.
	 */
	else if (preg->type == MHEUR_SINGLE)
		return frec_match(&preg->patterns[0], str, len, type,
		    nmatch, pmatch, eflags);

	/*
	 * General case. Look for the beginning of any of the patterns with the
	 * Wu-Manber algorithm and try to match from there with the automaton.
	 */
	else {
		frec_match_t *pm = NULL;
		frec_match_t rpm;
		size_t st = 0;

		DEBUG_PRINT("matching open-ended pattern set");

		/* Alloc frec_match_t structures for results */
		if (need_offsets) {
			pm = malloc(nmatch * sizeof(frec_match_t *));
			if (!pm)
				return REG_ESPACE;
		}

		while (st < len) {
			ret = frec_wmexec(preg->searchdata, INPUT(st),
			    len, type, nmatch, &rpm, eflags);
			if (ret != REG_OK)
				return ret;

			ret = frec_match(&preg->patterns[rpm.p], INPUT(rpm.m.rm_so),
			    len - rpm.m.rm_so, type, need_offsets ? nmatch : 0,
			    pm, eflags);
			if ((ret == REG_OK) && (pm[0].m.rm_so == 0)) {
				if (need_offsets)
					for (size_t i = 0; i < nmatch; i++) {
						pm[i].m.rm_so += st;
						pm[i].m.rm_eo += st;
						pm[i].p = rpm.p;
					}
				goto finish3;
			} else if ((ret != REG_NOMATCH) || (ret != REG_OK))
				goto finish3;
			st += rpm.m.rm_so + 1;
		}

finish3:
		if (pm)
			free(pm);
		return ret;
	}

}
