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

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "config.h"
#include "compile.h"
#include "convert.h"
#include "heuristic.h"
#include "mregex.h"
#include "frec.h"
#include "wu-manber.h"

int
frec_compile(frec_t *preg, const wchar_t *wregex, size_t wn,
    const char *regex, size_t n, int cflags)
{
	int ret;

	DEBUG_PRINT("enter");

	/*
	 * First, we always compile the NFA and it also serves as
	 * pattern validation.  In this way, validation is not
	 * scattered through the code.
	 */
	ret = _dist_regncomp(&preg->orig, regex, n, cflags);
	if (ret != REG_OK) {
		DEBUG_PRINT("regex lib compilation failed");
		return (ret);
	}

	/*
	 * Check if the pattern is literal even though REG_LITERAL
	 * was not set.
	 */
	ret = frec_compile_bm(preg, wregex, wn, regex, n, cflags);

	/* Only try to compile heuristic if the pattern is not literal */
	if ((ret != REG_OK) && !(cflags & REG_LITERAL)) {
		DEBUG_PRINT("not literal, trying to compile heuristics");
		frec_compile_heur(preg, wregex, wn, cflags);
	} else {
		DEBUG_PRINT("literal, no heuristic needed");
		preg->heur = NULL;
	}

	/* When here, at least NFA surely succeeded. */
	return (REG_OK);
}


int
frec_compile_bm(frec_t *preg, const wchar_t *wregex, size_t wn,
    const char *regex, size_t n, int cflags)
{
	fastmatch_t *shortcut;
	int ret = REG_OK;

	DEBUG_PRINT("enter");

	if (n < 2)
		goto too_short;
	shortcut = malloc(sizeof(fastmatch_t));
	if (!shortcut)
		return (REG_ESPACE);
	ret = (cflags & REG_LITERAL)
	    ? frec_proc_literal(shortcut, wregex, wn, regex, n, cflags)
	    : frec_proc_fast(shortcut, wregex, wn, regex, n, cflags);
	if (ret == REG_OK) {
		DEBUG_PRINT("literal matcher compiled");
		preg->shortcut = shortcut;
	} else {
		DEBUG_PRINT("literal matcher did not compile");
		free(shortcut);
too_short:
		preg->shortcut = NULL;
	}

	return (ret);
}

int
frec_compile_heur(frec_t *preg, const wchar_t *regex, size_t n, int cflags)
{
	heur_t *heur;
	int ret;

	heur = malloc(sizeof(heur_t));
	if (!heur)
		return (REG_ESPACE);

	ret = frec_proc_heur(heur, regex, n, cflags);
	if (ret != REG_OK) {
		DEBUG_PRINT("heuristic did not compile");
		free(heur);
		preg->heur = NULL;
	} else {
		DEBUG_PRINT("heuristic compiled");
		preg->heur = heur;
	}
	return (ret);
}

/* TODO:
 *
 * - REG_ICASE
 * - Test
 */

int
frec_mcompile(mregex_t *preg, size_t nr, const wchar_t **wregex,
    size_t *wn, const char **regex, size_t *n, int cflags)
{
	int ret;
	const wchar_t **frags = NULL;
	size_t *siz = NULL;
	wmsearch_t *wm = NULL;

	DEBUG_PRINT("enter");

	preg->k = nr;
	preg->searchdata = NULL;
	preg->patterns = malloc(nr * sizeof(frec_t));
	preg->cflags = cflags;
	if (!preg->patterns)
		return (REG_ESPACE);

	/* Compile NFA, BM and heuristic for each pattern. */
	for (size_t i = 0; i < nr; i++) {
		ret = frec_compile(&preg->patterns[i], wregex[i],
		     wn[i], regex[i], n[i], cflags);
		if (ret != REG_OK) {
			preg->err = i;
			goto err;
		}
	}

	/* Use single pattern matching in case of one pattern */
	if (nr == 1) {
		DEBUG_PRINT("strategy MHEUR_SINGLE");
		preg->type = MHEUR_SINGLE;
		goto finish;
	}


	/* Set the specific type of matching. */
	if (cflags & REG_LITERAL) {
		DEBUG_PRINT("strategy MHEUR_LITERAL");
		preg->type = MHEUR_LITERAL;
        } else if (cflags & REG_NEWLINE) {
		DEBUG_PRINT("strategy MHEUR_LONGEST");
                preg->type = MHEUR_LONGEST;
        } else {
		/* If not explicitly marked as literal, check if all patterns
		 * can be matched either with literal matching or the
		 * HEUR_LONGEST strategy. Otherwise, it is not possible
		 * to speed up multiple pattern matching.
		 */
		preg->type = MHEUR_LONGEST;
		for (size_t i = 0; i < nr; i++) {
			/* Literal */
			if (preg->patterns[i].shortcut != NULL)
				continue;
			/* HEUR_LONGEST */
			else if ((preg->patterns[i].heur != NULL)
			    && (((heur_t *)(preg->patterns[i].heur))->type == HEUR_LONGEST))
				continue;
			else {
				DEBUG_PRINT("strategy MHEUR_NONE");
				preg->type = MHEUR_NONE;
				goto finish;
			}
		}
		if (preg->type == MHEUR_LONGEST)
			DEBUG_PRINT("strategy MHEUR_LONGEST");
	}

	/*
	 * Set frag and siz to point to the fragments to compile and
	 * their respective sizes.
	 */
	if (!(cflags & REG_LITERAL)) {
		frags = malloc(nr * sizeof(char *));
		if (!frags)
			goto err;

		siz = malloc(nr * sizeof(size_t));
		if (!siz)
			goto err;

		for (size_t j = 0; j < nr; j++) {
			if (preg->patterns[j].shortcut != NULL) {
				frags[j] = preg->patterns[j].shortcut->wpattern;
				siz[j] = preg->patterns[j].shortcut->wlen;
			} else {
				frags[j] = ((heur_t *)(preg->patterns[j].heur))->warr[0];
				siz[j] = ((heur_t *)(preg->patterns[j].heur))->siz[0];
			}
		}
	} else {
		frags = wregex;
		siz = wn;
	}

	/* Alloc and compile the fragments for Wu-Manber algorithm. */
	wm = malloc(sizeof(wmsearch_t));
	if (!wm)
		goto err;
	ret = frec_wmcomp(wm, nr, frags, siz, cflags);
	if (ret != REG_OK)
		goto err;
	wm->cflags = cflags;
	preg->searchdata = wm;

	goto finish;

err:
	if (preg->patterns) {
		for (size_t i = 1; i < nr; i++)
			_dist_regfree(&preg->patterns[i].orig);
		free(preg->patterns);
	}
	if (wm)
		free(wm);

finish:
	if (!(cflags & REG_LITERAL) && !(preg->type == MHEUR_SINGLE)) {
		if (frags)
			free(frags);
		if (siz)
			free(siz);
	}
	return (ret);
}
