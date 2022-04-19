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

#include <stdlib.h>
#include <string-type.h>
#include <string.h>

#include "hashtable.h"
#include "frec-internal.h"
#include "wu-manber.h"

#define WM_B 2

#define FAIL								\
do {									\
	ret = REG_BADPAT;						\
	goto failcomp;							\
} while (0);

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

static int
procwm(const void **pat_arr, size_t *siz_arr, size_t nr, size_t chsiz, void *sh, size_t *m, int type)
 {
        wmentry_t *entry = NULL;
	wchar_t **warr = (wchar_t **)pat_arr;
	char **barr = (char **)pat_arr;
        int ret = REG_OK;

#define PAT_ARR(i, j)	((type == STR_WIDE) ? (const void *)(&warr[i][j]) \
			    : (const void *)(&barr[i][j]))

	/* Determine shortest pattern length */
	*m = siz_arr[0];
	for (size_t i = 1; i < nr; i++)
		*m = siz_arr[i] < *m ? siz_arr[i] : *m;

	/*
	 * m - WM_B + 1 fragment per pattern plus extra space to reduce
	 * collisions.
	 */
	sh = hashtable_init((*m - WM_B + 1) * nr * 2,
	    WM_B * chsiz, sizeof(wmentry_t));

	if (sh == NULL) {
		ret = REG_ESPACE;
		goto failcomp;
	}

	entry = malloc(sizeof(wmentry_t));
	if (!entry) {
		ret = REG_ESPACE;
		goto failcomp;
	}

	for (size_t i = 0; i < nr; i++) {
		size_t shift;
		int r;

		/* First fragment, treat special because it is a prefix */
		r = hashtable_get(sh, PAT_ARR(i, 0), entry);
		shift = *m - WM_B;
		switch (r) {
		case HASH_NOTFOUND:
			entry->shift = shift;
			entry->suff = 0;
			entry->pref = 0;
			entry->pref_list[entry->pref++] = i;
			r = hashtable_put(sh, PAT_ARR(i, 0), entry);
			if (r != HASH_OK)
				FAIL;
			break;
		case HASH_OK:
			entry->shift = MIN(entry->shift, shift);
			entry->pref_list[entry->pref++] = i;
			r = hashtable_put(sh, PAT_ARR(i, 0), entry);
			if (r != HASH_UPDATED)
				FAIL;
		}

		/* Intermediate fragments, only shift calculated */
		for (size_t j = 1; j < *m - WM_B; j++) {
			r = hashtable_get(sh, PAT_ARR(i, j), entry);
			shift = *m - WM_B - j;
			switch (r) {
			case HASH_NOTFOUND:
				entry->shift = shift;
				entry->suff = 0;
				entry->pref = 0;
				r = hashtable_put(sh, PAT_ARR(i, j), entry);
				if (r != HASH_OK)
					FAIL;
				break;
			case HASH_OK:
				entry->shift = MIN(entry->shift, shift);
				r = hashtable_put(sh, PAT_ARR(i, j), entry);
				if (r != HASH_UPDATED)
					FAIL;
			}
		}

		/* Suffix fragment */
		r = hashtable_get(sh, PAT_ARR(i, *m - WM_B), entry);
		shift = 0;
		switch (r) {
		case HASH_NOTFOUND:
			entry->shift = 0;
			entry->suff = 0;
			entry->pref = 0;
			entry->suff_list[entry->suff++] = i;
			r = hashtable_put(sh, PAT_ARR(i, *m - WM_B), entry);
			if (r != HASH_OK)
				FAIL;
		case HASH_OK:
			entry->shift = MIN(entry->shift, shift);
			entry->suff_list[entry->suff++] = i;
			r = hashtable_put(sh, PAT_ARR(i, *m - WM_B), entry);
			if (r != HASH_UPDATED)
				FAIL;
		}
	}

failcomp:
	if (entry)
		free(entry);
	return (ret);
}

typedef struct wmcomp_state
{
	wmsearch_t *wm;
	size_t nr;
	const wchar_t **regex;
	size_t *len;

	wmentry_t *entry;
	char **bregex;
	size_t *blen;

} wmcomp_state;

inline static int init_wmcomp(wmcomp_state *state, wmsearch_t *wm,
    size_t nr, const wchar_t **regex, size_t *len)
{
	state->wm = wm;
	state->nr = nr;
	state->regex = regex;
	state->len = len;

	state->bregex = malloc(sizeof(char *) * state->nr);
	if (state->bregex == NULL)
		return (REG_ESPACE);
	state->blen = malloc(sizeof(size_t) * state->nr);
	if (state->blen == NULL)
		return (REG_ESPACE);

	state->wm->wshift = NULL;
	state->wm->shift = NULL;

	for (size_t i = 0; i < state->nr; i++) {
		state->blen[i] = wcstombs(NULL, state->regex[i], 0);
		state->bregex[i] = malloc(state->blen[i] + 1);
		if (state->bregex[i] == NULL)
			return (REG_ESPACE);
		int ret = wcstombs(state->bregex[i], state->regex[i],
		    state->blen[i]);

		/* Should never happen */
		if ((size_t)ret == (size_t)-1)
			return (REG_BADPAT);
		state->bregex[i][state->blen[i]] = '\0';
	}

#ifdef WITH_DEBUG
	for (size_t i = 0; i < state->nr; i++)
		DEBUG_PRINTF("Wpattern %zu - %ls", i, state->regex[i]);

	for (size_t i = 0; i < state->nr; i++)
		DEBUG_PRINTF("Pattern %zu - %s", i, state->bregex[i]);
#endif

	state->wm->pat = state->bregex;
	state->wm->siz = state->blen;
	return (REG_OK);
}

