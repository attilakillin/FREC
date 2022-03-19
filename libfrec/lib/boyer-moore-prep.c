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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "boyer-moore.h"
#include "config.h"
#include "convert.h"
#include "regex-parser.h"

/*
 * Fills the bad character shift table in the given stnd preprocessing struct.
 * Standard character version. Optionally, the case can be ignored.
 */
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
	/* NB: the last character is skipped, because no jump occurs when the
	 * current character of the text is the same as the last pattern char. */
	for (size_t i = 0; i < len - 1; i++) {
		char c = pattern[i];

		/* Ignore case if necessary. */
		if (ignore_case) {
			c = tolower(c);
		}

		/* Set the shift value to their distance from the end of the word. */
		table[c] = len - 1 - i;
	}

	return (REG_OK);
}

/*
 * Fills the bad character shift table in the given wide preprocessing struct.
 * Wide character version. Optionally, the case can be ignored.
 * Initializes a hashtable struct as wide->badc_shifts, which must be freed!
 */
static int
fill_badc_shifts_wide(bm_preproc_wt *wide, bool ignore_case)
{
	wchar_t *pattern = wide->pattern;
	size_t len = wide->len;
	hashtable *table = hashtable_init(len * 8, sizeof(wchar_t), sizeof(int));
	wide->badc_shifts = table;

	/* If initialization fails, return early. */
	if (table == NULL) {
		return (REG_ESPACE);
	}

	/* For characters present in the word: */
	/* NB: the last character is skipped, because no jump occurs when the
	 * current character of the text is the same as the last pattern char. */
	for (size_t i = 0; i < len - 1; i++) {
		wchar_t c = pattern[i];
		
		/* Ignore case if necessary. */
		if (ignore_case) {
			c = towlower(c);
		}

		/* Set the shift value to their distance from the end of the word. */
		int value = len - 1 - i;
		int res = hashtable_put(table, &c, &value);
		if (res == HASH_FAIL || res == HASH_FULL) {
			return (REG_ESPACE);
		}
	}

	return (REG_OK);
}

/*
 * Fills the good suffix table for the given pattern. The pattern is received
 * as a char * variable, and as such, wchar_t pointers must be casted. The
 * width parameter signals the width multiplier (compared to sizeof(char))
 * of the casted pointer. Len is the number of elements in the pattern.
 */
static int
calculate_good_shifts(
	unsigned int *table, char *pattern, size_t width, size_t len)
{
	/* Calculate suffixes (as per the specification of the BM algorithm). */

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

	/* Calculate good suffix shifts. */

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

/*
 * Fills the good suffix shift table in the given stnd preprocessing struct.
 * Standard character version. Optionally, the case can be ignored.
 * Allocates memory for the good suffix table, which must be freed!
 */
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
		if (ignore_case) {
			free(pattern);
		}
		return (REG_ESPACE);
	}

	/* Calculate good shifts into the newly created table. */
	int res = calculate_good_shifts(
		stnd->goods_shifts, pattern, 1, len);

	/* If the case was ignored or an error occurred, the copy is freed. */
	if (ignore_case || res != REG_OK) {
		free(pattern);
	}

	return res;
}

/*
 * Fills the good suffix shift table in the given wide preprocessing struct.
 * Wide character version. Optionally, the case can be ignored.
 * Allocates memory for the good suffix table, which must be freed!
 */
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
		if (ignore_case) {
			free(pattern);
		}
		return (REG_ESPACE);
	}

	/* Calculate good shifts into the newly created table. */
	int res = calculate_good_shifts(wide->goods_shifts,
		(char *) pattern, sizeof(wchar_t) / sizeof(char), len);

	/* If the case was ignored or an error occurred, the copy is freed. */
	if (ignore_case || res != REG_OK) {
		free(pattern);
	}

	return res;
}

/*
 * Returns a zero initialized preprocessing struct or null if
 * the memory allocation failed.
 */
