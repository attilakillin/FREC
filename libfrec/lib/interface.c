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
#include <wchar.h>
#include <string.h>

#include "compile.h"
#include "heuristic.h"
#include "match.h"
#include "frec-internal.h"
#include "string-type.h"

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


static int
execute_match(const void *preg, string text,
    size_t nmatch, frec_match_t pmatch[], int eflags, bool multi)
{
    // Handle REG_STARTEND and optional start and end offsets.
    ssize_t offset_start = 0;
    ssize_t offset_end = text.len;

    if (eflags & REG_STARTEND) {
        offset_start = pmatch[0].soffset;
        offset_end = pmatch[0].eoffset;
    }

    if (offset_start >= offset_end) {
        return (REG_NOMATCH);
    }

    string_offset(&text, offset_start);
    text.len = offset_end - offset_start;

    bool nosub_not_set = true;

    // Execute matching.
    int ret;
    if (multi) {
        nosub_not_set = (((mfrec_t *)preg)->cflags & REG_NOSUB) == 0;
        ret = frec_mmatch(pmatch, nmatch, preg, text, eflags);
    } else {
        nosub_not_set = (((frec_t *)preg)->cflags & REG_NOSUB) == 0;
        ret = frec_match(pmatch, nmatch, preg, text, eflags);
    }

    // Fix offsets that may have been messed up by REG_STARTEND.
	if (ret == REG_OK) {
        if (eflags & REG_STARTEND && nosub_not_set) {
            for (ssize_t i = 0; i < nmatch && pmatch[i].soffset != -1; i++) {
                pmatch[i].soffset += offset_start;
                pmatch[i].eoffset += offset_start;
            }
        }
    }

	return ret;
}

int
frec_regnexec(const frec_t *preg, const char *str, size_t len,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{
    string text;
    string_borrow(&text, str, (ssize_t) len, false);

	return execute_match(preg, text, nmatch, pmatch, eflags, false);
}

int
frec_regexec(
    const frec_t *preg, const char *str,
    size_t nmatch, frec_match_t pmatch[], int eflags
) {
	return frec_regnexec(preg, str, strlen(str), nmatch, pmatch, eflags);
}


int
frec_regwnexec(const frec_t *preg, const wchar_t *str, size_t len,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{
    string text;
    string_borrow(&text, str, (ssize_t) len, true);

	return execute_match(preg, text, nmatch, pmatch, eflags, false);
}

int
frec_regwexec(
    const frec_t *preg, const wchar_t *str,
    size_t nmatch, frec_match_t pmatch[], int eflags
) {
	return frec_regwnexec(preg, str, wcslen(str), nmatch, pmatch, eflags);
}

size_t
frec_regerror(
    int errcode, const frec_t *preg, char *errbuf, size_t errbuf_size
) {
    return _dist_regerror(errcode, &preg->original, errbuf, errbuf_size);
}



int
frec_mregncomp(
    mfrec_t *preg, size_t nr,
    const char **regex, size_t *n, int cflags
) {
    string *patterns = malloc(sizeof(string) * nr);
    if (patterns == NULL) {
        return (REG_ESPACE);
    }

    for (ssize_t i = 0; i < nr; i++) {
        string_borrow(&patterns[i], regex[i], (ssize_t) n[i], false);
    }

	int ret = frec_mcompile(preg, patterns, (ssize_t) nr, cflags);

    free(patterns);
    return ret;
}

int
frec_mregcomp(mfrec_t *preg, size_t nr, const char **regex, int cflags)
{
	size_t *lens = malloc(sizeof(size_t) * nr);
	if (lens == NULL) {
		return (REG_ESPACE);
	}

	for (ssize_t i = 0; i < nr; i++) {
        lens[i] = strlen(regex[i]);
    }

	int ret = frec_mregncomp(preg, nr, regex, lens, cflags);

    free(lens);
	return ret;
}

int
frec_mregwncomp(
    mfrec_t *preg, size_t nr,
    const wchar_t **regex, size_t *n, int cflags
) {
    string *patterns = malloc(sizeof(string) * nr);
    if (patterns == NULL) {
        return (REG_ESPACE);
    }

    for (ssize_t i = 0; i < nr; i++) {
        string_borrow(&patterns[i], regex[i], (ssize_t) n[i], true);
    }

    int ret = frec_mcompile(preg, patterns, (ssize_t) nr, cflags);

    free(patterns);
	return ret;
}

int
frec_mregwcomp(mfrec_t *preg, size_t nr, const wchar_t **regex, int cflags)
{
	size_t *lens = malloc(nr * sizeof(size_t));
    if (lens == NULL) {
        return (REG_ESPACE);
    }

    for (ssize_t i = 0; i < nr; i++) {
        lens[i] = wcslen(regex[i]);
    }

    int ret = frec_mregwncomp(preg, nr, regex, lens, cflags);

    free(lens);
    return ret;
}


int
frec_mregnexec(
    const mfrec_t *preg, const char *str, size_t len,
    size_t nmatch, frec_match_t pmatch[], int eflags
) {
    string text;
    string_borrow(&text, str, (ssize_t) len, false);

	return execute_match(preg, text, nmatch, pmatch, eflags, true);
}

int
frec_mregexec(
    const mfrec_t *preg, const char *str,
    size_t nmatch, frec_match_t pmatch[], int eflags
) {
	return frec_mregnexec(preg, str, strlen(str), nmatch, pmatch, eflags);
}

int
frec_mregwnexec(
    const mfrec_t *preg, const wchar_t *str, size_t len,
    size_t nmatch, frec_match_t pmatch[], int eflags
) {
    string text;
    string_borrow(&text, str, (ssize_t) len, false);

	return execute_match(preg, text, nmatch, pmatch, eflags, true);
}

int
frec_mregwexec(
    const mfrec_t *preg, const wchar_t *str,
    size_t nmatch, frec_match_t pmatch[], int eflags
) {
	return frec_mregwnexec(preg, str, wcslen(str), nmatch, pmatch, eflags);
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
