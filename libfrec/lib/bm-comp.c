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
#include <frec-config.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string-type.h>
#include <wctype.h>

#include "bm.h"
#include "bm-type.h"
#include "regex-parser.h"

// Utility function. Asserts that str[at] equals stnd or wide in the correct
// sub-array.
static bool
string_has_char_at(string str, ssize_t at, char stnd, wchar_t wide)
{
    if (str.is_wide) {
        return str.wide[at] == wide;
    } else {
        return str.stnd[at] == stnd;
    }
}

// Fills the bad character shift table in the given compilation struct.
// Standard character version.
static int
fill_badc_shifts_stnd(bm_comp *comp)
{
	char *pattern = comp->pattern.stnd;
	ssize_t len = comp->pattern.len;
	uint *table = comp->bad_shifts_stnd;

	// For every character in the alphabet, set the shift to length.
	for (size_t i = 0; i <= UCHAR_MAX; i++) {
		table[i] = len;
	}

	// For characters present in the word:
	// NB: the last character is skipped, because no jump occurs when the
    // current character of the text is the same as the last pattern char.
	for (size_t i = 0; i < len - 1; i++) {
		char c = pattern[i];

		// Ignore case if necessary.
		if (comp->is_icase_set) {
			c = tolower((u_char) c);
		}

		// Set the shift value to their distance from the end of the word.
		table[c] = len - 1 - i;
	}

	return (REG_OK);
}

// Fills the bad character shift table in the given compilation struct.
// Wide character version.
static int
fill_badc_shifts_wide(bm_comp *comp)
{
	wchar_t *pattern = comp->pattern.wide;
	ssize_t len = comp->pattern.len;

	hashtable *table =
        hashtable_init(len * 8, sizeof(wchar_t), sizeof(unsigned int));
	comp->bad_shifts_wide = table;

	// If initialization fails, return early.
	if (table == NULL) {
		return (REG_ESPACE);
	}

	// For characters present in the word:
	// NB: the last character is skipped, because no jump occurs when the
    // current character of the text is the same as the last pattern char.
	for (ssize_t i = 0; i < len - 1; i++) {
		wchar_t c = pattern[i];
		
		// Ignore case if necessary. */
		if (comp->is_icase_set) {
			c = (wchar_t) towlower(c);
		}

		// Set the shift value to their distance from the end of the word.
		ssize_t value = len - 1 - i;
		int res = hashtable_put(table, &c, &value);
		if (res == HASH_FAIL || res == HASH_FULL) {
			return (REG_ESPACE);
		}
	}

	return (REG_OK);
}