bm_preproc_t *
bm_create_preproc()
{
	bm_preproc_t *prep = malloc(sizeof(bm_preproc_t));
	if (prep == NULL) {
		return NULL;
	}

	prep->f_ignorecase = false;
	prep->f_linebegin = false;
	prep->f_lineend = false;
	prep->f_matchall = false;
	prep->f_newline = false;
	prep->f_nosub = false;
	prep->f_wholewords = false;

	prep->stnd.goods_shifts = NULL;
	prep->stnd.pattern = NULL;
	prep->stnd.len = 0;

	prep->wide.goods_shifts = NULL;
	prep->wide.badc_shifts = NULL;
	prep->wide.pattern = NULL;
	prep->wide.len = 0;

	return prep;
}

/*
 * Frees the resources associated with the given BM preprocessing struct.
 * Frees the struct itself too.
 */
void
bm_free_preproc(bm_preproc_t *prep)
{
	if (prep == NULL) {
		return;
	}
	if (prep->stnd.goods_shifts != NULL) {
		free(prep->stnd.goods_shifts);
		prep->stnd.goods_shifts = NULL;
	}
	if (prep->stnd.pattern != NULL) {
		free(prep->stnd.pattern);
		prep->stnd.pattern = NULL;
	}

	if (prep->wide.goods_shifts != NULL) {
		free(prep->wide.goods_shifts);
		prep->wide.goods_shifts = NULL;
	}
	if (prep->wide.pattern != NULL) {
		free(prep->wide.pattern);
		prep->wide.pattern = NULL;
	}
	if (prep->wide.badc_shifts != NULL) {
		hashtable_free(prep->wide.badc_shifts);
		prep->wide.badc_shifts = NULL;
	}
	
	free(prep);
}

/*
 * Given a result struct and a pattern with the given length, execute the
 * Boyer-Moore preprocessing phase as if the pattern was a literal text.
 * Additional flags can be passed using the cflags parameter.
 * Returns REG_OK if the pattern preprocessing was successful, REG_ESPACE
 * if a memory error occurred and REG_BADPAT if the compilation failed.
 */
int
bm_preprocess_literal(
	bm_preproc_t *result, const wchar_t *pattern, size_t len, int cflags)
{
	/* Initialize flags (bool variables are not false by default). */
	result->f_ignorecase = cflags & REG_ICASE;
	result->f_wholewords = cflags & REG_WORD;
	result->f_newline = cflags & REG_NEWLINE;
	result->f_nosub = cflags & REG_NOSUB;
	result->f_matchall = false;
	result->f_linebegin = false;
	result->f_lineend = false;

	/* If the length of the pattern is 0, set the matchall flag and return. */
	if (len == 0) {
		result->wide.pattern = NULL;
		result->wide.len = 0;

		result->stnd.pattern = NULL;
		result->stnd.len = 0;

		result->f_matchall = true;
		return (REG_OK);
	}

	/* The options set by these flags won't work if MB_CUR_MAX > 1. */
	if ((result->f_ignorecase || result->f_wholewords) && MB_CUR_MAX > 1) {
		return (REG_BADPAT);
	}

	/* Initialize wide pattern from parameter. */
	result->wide.pattern = malloc(sizeof(wchar_t) * (len + 1));
	if (result->wide.pattern == NULL) {
		return (REG_ESPACE);
	}

	/* Copy pattern and ensure terminating null character is present. */
	memcpy(result->wide.pattern, pattern, sizeof(wchar_t) * len);
	result->wide.pattern[len] = L'\0';
	result->wide.len = len;

	/* Convert and initialize standard pattern from the given wide pattern. */
	int ret = frec_convert_wcs_to_mbs(
		pattern, len, &(result->stnd.pattern), &(result->stnd.len));
	if (ret != REG_OK) {
		bm_free_preproc(result);
		return ret;
	}

	/* Fill bad and good shift tables for both char varieties. */
	ret = fill_badc_shifts_stnd(&(result->stnd), result->f_ignorecase);
	if (ret != REG_OK) {
		bm_free_preproc(result);
		return ret;
	}
	ret = fill_badc_shifts_wide(&(result->wide), result->f_ignorecase);
	if (ret != REG_OK) {
		bm_free_preproc(result);
		return ret;
	}
	ret = fill_good_shifts_stnd(&(result->stnd), result->f_ignorecase);
	if (ret != REG_OK) {
		bm_free_preproc(result);
		return ret;
	}
	ret = fill_good_shifts_wide(&(result->wide), result->f_ignorecase);
	if (ret != REG_OK) {
		bm_free_preproc(result);
		return ret;
	}

	return (REG_OK);
}

