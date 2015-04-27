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

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "hashtable.h"
#include "mregex.h"
#include "frec.h"
#include "wu-manber.h"

#define WM_B 2

#define ALLOC(var, siz)							\
var = malloc(siz);							\
if (!var) {								\
	ret = REG_ESPACE;						\
	goto fail;							\
}

#define FAIL								\
do {									\
	ret = REG_BADPAT;						\
	goto failcomp;							\
} while (0);

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
		switch (ret) {
		case HASH_NOTFOUND:
			entry->shift = shift;
			entry->suff = 0;
			entry->pref = 1;
			entry->pref_list[0] = i;
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
		switch (r) {
		case HASH_NOTFOUND:
			entry->shift = shift = 0;
			entry->suff = 1;
			entry->pref = 0;
			entry->suff_list[0] = i;
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

#define _SAVE_PATTERNS(src, ss, dst, ds, type)				\
  do									\
    {									\
      ALLOC(dst, sizeof(type *) * nr);					\
      ALLOC(ds, sizeof(size_t) * nr);					\
      for (size_t i = 0; i < nr; i++)					\
	{								\
	  ALLOC(dst[i], ss[i] * sizeof(type));				\
	  memcpy(dst[i], src[i], ss[i] * sizeof(type));			\
	  ds[i] = ss[i];						\
	}								\
    } while (0);

#define SAVE_PATTERNS							\
  _SAVE_PATTERNS(bregex, bn, wm->pat, wm->siz, char)
#define SAVE_PATTERNS_WIDE						\
  _SAVE_PATTERNS(regex, n, wm->wpat, wm->wsiz, wchar_t)

/*
 * This file implements the Wu-Manber algorithm for pattern matching
 * with multiple patterns.  Even if it is not the best performing one
 * for low number of patterns but it scales well and it is very simple
 * compared to automaton-based multiple pattern algorithms.
 */

int
frec_wmcomp(wmsearch_t *wm, size_t nr, const wchar_t **regex,
	   size_t *n, __unused int cflags)
{
  wmentry_t *entry = NULL;
  int ret = REG_NOMATCH;
  char **bregex = NULL;
  size_t *bn = NULL;

  DEBUG_PRINT("enter");

  wm->wshift = NULL;
  wm->shift = NULL;

  ALLOC(bregex, sizeof(char *) * nr);
  ALLOC(bn, sizeof(size_t) * nr);

  for (size_t i = 0; i < nr; i++)
    {
      bn[i] = wcstombs(NULL, regex[i], 0);
      ALLOC(bregex[i], bn[i] + 1);
      ret = wcstombs(bregex[i], regex[i], bn[i]);

      /* Should never happen */
      if ((size_t)ret == (size_t)-1)
	{
	  ret = REG_BADPAT;
	  goto fail;
	}
      bregex[i][bn[i]] = '\0';
    }

  wm->pat = bregex;
  wm->siz = bn;

#ifdef WITH_DEBUG
  for (size_t i = 0; i < nr; i++)
    DEBUG_PRINTF("Wpattern %zu - %ls", i, regex[i]);

  for (size_t i = 0; i < nr; i++)
    DEBUG_PRINTF("Pattern %zu - %s", i, bregex[i]);
#endif

  SAVE_PATTERNS_WIDE;
//  SAVE_PATTERNS;

  ret = procwm((const void **)regex, n, nr, sizeof(wchar_t), wm->wshift, &wm->wm, STR_WIDE);
  if (ret != REG_OK)
    goto fail;
  ret = procwm((const void **)bregex, bn, nr, 1, wm->shift, &wm->m, STR_BYTE);
  if (ret != REG_OK)
    goto fail;


//  for (size_t i = 0; i < nr; i++)
//    free(bregex[i]);
//  free(bregex);
//  free(bn);

  return REG_OK;

fail:
  if (bn)
    free(bn);
  if (bregex)
    {
      for (size_t i = 0; i < nr; i++)
	free(bregex[i]);
      free(bregex);
    }
  if (wm->wshift)
    hashtable_free(wm->wshift);
  if (wm->shift)
    hashtable_free(wm->shift);
  if (entry)
    free(entry);
  return ret;
}

#define MATCH(beg, end, idx)						\
  do									\
    {									\
      if (!(wm->cflags & REG_NOSUB) && (nmatch > 0))			\
	{								\
	  pmatch->m.rm_so = beg;					\
	  pmatch->m.rm_eo = end;					\
	  pmatch->p = idx;						\
	}								\
      ret = REG_OK;							\
      goto finish;							\
    } while (0);

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
	size_t *bsiz = wm->siz;
	size_t *wsiz = wm->wsiz;
	size_t defsh = ((type == STR_WIDE) ? wm->wdefsh : wm->defsh);
	size_t m = ((type == STR_WIDE) ? wm->wm : wm->m);
	size_t shift;
	size_t pos = m;
	int ret = REG_NOMATCH;
	void *sh = ((type == STR_WIDE) ? wm->wshift : wm->shift);

#define INPUT(i) ((type == STR_WIDE) ? (void *)&wstr[i] : (void *)&bstr[i])
#define SIZES ((type == STR_WIDE) ? wsiz : bsiz)
#define CHSIZ ((type == STR_WIDE) ? sizeof(wchar_t) : 1)

	DEBUG_PRINT("enter");

	ALLOC(s_entry, sizeof(wmentry_t));
	ALLOC(p_entry, sizeof(wmentry_t));

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
							size_t k;

							if (len - pos > SIZES[idx] - m) {
								for (k = WM_B; k < SIZES[idx]; k++)
									if (memcmp(PAT_ARR(idx, k), INPUT(pos - m + k), CHSIZ) != 0)
										break;
								if (k == SIZES[idx])
									MATCH(pos - m, pos - m + SIZES[idx], idx);
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

  if (wm->shift)
    hashtable_free(wm->shift);
  for (size_t i = 0; i < wm->n; i++)
    if (wm->pat[i])
      free(wm->pat[i]);
  if (wm->pat)
    free(wm->pat);
  if (wm->siz)
    free(wm->siz);
  if (wm->wshift)
    hashtable_free(wm->wshift);
  for (size_t i = 0; i < wm->wn; i++)
    if (wm->wpat[i])
      free(wm->wpat[i]);
  if (wm->wpat)
    free(wm->wpat);
  if (wm->wsiz)
    free(wm->wsiz);
}