// Fills the good suffix table for the given pattern.
static int
calculate_good_shifts(unsigned int *table, string *pattern)
{
    ssize_t len = pattern->len;

	// Calculate suffixes (as per the specification of the BM algorithm).
	ssize_t *suff = malloc(sizeof(ssize_t) * len);
	if (suff == NULL) {
		return (REG_ESPACE);
	}

	suff[len - 1] = len;

	ssize_t f = 0;
	ssize_t g = len - 1;
	for (ssize_t i = len - 2; i >= 0; i--) {
		if (i > g && suff[len - 1 + i - f] < i - g) {
			suff[i] = suff[len - 1 + i - f];
		} else {
			if (i < g) {
				g = i;
			}
			f = i;

            if (pattern->is_wide) {
                wchar_t *str = pattern->wide;
                while (g >= 0 && str[g] == str[g + len - 1 -f]) {
                    g--;
                }
            } else {
                char *str = pattern->stnd;
                while (g >= 0 && str[g] == str[g + len - 1 -f]) {
                    g--;
                }
            }

			suff[i] = f - g;
		}
	}

	// Calculate good suffix shifts.

	for (ssize_t i = 0; i < len; i++) {
		table[i] = len;
	}

	int j = 0;
	for (ssize_t i = len - 1; i >= 0; i--) {
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

// Fills the good suffix shift table in the given compilation struct.
// Can ignore the case of the pattern, as preset in the given struct.
// May allocate memory in comp even if the method itself does not succeed.
static int
fill_good_shifts(bm_comp *comp)
{
	// The pattern to use is the original pattern.
	string *pattern = &comp->pattern;

	// Unless the case needs to be ignored.
	if (comp->is_icase_set) {
        // In which case we duplicate the string
        bool success = string_duplicate(pattern, comp->pattern);
		if (!success) {
			return (REG_ESPACE);
		}

        // And convert every character into lowercase.
		for (int i = 0; i < pattern->len; i++) {
            if (pattern->is_wide) {
                pattern->wide[i] = (wchar_t) towlower(pattern->wide[i]);
            } else {
                pattern->stnd[i] = (char) tolower((u_char) pattern->stnd[i]);
            }
		}
	}

    // Initialize good_shifts attribute.
    comp->good_shifts = malloc(sizeof(unsigned int) * pattern->len);
    if (comp->good_shifts == NULL) {
        if (comp->is_icase_set) {
            string_free(pattern);
        }
        return (REG_ESPACE);
    }

	// Calculate good shifts into the newly created table.
	int res = calculate_good_shifts(comp->good_shifts, pattern);

	// If the case was ignored or an error occurred, the copy is freed.
	if (comp->is_icase_set || res != REG_OK) {
		string_free(pattern);
	}

	return res;
}

// Internal compile function without bm_comp_init call, as this init
// function should only be called once, at the start of either
// bm_compile_literal or bm_compile_full
static int
compile_literal(bm_comp *comp, string patt, int cflags)
{
    // Return early if the pattern is a zero-length string.
    if (patt.len == 0) {
        string_borrow(&comp->pattern, NULL, 0, patt.is_wide);
        comp->has_glob_match = true;
        return (REG_OK);
    }

    // The options set by these flags won't work if MB_CUR_MAX > 1.
    if (comp->is_icase_set && MB_CUR_MAX > 1) {
        return (REG_BADPAT);
    }

    // Create a long-enough string for the literal pattern.
    bool success = string_duplicate(&comp->pattern, patt);
    if (!success) {
        return (REG_ESPACE);
    }

    int ret = (comp->pattern.is_wide)
        ? fill_badc_shifts_wide(comp)
        : fill_badc_shifts_stnd(comp);
    if (ret == REG_OK) {
        ret = fill_good_shifts(comp);
    }

	if (ret != REG_OK) {
        bm_comp_free(comp);
		return ret;
	}

	return (REG_OK);
}

int
bm_compile_literal(bm_comp *comp, string patt, int cflags)
{
    bm_comp_init(comp, cflags);
    return compile_literal(comp, patt, cflags);
}

// Strips escape characters from the given str pattern and outputs the
// literal text result into out_str.
// Input flags can be specified in in_flags, while output flags are set in the
// given comp compilation struct.
static int
strip_specials(string str, string *out_str, int in_flags, bm_comp *comp)
{
	ssize_t len = str.len;

	// If the first character is ^, set the given flag and continue.
	if (string_has_char_at(str, 0, '^', L'^')) {
		comp->has_bol_anchor = true;
        string_offset(&str, 1);
	}

	// If the last character is a $ with special meaning, do the same.
	if (
        (len >= 1 && string_has_char_at(str, len - 1, '$', L'$'))
        && (len == 1 || !string_has_char_at(str, len - 2, '\\', L'\\'))
	) {
		comp->has_eol_anchor = true;
		str.len--;
	}

	regex_parser parser;
	parser.escaped = false;
	parser.extended = in_flags & REG_EXTENDED;

	// Traverse the given pattern:
	for (ssize_t i = 0; i < len; i++) {
        parse_result result;
        if (str.is_wide) {
            result = parse_wchar(&parser, str.wide[i]);
        } else {
            result = parse_char(&parser, str.stnd[i]);
        }

		switch (result) {
			case NORMAL_CHAR:
                string_append_from(out_str, str, i);
				break;
			case NORMAL_NEWLINE:
                string_append(out_str, '\n', L'\n');
				break;
			case SHOULD_SKIP:
				break;
			// If any special character was found, we abort.
			default:
				return (REG_BADPAT);
		}
	}

	// End the pattern with terminating null character and set length.
    string_null_terminate(out_str);

	return (REG_OK);
}

int
bm_compile_full(bm_comp *comp, string patt, int cflags)
{
    bm_comp_init(comp, cflags);

	// We'll execute literal preprocessing on a clean pattern.
    string clean_pattern;
    bool success = string_duplicate(&clean_pattern, patt);
    if (!success) {
        return (REG_ESPACE);
    }

    // Set the length to zero so that the string_append functions work properly.
    // Its capacity is still patt.len, which has to be enough.
    clean_pattern.len = 0;

    int ret = strip_specials(patt, &clean_pattern, cflags, comp);
	if (ret != REG_OK) {
        string_free(&clean_pattern);
		return ret;
	}

	// Execute literal preprocessing.
	ret = compile_literal(comp, clean_pattern, cflags);

	string_free(&clean_pattern);
	return ret;
}
