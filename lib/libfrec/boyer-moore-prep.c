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

#include "boyer-moore.h"

static int
fill_badc_shifts_stnd(bm_preproc_ct *stnd, bool ignore_case)
{
	char *pattern = stnd->pattern;
	size_t len = stnd->len;
	unsigned int *table = stnd->badc_shifts;

	/* For every character in the alphabet, set the shift to length + 1. */
	for (size_t i = 0; i <= UCHAR_MAX; i++) {
		table[i] = len;
	}
	/* For characters present in the word: */
	for (size_t i = 0; i < len - 1; i++) {
		char c = pattern[i];

		/* Ignore case if necessary. */
		if (ignore_case) {
			c = tolower(c);
		}

		/* Set the shift value to their distance from the end of the word. */
		table[c] = len - i - 1;
	}

	return (REG_OK);
}

static int
fill_badc_shifts_wide(bm_preproc_wt *wide, bool ignore_case)
{
	wchar_t *pattern = wide->pattern;
	size_t len = wide->len;
	hashtable *table = hashtable_init(
		len * (ignore_case ? 8 : 4), sizeof(wchar_t), sizeof(int));
	wide->badc_shifts = table;

	if (table == NULL) {
		return (REG_ESPACE);
	}

	for (size_t i = 0; i < len - 1; i++) {
		wchar_t c = pattern[i];
		int value = len - i - 1;

		if (ignore_case) {
			c = towlower(c);
		}

		int res = hashtable_put(table, &c, &value);
		if (res == HASH_FAIL || res == HASH_FULL) {
			return (REG_ESPACE);
		}
	}

	return (REG_OK);
}

static int
calculate_good_shifts(
	unsigned int *table, char *pattern, size_t width, size_t len)
{
	/* Calculate suffixes. */

	int *suff = malloc(sizeof(int) * len);
	if (suff == NULL) {
		return (REG_ESPACE);
	}

	suff[len - 1] = len;

	int f = 0;
	int g = len - 1;
	for (int i = len - 2; i >= 0; i--) {
		if (i > g && suff[len - 1 + i - f] < i - g) {
			suff[i] = suff[len - 1 + i - f];
		} else {
			if (i < g) {
				g = i;
			}
			f = i;
			char *a = &(pattern[width * (g)]);
			char *b = &(pattern[width * (g + len - 1 - f)]);
			while (g >= 0 && memcmp(a, b, sizeof(char) * width) == 0) {
				g--;
			}
			suff[i] = f - g;
		}
	}

	/* Calculate good shifts. */

	for (int i = 0; i < len; i++) {
		table[i] = len;
	}

	int j = 0;
	for (int i = len - 1; i >= 0; i--) {
		if (suff[i] == i + 1) {
			while (j < len - 1 - i) {
				if (table[j] == len) {
					table[j] = len - 1 - i;
				}
				j++;
			}
		}
	}
	for (int i = 0; i <= len - 2; i++) {
		table[len - 1 - suff[i]] = len - 1 - i;
	}

	free(suff);
	return (REG_OK);
}

static int
fill_good_shifts_stnd(bm_preproc_ct *stnd, bool ignore_case)
{
	size_t len = stnd->len;

	/* The pattern to use is the original pattern. */
	char *pattern = stnd->pattern;

	/* Unless the case needs to be ignored. */
	if (ignore_case) {
		pattern = malloc(sizeof(char) * len);
		if (pattern == NULL) {
			return (REG_ESPACE);
		}
		for (int i = 0; i < len; i++) {
			char c = stnd->pattern[i];
			pattern[i] = tolower(c);
		}
	}

	/* Initialize good shifts table. */
	stnd->goods_shifts = malloc(sizeof(int) * len);
	if (stnd->goods_shifts == NULL) {
		return (REG_ESPACE);
	}

	/* Calculate good shifts into the newly created table. */
	int res = calculate_good_shifts(
		stnd->goods_shifts, pattern, 1, len);

	/* If the case was ignored, the copy must be freed. */
	if (ignore_case) {
		free(pattern);
	}

	return res;
}

static int
fill_good_shifts_wide(bm_preproc_wt *wide, bool ignore_case)
{
	size_t len = wide->len;

	/* The pattern to use is the original pattern. */
	wchar_t *pattern = wide->pattern;

	/* Unless the case needs to be ignored. */
	if (ignore_case) {
		pattern = malloc(sizeof(wchar_t) * len);
		if (pattern == NULL) {
			return (REG_ESPACE);
		}
		for (int i = 0; i < len; i++) {
			wchar_t c = wide->pattern[i];
			pattern[i] = towlower(c);
		}
	}

	/* Initialize good shifts table. */
	wide->goods_shifts = malloc(sizeof(int) * len);
	if (wide->goods_shifts == NULL) {
		return (REG_ESPACE);
	}

	/* Calculate good shifts into the newly created table. */
	int res = calculate_good_shifts(
		wide->goods_shifts, pattern, sizeof(wchar_t) / sizeof(char), len);

	/* If the case was ignored, the copy must be freed. */
	if (ignore_case) {
		free(pattern);
	}

	return res;
}

