/*-
 * Copyright (C) 2011 Gabor Kovesdan <gabor@FreeBSD.org>
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
#include <string.h>
#include <wctype.h>

#include "config.h"
#include "frec.h"

/*
 * A full regex implementation requires a finite state automaton
 * and using an automaton is always about a trade-off. A DFA is
 * fast but complex and requires more memory because of the
 * high number of states. NFA is slower but simpler and uses less
 * memory. Regular expression matching is an underlying common task
 * that is required to be efficient but correctness, clean and
 * maintanable code are also requirements. So what we do is using
 * an NFA implementation and heuristically locate the possible matches
 * with a cheaper algorithm and only apply the heavy one to the
 * possibly matching segments. This allows us to benefit from the
 * advantages of an NFA implementation reducing the effect of the
 * performance impact.
 */

/*
 * Parses bracket expression seeking to the end of the enclosed text.
 * The parameters are the opening (oe) and closing elements (ce).
 * Can handle nested bracket expressions.
 */
#define PARSE_UNIT(oe, ce)						\
do {									\
	int level = 0;							\
									\
	while (i < len) {						\
		if (regex[i] == oe)					\
			level++;					\
		else if (regex[i] == ce)				\
			level--;					\
		if (level == 0)						\
			break;						\
		i++;							\
	}								\
} while (0)

#define PARSE_BRACKETS							\
do {									\
	i++;								\
	if (regex[i] == L'^')						\
		i++;							\
	if (regex[i] == ']')						\
		i++;							\
									\
	for (; i < len; i++) {						\
		if (regex[i] == L'[')					\
			return (REG_BADPAT);				\
		if (regex[i] == L']')					\
			break;						\
	}								\
} while (0)

/*
 * Finishes a segment (fixed-length text fragment).
 */
#define END_SEGMENT(varlen)						\
do {									\
	if (varlen)							\
		tlen = -1;						\
	st = i + 1;							\
	escaped = false;						\
	goto end_segment;						\
} while (0)

#define STORE_CHAR							\
do {									\
	heur[pos++] = regex[i];						\
	escaped = false;						\
	tlen = (tlen == -1) ? -1 : tlen + 1;				\
	continue;							\
} while (0)

#define DEC_POS pos = (pos == 0) ? 0 : pos - 1;

/*
 * Parses a regular expression and constructs a heuristic in heur_t and
 * returns REG_OK if successful or the corresponding error code if a
 * heuristic cannot be constructed.
 */