inline static void free_wmcomp(wmcomp_state *state)
{
	if (state->bregex != NULL) {
		for (size_t i = 0; i < state->nr; i++) {
			if (state->bregex[i] != NULL)
				free(state->bregex[i]);
		}
		free(state->bregex);
	}
	if (state->blen != NULL)
		free(state->blen);

	state->wm->pat = NULL;
	state->wm->siz = NULL;

	if (state->wm->wshift != NULL)
		hashtable_free(state->wm->wshift);
	if (state->wm->shift != NULL)
		hashtable_free(state->wm->shift);
}

inline static int procwm_mbs(wmcomp_state *state)
{
	return procwm((const void **)state->bregex, state->blen, state->nr, 1,
	    state->wm->shift, &state->wm->m, STR_BYTE);
}

inline static int procwm_wide(wmcomp_state *state)
{
	return procwm((const void **)state->regex, state->len, state->nr, sizeof(wchar_t),
	    state->wm->wshift, &state->wm->wm, STR_WIDE);
}

/*
 * This file implements the Wu-Manber algorithm for pattern matching
 * with multiple patterns.  Even if it is not the best performing one
 * for low number of patterns but it scales well and it is very simple
 * compared to automaton-based multiple pattern algorithms.
 */

int
frec_wmcomp(wmsearch_t *wm, size_t nr, const wchar_t **regex,
	   size_t *len, __unused int cflags)
{
  int ret;
  wmcomp_state state;

  DEBUG_PRINT("enter");

  ret = init_wmcomp(&state, wm, nr, regex, len);
  if (ret != REG_OK)
    goto fail;

  ret = procwm_mbs(&state);
  if (ret != REG_OK)
    goto fail;
  ret = procwm_wide(&state);
  if (ret != REG_OK)
    goto fail;

  free_wmcomp(&state);

  return REG_OK;

fail:
  free_wmcomp(&state);
  return ret;
}

int
frec_wmexec(const wmsearch_t *wm, const void *str, size_t len,
           int type, size_t nmatch, frec_match_t pmatch[],
           __unused int eflags)
{
	wmentry_t *s_entry = NULL, *p_entry = NULL;
	wchar_t **warr = (wchar_t **)wm->wpat;
	wchar_t *wstr = (wchar_t *)str;
	char **barr = (char **)wm->pat;
	char *bstr = (char *)str;
	size_t defsh = ((type == STR_WIDE) ? wm->wdefsh : wm->defsh);
	size_t m = ((type == STR_WIDE) ? wm->wm : wm->m);
	size_t shift;
	size_t pos = m;
	int ret = REG_NOMATCH;
	void *sh = ((type == STR_WIDE) ? wm->wshift : wm->shift);

#define INPUT(i) ((type == STR_WIDE) ? (void *)&wstr[i] : (void *)&bstr[i])
#define SIZES ((type == STR_WIDE) ? wm->wsiz : wm->siz)
#define CHSIZ ((type == STR_WIDE) ? sizeof(wchar_t) : 1)

	DEBUG_PRINT("enter");

	s_entry = malloc(sizeof(wmentry_t));
	if (s_entry == NULL) {
		ret = REG_ESPACE;
		goto fail;
	}
	p_entry = malloc(sizeof(wmentry_t));
	if (p_entry == NULL) {
		ret = REG_ESPACE;
		goto fail;
	}

	while (pos < len) {
		ret = hashtable_get(sh, INPUT(pos - WM_B), s_entry);
		shift = (ret == HASH_OK) ? s_entry->shift : defsh;
		DEBUG_PRINTF("shifting %zu chars", shift);
		if (shift == 0) {
			ret = hashtable_get(sh, INPUT(pos - m), p_entry);
			if (ret == HASH_NOTFOUND) {
				pos++;
				continue;
			} else {
				for (size_t i = 0; i < p_entry->pref; i++)
					for (size_t j = 0; (j < s_entry->suff) &&
					    (s_entry->suff_list[j] <= p_entry->pref_list[i]); j++)
						if (s_entry->suff_list[j] == p_entry->pref_list[i]) {
							size_t idx = s_entry->suff_list[j];

							if (len - pos >= SIZES[idx] - m) {
								if (memcmp(PAT_ARR(idx, 0), INPUT(pos - SIZES[idx]), CHSIZ * SIZES[idx])) {
									if (!(wm->cflags & REG_NOSUB) && (nmatch > 0)) {
										pmatch->soffset = pos - SIZES[idx];
										pmatch->eoffset = pos;
										pmatch->pattern_id = idx;
									}
									ret = REG_OK;
									goto finish;

								}
							}
						}
						pos++;
			}
		} else
			pos += shift;
 	}
fail:
finish:
	if (s_entry)
		free(s_entry);
	if (p_entry)
		free(p_entry);
	return (ret);
}

void
frec_wmfree(wmsearch_t *wm)
{

	DEBUG_PRINT("enter");
	if (wm->pat != NULL) {
		for (size_t i = 0; i < wm->n; i++)
			if (wm->pat[i] != NULL)
				free(wm->pat[i]);
		free(wm->pat);
	}
	if (wm->siz != NULL)
		free(wm->siz);
	if (wm->shift != NULL);
		hashtable_free(wm->shift);

	if (wm->wpat != NULL) {
		for (size_t i = 0; i < wm->n; i++)
			if (wm->wpat[i] != NULL)
				free(wm->wpat[i]);
		free(wm->wpat);
	}
	if (wm->wsiz != NULL)
		free(wm->wsiz);
	if (wm->wshift != NULL)
		hashtable_free(wm->wshift);
}
