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

#include <frec-config.h>
#include <string-type.h>
#include <string.h>

#include "bm.h"

// Utility functions.
static ssize_t max(ssize_t a, ssize_t b) { return (a > b) ? a : b; }
static ssize_t min(ssize_t a, ssize_t b) { return (a < b) ? a : b; }

// Utility function. Asserts that str[at] equals stnd or wide in the correct
// sub-array.
static bool
string_has_newline_at(string str, ssize_t at)
{
    if (str.is_wide) {
        return str.wide[at] == L'\n';
    } else {
        return str.stnd[at] == '\n';
    }
}

// Assert that str1[pos1] equals str2[pos2].
static bool
compare_strings_at(string str1, ssize_t pos1, string str2, ssize_t pos2)
{
    if (str1.is_wide) {
        return str1.wide[pos1] == str2.wide[pos2];
    } else {
        return str1.stnd[pos1] == str2.stnd[pos2];
    }
}

// TODO Not properly configured for case ignoring

// Executes the turbo Boyer-Moore algorithm on the given text with the given
// length, and stores the result.
// Will not store the match if the respective flag is set.
static int
exec_turbo_bm(
    frec_match_t *result, const bm_comp *comp, string text, bool store_matches
) {
    const string patt = comp->pattern; // Shortcut variable to pattern.

    ssize_t srch_pos = 0; // The current start pos in the text, can change.

    ssize_t shift = patt.len; // Used to shift srch_pos, can change.
    ssize_t prev_suf = 0; // The matched suffix length on our previous try.

    while (srch_pos + patt.len <= text.len) {
        // Find the first mismatched character pair (from the end).
        ssize_t i = patt.len - 1;
        while (i >= 0 && compare_strings_at(patt, i, text, srch_pos + i)) {
            i--;
            if (prev_suf != 0 && i == patt.len - 1 - shift) {
                i -= prev_suf;
            }
        }

        // If i < 0, the whole pattern matched with the text.
        // Otherwise, there was a mismatch, and we can shift the search.
        if (i < 0) {
            // Check BOL and EOL. If they have to match, our current
            // candidate may not be correct.
            bool can_return = true;
            if (comp->has_bol_anchor) {
                can_return &= ((srch_pos == 0)
                    || string_has_newline_at(text, srch_pos - 1));
            }

            if (comp->has_eol_anchor) {
                ssize_t end = srch_pos + patt.len;
                can_return &= ((end == text.len)
                    || string_has_newline_at(text, end));
            }

            // Only return if the above checks succeeded.
            if (can_return) {
                // If we don't have to store matches, just return.
                if (!store_matches) {
                    return (REG_OK);
                }

                // Else set the resulting offsets and return.
                result->soffset = srch_pos;
                result->eoffset = srch_pos + patt.len;
                return (REG_OK);
            }

            // Otherwise, continue the while loop.
        } else {
            // Apply Turbo-BM calculations.
            ssize_t v = patt.len - 1 - i;
            ssize_t turbo_shift = prev_suf - v;

            ssize_t value;

            if (text.is_wide) {
                wchar_t key = text.wide[srch_pos + i];
                int ret = hashtable_get(comp->bad_shifts_wide, &key, &value);
                // If the char is not present in the hash, this shift value
                // equals the length of the pattern.
                if (ret != HASH_OK) {
                    value = patt.len;
                }
            } else {
                char key = text.stnd[srch_pos + i];
                // If the key is invalid (< 0), no pattern can match with it.
                value = (key < 0)
                    ? patt.len
                    : comp->bad_shifts_stnd[key];
            }

            ssize_t bad_shift = value - v;
            
            shift = max(turbo_shift, bad_shift);
            shift = max(shift, comp->good_shifts[i]);

            if (shift == comp->good_shifts[i]) {
                prev_suf = min(patt.len - shift, v);
            } else {
                if (turbo_shift < bad_shift) {
                    shift = max(shift, prev_suf + 1);
                }
                prev_suf = 0;
            }
        }
        srch_pos += shift;
    }

    // We only exit the while loop if we reached the end of the text.
    return (REG_NOMATCH);
}

int
bm_execute(frec_match_t *result, const bm_comp *comp, string text, int eflags)
{
    // Entry condition: the comp was created with the same char type as text.
    if (comp->pattern.is_wide != text.is_wide) {
        return (REG_BADPAT);
    }

    // Set bool fields.
    bool store_matches = !comp->is_nosub_set && result != NULL;
    bool no_bol_anchor = eflags & REG_NOTBOL;
    bool no_eol_anchor = eflags & REG_NOTEOL;

    // If the prep pattern matches everything, return early.
    if (comp->has_glob_match) {
        if (store_matches) {
            result[0].soffset = 0;
            result[0].eoffset = text.len;
        }
        return (REG_OK);
    }

    // If BOL and EOL can't match the start and end of the text, we won't
    // accept any matches that are at the very beginning or the very end.
    if (comp->has_bol_anchor && no_bol_anchor) {
        string_offset(&text, 1);
    }
    if (comp->has_eol_anchor && no_eol_anchor) {
        text.len--;
    }

    // If the original pattern is longer than the text, return.
    if (comp->pattern.len > text.len) {
        return (REG_NOMATCH);
    }

    // Execute BM algorithm.
    return exec_turbo_bm(result, comp, text, store_matches);
}
