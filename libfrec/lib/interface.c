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

#include <stdbool.h>
#include <stdlib.h>
#include <string-type.h>
#include <wchar.h>
#include <string.h>

#include "bm-type.h"
#include "compile.h"
#include "convert.h"
#include "heuristic.h"
#include "match.h"
#include "frec-internal.h"
#include "string-type.h"
#include "wu-manber.h"

int
frec_regncomp(frec_t *preg, const char *regex, size_t len, int cflags)
{
    string pattern;
    string_borrow(&pattern, regex, (ssize_t) len, false);

	int ret = frec_compile(preg, pattern, cflags);

    string_free(&pattern);
	return ret;
}

int
frec_regcomp(frec_t *preg, const char *regex, int cflags)
{
	ssize_t len;
	if ((cflags & REG_PEND) && preg->re_endp >= regex) {
		len = preg->re_endp - regex;
	} else {
		len = (regex != NULL) ? ((ssize_t) strlen(regex)) : 0;
	}

	return frec_regncomp(preg, regex, len, cflags);
}

int
frec_regwncomp(frec_t *preg, const wchar_t *regex, size_t len, int cflags)
{
    string pattern;
    string_borrow(&pattern, regex, (ssize_t) len, true);

    int ret = frec_compile(preg, pattern, cflags);

    string_free(&pattern);
    return ret;
}

int
frec_regwcomp(frec_t *preg, const wchar_t *regex, int cflags)
{
	ssize_t len;
	if ((cflags & REG_PEND) && preg->re_wendp >= regex) {
		len = preg->re_wendp - regex;
	} else {
		len = (regex != NULL) ? ((ssize_t) wcslen(regex)) : 0;
	}

	return frec_regwncomp(preg, regex, len, cflags);
}

void
frec_regfree(frec_t *preg)
{
	bm_comp_free(preg->boyer_moore);
	frec_free_heur(preg->heuristic);
	_dist_regfree(&preg->original);
}

typedef struct regexec_state {
	const void *preg;
	const void *str;
	size_t len;
	size_t nmatch;
	frec_match_t *pmatch;
	int cflags;
	int eflags;
	int type;

	size_t slen;
	size_t offset;
} regexec_state;

inline static void init_state(regexec_state *state, const void *preg,
    const void *str, size_t len, size_t nmatch, frec_match_t pmatch[],
    int cflags, int eflags, int type)
{

	state->preg = preg;
	state->str = str;
	state->len = len;
	state->nmatch = nmatch;
	state->pmatch = pmatch;
	state->cflags = cflags;
	state->eflags = eflags;
	state->type = type;

	state->slen = 0;
	state->offset = 0;
}

inline static void calc_offsets_pre(regexec_state *state)
{

	if (state->type == STR_WIDE)
		state->slen = (state->len == (size_t)-1)
		    ? wcslen((const wchar_t *)state->str) : state->len;
	else
		state->slen = (state->len == (size_t)-1)
		    ? strlen((const char *)state->str) : state->len;
	state->offset = 0;

	if (state->eflags & REG_STARTEND) {
		state->offset += state->pmatch[0].soffset;
		state->slen -= state->offset;
	}
	DEBUG_PRINTF("search offsets %ld - %ld", state->offset,
	    state->offset + state->slen);
}

inline static void calc_offsets_post(regexec_state *state)
{

	if ((state->eflags & REG_STARTEND) && !(state->cflags & REG_NOSUB))
		for (size_t i = 0; i < state->nmatch; i++) {
			state->pmatch[i].soffset += (ssize_t) state->offset;
			state->pmatch[i].soffset += (ssize_t) state->offset;
			DEBUG_PRINTF("pmatch[%zu] offsets %d - %d", i,
			    state->pmatch[i].soffset, state->pmatch[i].soffset);
		}
}

inline static int
_regexec(const void *preg, const void *str, size_t len,
    size_t nmatch, frec_match_t pmatch[], int eflags, int type, bool multi)
{
	regexec_state state;
	int ret;
	int cflags = multi ? ((const mfrec_t *)preg)->cflags
	    : ((const frec_t *)preg)->cflags;

	init_state(&state, preg, str, len, nmatch, pmatch, cflags, eflags, type);
	calc_offsets_pre(&state);
	if ((state.eflags & REG_STARTEND) && (state.pmatch[0].soffset > state.pmatch[0].eoffset))
		return (REG_NOMATCH);
	if (multi)
		ret = oldfrec_mmatch(&((const wchar_t *)state.str)[state.offset],
		    state.slen, state.type, state.nmatch, state.pmatch,
		    state.eflags, state.preg);
	else {
        string text;
        string_borrow(&text, str, (ssize_t) len, type == STR_WIDE);
        string_offset(&text, (ssize_t) state.offset);
		ret = frec_match(state.pmatch, state.nmatch, state.preg, text, eflags);

		string_free(&text);
	}
	if (ret == REG_OK)
		calc_offsets_post(&state);
	DEBUG_PRINTF("returning %d", ret);
	return ret;

}