int
bm_preprocess_literal(
	bm_preproc_t *result, wchar_t *pattern, size_t len, int cflags)
{
	result->f_ignorecase = cflags & REG_ICASE;
	result->f_wholewords = cflags & REG_WORD;
	result->f_newline = cflags & REG_NEWLINE;
	result->f_nosub = cflags & REG_NOSUB;

	if (len == 0) {
		result->wide.pattern = malloc(sizeof(wchar_t) * 1);
		if (result->wide.pattern == NULL) {
			return (REG_ESPACE);
		}
		result->wide.pattern[0] = L'\0';
		result->wide.len = 0;

		result->stnd.pattern = malloc(sizeof(char) * 1);
		if (result->stnd.pattern == NULL) {
			// TODO Free
			return (REG_ESPACE);
		}
		result->stnd.pattern[0] = '\0';
		result->stnd.len = 0;
		return (REG_OK);
	}

	// TODO This might happen even when we intend to use wchars
	if (MB_CUR_MAX > 1 && (result->f_ignorecase || result->f_wholewords)) {
		return (REG_BADPAT);
	}

	/* Initialize wide pattern from parameter. */
	result->wide.pattern = malloc(sizeof(wchar_t) * (len + 1));
	if (result->wide.pattern == NULL) {
		return (REG_ESPACE);
	}
	memcpy(result->wide.pattern, pattern, sizeof(wchar_t) * (len + 1)); // TODO Might cause issue
	result->wide.len = len;

	int ret;

	/* Convert and initialize standard pattern from parameter. */
	ret = frec_convert_wcs_to_mbs(
		pattern, len, &(result->stnd.pattern), &(result->stnd.len));
	if (ret != REG_OK) {
		// TODO Free
		return ret;
	}

	
	ret = fill_badc_shifts_stnd(&(result->stnd), result->f_ignorecase);
	if (ret != REG_OK) {
		// TODO Free
		return ret;
	}
	ret = fill_badc_shifts_wide(&(result->wide), result->f_ignorecase);
	if (ret != REG_OK) {
		// TODO Free
		return ret;
	}
	ret = fill_good_shifts_stnd(&(result->stnd), result->f_ignorecase);
	if (ret != REG_OK) {
		// TODO Free
		return ret;
	}
	ret = fill_good_shifts_wide(&(result->wide), result->f_ignorecase);
	if (ret != REG_OK) {
		// TODO Free
		return ret;
	}

	return (REG_OK);
}

static int
strip_specials(
	wchar_t *in_pat, size_t in_len, wchar_t *out_pat, size_t *out_len,
	int in_flags, bm_preproc_t *out_flags)
{
	wchar_t *pattern = in_pat;
	size_t len = in_len;

	if (pattern[0] == L'^') {
		out_flags->f_linebegin = true;
		pattern++;
		len--;
	}

	if (pattern[0] == L'$' && len == 1) {
		out_flags->f_lineend = true;
		pattern++;
		len--;
	}

	if ((in_flags & REG_GNU) && len >= 14 && 
		(memcmp(pattern, L"[[:<:]]", sizeof(wchar_t) * 7) == 0) &&
		(memcmp(pattern + len - 7, L"[[:>:]]", sizeof(wchar_t) * 7) == 0)) {
		out_flags->f_wholewords = true;
		pattern += 7;
		len -= 14;
	}

	bool escaped = false;
	size_t pos = 0;
	for (size_t i = 0; i < len; i++) {
		wchar_t c = pattern[i];
		switch (c) {
		case L'\\':
			if (escaped) {
				out_pat[pos] = c;
				escaped = false;
				pos++;
			} else {
				escaped = true;
			}
			break;
		case L'[':
		case L'.':
			if (escaped) {
				out_pat[pos] = c;
				escaped = false;
				pos++;
			} else {
				// TODO FREE
				return (REG_BADPAT);
			}
			break;
		case '*':
			if (escaped || !(in_flags & REG_EXTENDED)) {
				out_pat[pos] = c;
				escaped = false;
				pos++;
			} else {
				// TODO FREE
				return (REG_BADPAT);
			}
			break;
		case L'+':
		case L'?':
			if ((in_flags & REG_EXTENDED) && i == 0) {
				continue;
			}
			if (!escaped ^ (in_flags & REG_EXTENDED)) {
				out_pat[pos] = c;
				escaped = false;
				pos++;
			} else {
				// TODO FREE
				return (REG_BADPAT);
			}
			break;
		case L'^':
			out_pat[pos] = c;
			escaped = false;
			pos++;
			break;
		case L'$':
			if (!escaped && i == len - 1) {
				out_flags->f_lineend = true;
			} else {
				out_pat[pos] = c;
				escaped = false;
				pos++;
			}
			break;
		case L'|':
		case L'(':
			if (escaped ^ (in_flags & REG_EXTENDED)) {
				// TODO FREE
				return (REG_BADPAT);
			} else {
				out_pat[pos] = c;
				escaped = false;
				pos++;
			}
			break;
		case L'{':
			if ((escaped ^ !(in_flags & REG_EXTENDED)) || i == 0) {
				out_pat[pos] = c;
				escaped = false;
				pos++;
			} else {
				// TODO FREE
				return (REG_BADPAT);
			}
			break;
		default:
			if (escaped) {
				// TODO FREE
				return (REG_BADPAT);
			} else {
				out_pat[pos] = c;
				escaped = false;
				pos++;
			}
			break;
		}
	}

	*out_len = pos;
	return (REG_OK);
}

int
bm_preprocess_full(
	bm_preproc_t *result, wchar_t *pattern, size_t len, int cflags)
{
	wchar_t *clean_pattern = malloc(sizeof(wchar_t) * len);
	if (clean_pattern == NULL) {
		return (REG_ESPACE);
	}
	size_t clean_len = 0;
	
	int ret = strip_specials(pattern, len, clean_pattern, &clean_len, cflags, result);
	if (ret != REG_OK) {
		// TODO FREE
		return (REG_BADPAT);
	}
	ret = bm_preprocess_literal(result, clean_pattern, clean_len, cflags);
	if (ret != REG_OK) {
		// TODO FREE
		return (REG_BADPAT);
	}

	free(clean_pattern);
	return (REG_OK);
}
