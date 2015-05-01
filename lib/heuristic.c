/*-
 * Copyright (C) 2011, 2015 Gabor Kovesdan <gabor@FreeBSD.org>
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
		else if (regex[i] == L'.')				\
			has_dot = true;					\
		else if (regex[i] == L'\n')				\
			has_lf = true;					\
		if (level == 0)						\
			break;						\
		i++;							\
	}								\
} while (0)

#define PARSE_BRACKETS							\
do {									\
	i++;								\
	if (regex[i] == L'^') {						\
		has_negative_set = true;				\
		i++;							\
	}								\
	if (regex[i] == L']')						\
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

#define STORE(c)							\
do {									\
        heur[pos++] = c;						\
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
	bool has_dot = false;
	bool has_lf = false;
	bool has_negative_set = false;

	DEBUG_PRINT("enter");

	heur = malloc(len * sizeof(wchar_t));
	if (!heur)
		return (REG_ESPACE);

	arr = malloc(MAX_FRAGMENTS * sizeof(wchar_t *));
	if (!arr) {
		errcode = REG_ESPACE;
		goto err;
	}

	h->type = HEUR_PREFIX;

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
					has_dot = true;
					tlen = (tlen == -1) ? -1 : tlen + 1;
					END_SEGMENT(false);
				}
				continue;
			case L'\n':
				has_lf = true;
				STORE_CHAR;
				continue;

			/*
			 * If escaped, terminates segment.
			 * Otherwise adds current character to the current segment
			 * by copying it to the temporary space.
			 */
			default:
				if (escaped) {
					if (regex[i] == L'n') {
						has_lf = true;
						STORE(L'\n');
					}
					END_SEGMENT(true);
				} else
					STORE_CHAR;
				continue;
			}
		}

		/* We are done with the pattern if we got here. */
		st = len;
 
end_segment:

		if (st == len && pos == 0) {
			if (j == 0) {
				errcode = REG_BADPAT;
				goto err;
			}
			goto ok;
		}

		/* Continue if we got some variable-length part */
		if (pos == 0)
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
		size_t m = 0;

		h->tlen = tlen;

		if ((cflags & REG_NEWLINE) ||
			(!has_dot && !has_lf && !has_negative_set))
			h->type = HEUR_LONGEST;
		DEBUG_PRINTF("strategy: %s", h->type == HEUR_LONGEST ?
			"HEUR_LONGEST" : "HEUR_PREFIX");


		/* Will hold the final heuristic that we actually use */
		farr = malloc(1 * sizeof(wchar_t *));
		if (!farr) {
			errcode = REG_ESPACE;
			goto err;
		}

		/* Sizes for the final fragments */
		fsiz = malloc(1 * sizeof(size_t));
		if (!fsiz) {
			errcode = REG_ESPACE;
			goto err;
		}


		if (h->type == HEUR_PREFIX) {
			/* Save beginning */
			farr[0] = arr[0];
			fsiz[0] = length[0];
			arr[0] = NULL;
			DEBUG_PRINTF("beginning fragment: %ls", farr[0]);
		} else if (h->type == HEUR_LONGEST) {
			for (i = 0; i < j; i++)
				m = (length[i] > length[m]) ? i : m;

			farr[0] = arr[m];
			fsiz[0] = length[m];
			arr[m] = NULL;
			DEBUG_PRINTF("longest fragment: %ls, index: %zu", farr[0], m);
		}
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
	barr = malloc(1 * sizeof(char *));
	if (!barr) {
		errcode = REG_ESPACE;
		goto err;
	}

	bsiz = malloc(1 * sizeof(size_t));
	if (!bsiz) {
		errcode = REG_ESPACE;
		goto err;
	}

	bsiz[0] = wcstombs(NULL, farr[0], 0);
	barr[0] = malloc(bsiz[0] + 1);
	if (!barr[0]) {
		errcode = REG_ESPACE;
		goto err;
	}
	wcstombs(barr[0], farr[0], bsiz[0]);
	barr[0][bsiz[0]] = '\0';

	h->warr = farr;
	h->wsiz = fsiz;
	h->arr = barr;
	h->siz = bsiz;

	/*
	 * Compile all the useful fragments for actual matching.
	 */
	h->heurs = malloc(1 * sizeof(fastmatch_t *));
	if (!h->heurs) {
		errcode = REG_ESPACE;
		goto err;
	}

	h->heurs[0] = malloc(sizeof(fastmatch_t));
	if (!h->heurs[0]) {
		errcode = REG_ESPACE;
		goto err;
	}
	errcode = frec_proc_literal(h->heurs[0], farr[0], fsiz[0],
		barr[0], bsiz[0], 0);
	if (errcode != REG_OK) {
		errcode = REG_BADPAT;
		goto err;
	}

	errcode = REG_OK;
	goto finish;

err:
	if (barr) {
		if (barr[0])
			free(barr[0]);
		free(barr);
	}

	if (bsiz)
		free(bsiz);

	if (farr) {
		if (farr[0])
			free(farr[0]);
		free(farr);
	}

	if (fsiz)
		free(fsiz);

	if (h->heurs) {
		if (h->heurs[0] != NULL)
			frec_free_fast(h->heurs[0]);
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