int
frec_proc_heur(heur_t *h, const wchar_t *regex, size_t len, int cflags)
{
	wchar_t **arr, *heur;
	wchar_t **farr = NULL;
	char **barr = NULL;
	size_t *bsiz = NULL, *fsiz = NULL;
	size_t length[MAX_FRAGMENTS];
	size_t i, j = 0, st = 0;
	ssize_t tlen = 0;
	int errcode, pos = 0;
	bool escaped = false;

	DEBUG_PRINT("enter");

	heur = malloc(len * sizeof(wchar_t));
	if (!heur)
		return (REG_ESPACE);

	arr = malloc(MAX_FRAGMENTS * sizeof(wchar_t *));
	if (!arr) {
		errcode = REG_ESPACE;
		goto err;
	}

	h->type = HEUR_ARRAY;

	while (true) {

		/*
		 * Process the pattern char-by-char.
		 *
		 * i: position in regex
		 * j: number of fragment
		 * st: start offset of current segment (fixed-length fragment)
		 *     to be processed
		 * pos: current position (and length) in the temporary space where
		 *      we copy the current segment
		 */
		for (i = st; i < len; i++) {
			switch (regex[i]) {

			/*
			 * Bracketed expression ends the segment or the
			 * brackets are treated as normal if at least the opening
			 * bracket is escaped.
			 */
			case L'[':
				if (escaped)
					STORE_CHAR;
				else {
					PARSE_BRACKETS;
					END_SEGMENT(true);
				}
				continue;

			/*
			 * If a repetition marker, erases the repeting character
			 * and terminates the segment, otherwise treated as a normal
			 * character.
			 */
			case L'{':
				if (escaped && (i == 1))
					STORE_CHAR;
				else if (i == 0)
					STORE_CHAR;

				PARSE_UNIT('{', '}');
				if (escaped ^ (cflags & REG_EXTENDED)) {
					DEC_POS;
					END_SEGMENT(true);
				} else
					STORE_CHAR;
				continue;

			/*
			 * Terminates the current segment when escaped,
			 * otherwise treated as a normal character.
			 */
			case L'(':
				if (escaped ^ (cflags & REG_EXTENDED)) {
					PARSE_UNIT('(', ')');
					END_SEGMENT(true);
				} else
					STORE_CHAR;
				continue;

			/*
			 * Sets escaped flag.
			 * Escaped escape is treated as a normal character.
			 * (This is also the GNU behaviour.)
			 */
			case L'\\':
				if (escaped)
					STORE_CHAR;
				else
					escaped = true;
				continue;

			/*
			 * BRE: If not the first character and not escaped, erases the
			 * last character and terminates the segment.
			 * Otherwise treated as a normal character.
			 * ERE: Skipped if first character (GNU), rest is like in BRE.
			 */
			case L'*':
				if (escaped || (!(cflags & REG_EXTENDED) && (i == 0)))
					STORE_CHAR;
				else if ((i != 0)) {
					DEC_POS;
					END_SEGMENT(true);
				}
				continue;

			/*
			 * In BRE, it is a normal character, behavior is undefined
			 * when escaped.
			 * In ERE, it is special unless escaped. Terminate segment
			 * when not escaped. Last character is not removed because it
			 * must occur at least once. It is skipped when first
			 * character (GNU).
			 */
			case L'+':
				if ((cflags & REG_EXTENDED) && (i == 0))
					continue;
				else if ((cflags & REG_EXTENDED) ^ escaped)
					END_SEGMENT(true);
				else
					STORE_CHAR;
				continue;

			/*
			 * In BRE, it is a normal character, behavior is undefined
			 * when escaped.
			 * In ERE, it is special unless escaped. Terminate segment
			 * when not escaped. Last character is removed. Skipped when
			 * first character (GNU).
			 */
			case L'?':
				if ((cflags & REG_EXTENDED) && (i == 0))
					continue;
				if ((cflags & REG_EXTENDED) ^ escaped) {
					DEC_POS;
					END_SEGMENT(true);
				} else
					STORE_CHAR;
				continue;

			/*
			 * Fail if it is an ERE alternation marker.
			 */
			case L'|':
				if ((cflags & REG_EXTENDED) && !escaped) {
					errcode = REG_BADPAT;
					goto err;
				} else if (!(cflags & REG_EXTENDED) && escaped) {
					errcode = REG_BADPAT;
					goto err;
				} else
					STORE_CHAR;
				continue;

			case L'.':
				if (escaped)
					STORE_CHAR;
				else {
					tlen = (tlen == -1) ? -1 : tlen + 1;
					END_SEGMENT(false);
				}
				continue;

			/*
			 * If escaped, terminates segment.
			 * Otherwise adds current character to the current segment
			 * by copying it to the temporary space.
			 */
			default:
				if (escaped)
					END_SEGMENT(true);
				else
					STORE_CHAR;
				continue;
			}
		}

		/* We are done with the pattern if we got here. */
		st = len;
 
end_segment:
		/* Check if pattern is open-ended */
		if (st == len && pos == 0) {
			if (j == 0) {
				errcode = REG_BADPAT;
				goto err;
			}
			h->type = HEUR_PREFIX_ARRAY;
			goto ok;
		} else if (pos == 0)
			/* Continue if we got some variable-length part */
			continue;

		/* Too many fragments - should never happen but to be safe */
		if (j == MAX_FRAGMENTS) {
			errcode = REG_BADPAT;
			goto err;
		}

		/* Alloc space for fragment and copy it */
		arr[j] = malloc((pos + 1) * sizeof(wchar_t));
		if (!arr[j]) {
			errcode = REG_ESPACE;
			goto err;
		}
		heur[pos] = L'\0';
		memcpy(arr[j], heur, (pos + 1) * sizeof(wchar_t));
		length[j] = pos;
		j++;
		pos = 0;

		/* Check whether there is more input */
		if (st == len)
			goto ok;
	}

ok:
	{
		size_t idx = 0, m = 0;

		h->tlen = tlen;

		/* Look up maximum length fragment */
		for (i = 1; i < j; i++)
			m = (length[i] > length[m]) ? i : m;

		DEBUG_PRINTF("longest fragment: %ls, index: %zu", arr[m], m);

		/* Will hold the final fragments that we actually use */
		farr = malloc(4 * sizeof(wchar_t *));
		if (!farr) {
			errcode = REG_ESPACE;
			goto err;
		}

		/* Sizes for the final fragments */
		fsiz = malloc(4 * sizeof(size_t));
		if (!fsiz) {
			errcode = REG_ESPACE;
			goto err;
		}

		/*
		 * Only save the longest fragment if match is line-based.
		 */
		if (cflags & REG_NEWLINE) {
			h->type = HEUR_LONGEST;
			farr[0] = arr[m];
			arr[m] = NULL;
			fsiz[0] = length[0];
			farr[1] = NULL;
		} else
		/*
		 * Otherwise try to save up to three fragments: beginning, longest
		 * intermediate pattern, ending.  If either the beginning or the
		 * ending fragment is longer than any intermediate fragments, we will
		 * not save any intermediate one.  The point here is to always maximize
		 * the possible shifting when searching in the input.  Measurements
		 * have shown that the eager approach works best.
		 */

		DEBUG_PRINTF("strategy: %s", h->type == HEUR_PREFIX_ARRAY ?
			"HEUR_PREFIX_ARRAY" : "HEUR_ARRAY");

		/* Always start by saving the beginning */
		farr[idx] = arr[0];
		 DEBUG_PRINTF("fragment %zu: %ls", idx, farr[idx]);
		arr[0] = NULL;
		fsiz[idx++] = length[0];

		/*
		 * If the longest pattern is not the beginning nor the ending,
		 * save it.
		 */
		if ((m != 0) && (m != j - 1)) {
			farr[idx] = arr[m];
			DEBUG_PRINTF("fragment %zu: %ls", idx, farr[idx]);
			fsiz[idx++] = length[m];
			arr[m] = NULL;
		}

		/*
		 * If we have an ending pattern (even if the pattern is
		 * "open-ended"), save it.
		 */
		if (j > 1) {
			farr[idx] = arr[j - 1];
			DEBUG_PRINTF("fragment %zu: %ls", idx, farr[idx]);
			fsiz[idx++] = length[j - 1];
			arr[j - 1] = NULL;
		}

		farr[idx] = NULL;
	}

	/* Once necessary pattern saved, free original array */
	for (i = 0; i < j; i++)
		if (arr[i])
			free(arr[i]);
	free(arr);

	/*
	 * Store the array in single-byte and wide char forms in the
	 * heur_t struct for later reuse.  When supporting wchar_t
	 * convert the fragments to single-byte string because
	 * conversion is probably faster than processing the patterns
	 * again in single-byte form.
	 */
	barr = malloc(4 * sizeof(char *));
	if (!barr) {
		errcode = REG_ESPACE;
		goto err;
	}

	bsiz = malloc(4 * sizeof(size_t));
	if (!bsiz) {
		errcode = REG_ESPACE;
		goto err;
	}

	for (i = 0; farr[i] != NULL; i++) {
		bsiz[i] = wcstombs(NULL, farr[i], 0);
		barr[i] = malloc(bsiz[i] + 1);
		if (!barr[i]) {
			errcode = REG_ESPACE;
			goto err;
		}
		wcstombs(barr[i], farr[i], bsiz[i]);
		barr[i][bsiz[i]] = '\0';
      	}
	barr[i] = NULL;

	h->warr = farr;
	h->wsiz = fsiz;
	h->arr = barr;
	h->siz = bsiz;

	/*
	 * Compile all the useful fragments for actual matching.
	 */
	h->heurs = malloc(4 * sizeof(fastmatch_t *));
	if (!h->heurs) {
		errcode = REG_ESPACE;
		goto err;
	}
	for (i = 0; farr[i] != NULL; i++) {
		h->heurs[i] = malloc(sizeof(fastmatch_t));
		if (!h->heurs[i]) {
			errcode = REG_ESPACE;
			goto err;
		}
		errcode = frec_proc_literal(h->heurs[i], farr[i], fsiz[i],
			barr[i], bsiz[i], 0);
		if (errcode != REG_OK) {
			errcode = REG_BADPAT;
			goto err;
		}
	}

	h->heurs[i] = NULL;
	errcode = REG_OK;
	goto finish;

err:
	if (barr) {
		for (i = 0; i < 4; i++)
			if (barr[i])
				free(barr[i]);
		free(barr);
	}

	if (bsiz)
		free(bsiz);

	if (farr) {
		for (i = 0; i < j; i++)
			if (farr[i])
				free(farr[i]);
		free(farr);
	}

	if (fsiz)
		free(fsiz);

	if (h->heurs) {
		for (i = 0; h->heurs[i] != NULL; i++)
			frec_free_fast(h->heurs[i]);
		free(h->heurs);
	}
finish:
	if (heur)
		free(heur);
	return (errcode);
}

/*
 * Frees a heuristic.
 */
void
frec_free_heur(heur_t *h)
{

	for (int i = 0; h->heurs[i] != NULL; i++)
		frec_free_fast(h->heurs[i]);

	if (h->arr) {
		for (int i = 0; h->arr[i] != NULL; i++)
			if (h->arr[i])
				free(h->arr[i]);
		free(h->arr);
	}

	if (h->warr) {
		for (int i = 0; h->warr[i] != NULL; i++)
			if (h->warr[i])
				free(h->warr[i]);
		free(h->warr);
	}
}