/*
 * Strips escape characters from the given in_pat pattern (of in_len) length,
 * and outputs the literal text result into out_pat (and its length into
 * out_len).
 * Input flags can be specified in in_flags, while output flags are set in the
 * given out_flags preprocessing struct.
 */
static int
strip_specials(
	const wchar_t *in_pat, size_t in_len, wchar_t *out_pat, size_t *out_len,
	int in_flags, bm_preproc_t *out_flags)
{
	const wchar_t *pattern = in_pat;
	size_t len = in_len;

	/* If the first character is ^, set the given flag and continue. */
	if (pattern[0] == L'^') {
		out_flags->f_linebegin = true;
		pattern++;
		len--;
	}

	/* If the only character remaining is a $, do the same. */
	if (len >= 1 && pattern[len - 1] == L'$') {
		out_flags->f_lineend = true;
		len--;
	}

	/* If GNU extensions are enabled, check for whole word markers. */
	if ((in_flags & REG_GNU) && len >= 14 && 
		(memcmp(pattern, L"[[:<:]]", sizeof(wchar_t) * 7) == 0) &&
		(memcmp(pattern + len - 7, L"[[:>:]]", sizeof(wchar_t) * 7) == 0)) {
		out_flags->f_wholewords = true;
		pattern += 7;
		len -= 14;
	}

	regex_parser_t parser;
	parser.escaped = false;
	parser.extended = in_flags & REG_EXTENDED;

	size_t pos = 0;
	/* Traverse the given pattern: */
	for (size_t i = 0; i < len; i++) {
		wchar_t c = pattern[i];
		switch (parse_wchar(&parser, c)) {
			case NORMAL_CHAR:
				out_pat[pos++] = c;
				break;
			case NORMAL_NEWLINE:
				out_pat[pos++] = '\n';
				break;
			case SHOULD_SKIP:
				break;
			/* If any special character was found, we abort. */
			default:
				return (REG_BADPAT);
		}
	}

	/* End the pattern with terminating null character and set length. */
	out_pat[pos] = L'\0';
	*out_len = pos;

	return (REG_OK);
}

/*
 * Given a result struct and a pattern with the given length, execute the
 * Boyer-Moore preprocessing phase while ignoring escaped characters.
 * Additional flags can be passed using the cflags parameter.
 * Returns REG_OK if the pattern preprocessing was successful, REG_ESPACE
 * if a memory error occurred and REG_BADPAT if the given pattern could not
 * be reduced to a literal text string.
 */
int
bm_preprocess_full(
	bm_preproc_t *result, const wchar_t *pattern, size_t len, int cflags)
{
	/* We'll execute literal preprocessing on a clean pattern. */
	wchar_t *clean_pattern = malloc(sizeof(wchar_t) * (len + 1));
	if (clean_pattern == NULL) {
		return (REG_ESPACE);
	}
	size_t clean_len = 0;
	
	/* Strip escapes when applicable. */
	int ret = strip_specials(
		pattern, len, clean_pattern, &clean_len, cflags, result);
	if (ret != REG_OK) {
		free(clean_pattern);
		return ret;
	}

	/* Execute literal preprocessing. */
	ret = bm_preprocess_literal(result, clean_pattern, clean_len, cflags);
	if (ret != REG_OK) {
		free(clean_pattern);
		return ret;
	}

	free(clean_pattern);
	return (REG_OK);
}
