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
#include <string.h>
#include <wctype.h>

#include "boyer-moore.h"

static int max(int a, int b) { return (a > b) ? a : b; }
static int min(int a, int b) { return (a < b) ? a : b; }

static bool bol_matches_stnd(const char *text, size_t start_pos) {
    return start_pos == 0 || text[start_pos - 1] == '\n';
}
static bool bol_matches_wide(const wchar_t *text, size_t start_pos) {
    return start_pos == 0 || text[start_pos - 1] == L'\n';
}

static bool eol_matches_stnd(const char *text, size_t text_len, size_t end_pos) {
    return end_pos == text_len || text[end_pos] == '\n';
}
static bool eol_matches_wide(const wchar_t *text, size_t text_len, size_t end_pos) {
    return end_pos == text_len || text[end_pos] == L'\n';
}

// TODO Not properly configured for case ignoring

static int
exec_turbo_bm_stnd(
    bm_match_t result[], size_t nmatch,
    bm_preproc_t *prep, const char *text, size_t len,
    bool store_matches)
{
    int res_cnt = 0; /* A new match will be written to this index of result. */

    int srch_pos = 0; /* The current start pos in the text of our search. */
    int pat_len = prep->stnd.len; /* The length of the pattern. */
    unsigned int *good_shs = prep->stnd.goods_shifts;
    unsigned int *badc_shs = prep->stnd.badc_shifts;

    int shift = pat_len; /* This variable is used to shift srch_pos. */
    int prev_suf = 0; /* The length of the previously matched suffix. */

    /* While there is a long enough text to search. */
    while (srch_pos <= len - pat_len) {
        /* Find the first mismatched character pair (from the end). */
        int i = pat_len - 1;
        while (i >= 0 && prep->stnd.pattern[i] == text[srch_pos + i]) {
            i--;
            if (prev_suf != 0 && i == pat_len - 1 - shift) {
                i -= prev_suf;
            }
        }

        /* If i < 0, the whole pattern matched with the text,
         * if not, there was a mismatch and we can shift the search. */
        if (i < 0) {
            if ((!prep->f_linebegin || bol_matches_stnd(text, srch_pos)) &&
                (!prep->f_lineend || eol_matches_stnd(text, len, srch_pos + pat_len)) &&
                store_matches) {
                result[res_cnt].soffset = srch_pos;
                result[res_cnt].eoffset = srch_pos + pat_len;
                res_cnt++;
            }

            /* If nmatch was 0, or we found the required match amount, return. */
            if (res_cnt >= nmatch) {
                return (REG_OK);
            }
            
            shift = good_shs[0];
            prev_suf = pat_len - shift;
        } else {
            /* Apply Turbo-BM calculations. */
            int v = pat_len - 1 - i;
            int turbo_shift = prev_suf - v;
            int bad_shift = badc_shs[text[srch_pos + i]] - v;
            
            shift = max(turbo_shift, bad_shift);
            shift = max(shift, good_shs[i]);

            if (shift == good_shs[i]) {
                prev_suf = min(pat_len - shift, v);
            } else {
                if (turbo_shift < bad_shift) {
                    shift = max(shift, prev_suf + 1);
                }
                prev_suf = 0;
            }
        }
        srch_pos += shift;
    }

    return (res_cnt == 0) ? (REG_NOMATCH) : (REG_OK);
}

static int
exec_turbo_bm_wide(
    bm_match_t result[], size_t nmatch,
    bm_preproc_t *prep, const wchar_t *text, size_t len,
    bool store_matches)
{
    int res_cnt = 0; /* A new match will be written to this index of result. */

    int srch_pos = 0; /* The current start pos in the text of our search. */
    int pat_len = prep->wide.len; /* The length of the pattern. */
    unsigned int *good_shs = prep->wide.goods_shifts;
    hashtable *badc_shs = prep->wide.badc_shifts;

    int shift = pat_len; /* This variable is used to shift srch_pos. */
    int prev_suf = 0; /* The length of the previously matched suffix. */

    /* While there is a long enough text to search. */
    while (srch_pos <= len - pat_len) {
        /* Find the first mismatched character pair (from the end). */
        int i = pat_len - 1;
        while (i >= 0 && prep->wide.pattern[i] == text[srch_pos + i]) {
            i--;
            if (prev_suf != 0 && i == pat_len - 1 - shift) {
                i -= prev_suf;
            }
        }

        /* If i < 0, the whole pattern matched with the text,
         * if not, there was a mismatch and we can shift the search. */
        if (i < 0) {
            if ((!prep->f_linebegin || bol_matches_wide(text, srch_pos)) &&
                (!prep->f_lineend || eol_matches_wide(text, len, srch_pos + pat_len)) &&
                store_matches) {
                result[res_cnt].soffset = srch_pos;
                result[res_cnt].eoffset = srch_pos + pat_len;
                res_cnt++;
            }

            /* If nmatch was 0, or we found the required match amount, return. */
            if (res_cnt >= nmatch) {
                return (REG_OK);
            }
            
            shift = good_shs[0];
            prev_suf = pat_len - shift;
        } else {
            /* Apply Turbo-BM calculations. */
            int v = pat_len - 1 - i;
            int turbo_shift = prev_suf - v;

            wchar_t key = text[srch_pos + i];
            int value;
            hashtable_get(badc_shs, &key, &value);

            int bad_shift = value - v;
            
            shift = max(turbo_shift, bad_shift);
            shift = max(shift, good_shs[i]);

            if (shift == good_shs[i]) {
                prev_suf = min(pat_len - shift, v);
            } else {
                if (turbo_shift < bad_shift) {
                    shift = max(shift, prev_suf + 1);
                }
                prev_suf = 0;
            }
        }
        srch_pos += shift;
    }

    return (res_cnt == 0) ? (REG_NOMATCH) : (REG_OK);
}

int
bm_execute_stnd(
    bm_match_t result[], size_t nmatch,
    bm_preproc_t *prep, const char *_text, size_t _len,
    bool no_bol_anchor, bool no_eol_anchor)
{
    bool store_matches = !prep->f_nosub && nmatch != 0;

    const char *text = _text;
    size_t len = _len;

    if (len == (size_t) - 1) {
        len = strlen(text);
    }

    if (prep->f_matchall) {
        if (store_matches) {
            result[0].soffset = 0;
            result[0].eoffset = len;
        }
        return (REG_OK);
    }

    if (prep->f_linebegin && no_bol_anchor) {
        text++;
        len--;
    }

    if (prep->f_lineend && no_eol_anchor) {
        len--;
    }

    if (prep->stnd.len > len) {
        return (REG_NOMATCH);
    }

    return exec_turbo_bm_stnd(result, nmatch, prep, text, len, store_matches);
}

int
bm_execute_wide(
    bm_match_t result[], size_t nmatch,
    bm_preproc_t *prep, const wchar_t *_text, size_t _len,
    bool no_bol_anchor, bool no_eol_anchor)
{
    bool store_matches = !prep->f_nosub && nmatch != 0;

    const wchar_t *text = _text;
    size_t len = _len;

    if (len == (size_t) - 1) {
        len = wcslen(text);
    }

    if (prep->f_matchall) {
        if (store_matches) {
            result[0].soffset = 0;
            result[0].eoffset = len;
        }
        return (REG_OK);
    }

    if (prep->f_linebegin && no_bol_anchor) {
        text++;
        len--;
    }

    if (prep->f_lineend && no_eol_anchor) {
        len--;
    }

    if (prep->wide.len > len) {
        return (REG_NOMATCH);
    }

    return exec_turbo_bm_wide(result, nmatch, prep, text, len, store_matches);
}