int
frec_regnexec(const frec_t *preg, const char *str, size_t len,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{
	int type = (MB_CUR_MAX == 1) ? STR_BYTE : STR_MBS;
	return _regexec(preg, str, len, nmatch, pmatch, eflags, type, false);
}

int
frec_regexec(const frec_t *preg, const char *str,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{

	int ret = frec_regnexec(preg, str, (size_t)-1, nmatch,
	    pmatch, eflags);

	DEBUG_PRINTF("returning %d", ret);
	return ret;
}


int
frec_regwnexec(const frec_t *preg, const wchar_t *str, size_t len,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{
	return _regexec(preg, str, len, nmatch, pmatch, eflags, STR_WIDE, false);
}

int
frec_regwexec(const frec_t *preg, const wchar_t *str,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{
	int ret = frec_regwnexec(preg, str, (size_t)-1, nmatch,
	    pmatch, eflags);
	DEBUG_PRINTF("returning %d", ret);
	return ret;
}



int
frec_mregncomp(mfrec_t *preg, size_t nr, const char **regex,
    size_t *n, int cflags)
{
	int ret = REG_NOMATCH;
	const wchar_t **wr;
	wchar_t **wregex = NULL;
	size_t *wlen = NULL;
	size_t i = 0;

	DEBUG_PRINT("enter");

	wregex = malloc(nr * sizeof(wchar_t *));
	if (!wregex) {
		DEBUG_PRINT("returning REG_ESPACE");
		return (REG_ESPACE);
	}
	wlen = malloc(nr * sizeof(size_t));
	if (!wlen)
		goto err;

	for (i = 0; i < nr; i++) {
		ret = convert_mbs_to_wcs(regex[i], n[i], &wregex[i], &wlen[i]);
		if (ret != REG_OK)
			goto err;
	}

	wr = (const wchar_t **)wregex;
	//ret = frec_mcompile(preg, nr, wr, wlen, cflags);

err:
	if (wregex) {
		for (size_t j = 0; j < i; j++)
			if (wregex[j])
				free(wregex[j]);
	}
	if (wlen)
		free(wlen);
	DEBUG_PRINTF("returning %d", ret);
	return ret;
}

int
frec_mregcomp(mfrec_t *preg, size_t nr, const char **regex, int cflags)
{
	int ret;
	size_t *len;

	DEBUG_PRINT("enter");

	len = malloc(nr * sizeof(size_t));
	if (!len) {
		DEBUG_PRINT("returning REG_ESPACE");
		return REG_ESPACE;
	}

	for (size_t i = 0; i < nr; i++)
		len[i] = strlen(regex[i]);

	ret = frec_mregncomp(preg, nr, regex, len, cflags);
	free(len);
	DEBUG_PRINTF("returning %d", ret);
	return (ret);
}


int
frec_mregwncomp(mfrec_t *preg, size_t nr, const wchar_t **regex,
    size_t *n, int cflags)
{
	int ret = REG_NOMATCH;
	const char **sr;
	char **sregex = NULL;
	size_t *slen = NULL;
	size_t i = 0;

	DEBUG_PRINT("enter");

	sregex = malloc(nr * sizeof(char *));
	if (!sregex) {
		DEBUG_PRINT("returning REG_ESPACE");
		return REG_ESPACE;
	}
	slen = malloc(nr * sizeof(size_t));
	if (!slen)
		goto err;

	for (i = 0; i < nr; i++) {
		ret = convert_wcs_to_mbs(regex[i], n[i], &sregex[i], &slen[i]);
		if (ret != REG_OK)
			goto err;
	}

	sr = (const char **)sregex;
	//ret = frec_mcompile(preg, nr, regex, n, cflags);

err:
	if (sregex) {
		for (size_t j = 0; j < i; j++)
			if (sregex[j])
				free(sregex[j]);
	}
	if (slen)
		free(slen);
	DEBUG_PRINTF("returning %d", ret);
	return (ret);
}

int
frec_mregwcomp(mfrec_t *preg, size_t nr, const wchar_t **regex,
    int cflags)
{
	int ret;
	size_t *len;

	DEBUG_PRINT("enter");

	len = malloc(nr * sizeof(size_t));
	if (!len) {
		DEBUG_PRINT("returning REG_ESPACE");
		return REG_ESPACE;
	}

	for (size_t i = 0; i < nr; i++)
		len[i] = wcslen(regex[i]);

	ret = frec_mregwncomp(preg, nr, regex, len, cflags);
	free(len);
	DEBUG_PRINTF("returning %d", ret);
	return (ret);
}

void
frec_mregfree(mfrec_t *preg)
{

	DEBUG_PRINT("enter");

	if (!preg) {
		frec_wmfree(preg->searchdata);
		free(preg);
	}
}

int
frec_mregnexec(const mfrec_t *preg, const char *str, size_t len,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{
	int type = (MB_CUR_MAX == 1) ? STR_BYTE : STR_MBS;
	return _regexec(preg, str, len, nmatch, pmatch, eflags, type, true);
}

int
frec_mregexec(const mfrec_t *preg, const char *str,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{
	int ret = frec_mregnexec(preg, str, (size_t)-1, nmatch,
	    pmatch, eflags);
	DEBUG_PRINTF("returning %d", ret);
	return ret;
}


int
frec_mregwnexec(const mfrec_t *preg, const wchar_t *str, size_t len,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{
	return _regexec(preg, str, len, nmatch, pmatch, eflags, STR_WIDE, true);
}

int
frec_mregwexec(const mfrec_t *preg, const wchar_t *str,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{

	int ret = frec_mregwnexec(preg, str, (size_t)-1, nmatch, pmatch, eflags);
	DEBUG_PRINTF("returning %d", ret);
	return ret;
}

size_t
frec_mregerror(int errcode, const mfrec_t *preg, int *errpatn, char *errbuf,
    size_t errbuf_size)
{

	DEBUG_PRINT("enter");

	if (errpatn)
		*errpatn = preg->err;

	int ret = _dist_regerror(errcode, NULL, errbuf, errbuf_size);
	DEBUG_PRINTF("returning %d", ret);
	return ret;
}
