/*-
 * Copyright (C) 2011, 2015, 2017 Gabor Kovesdan <gabor@FreeBSD.org>
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

typedef struct parser_state {

	/* provided */
	const wchar_t *regex;
	size_t len;
	bool ere;
	bool newline;

	/* calculations */
	size_t procidx;		// iteration var up to len
	bool escaped;
	wchar_t *heur;		// current fragment
	size_t heurpos;		// current fragment len
	wchar_t **fragments;
	size_t fragment_no;
	size_t fragment_lens[MAX_FRAGMENTS];

	/* results */
	ssize_t tlen;		// length limit
	bool has_dot;
	bool has_lf;
	bool has_negative_set;
	bool starts_with_literal;
	char *bheur;		// chosen literal
	size_t blen;
	wchar_t *wheur;		// chosen literal - wide
	size_t wlen;
} parser_state;

inline static int init_state(parser_state *state, const wchar_t *regex,
    size_t len, int cflags) {

	memset(state, 0, sizeof(parser_state));

	state->regex = regex;
	state->len = len;
	state->ere = cflags & REG_EXTENDED;
	state->newline = cflags & REG_NEWLINE;

	state->heur = malloc(len * sizeof(wchar_t));
	if (state->heur == NULL)
		return (REG_ESPACE);
	state->fragments = malloc(MAX_FRAGMENTS * sizeof(wchar_t *));
	if (state->fragments == NULL) {
		free(state->heur);
		return (REG_ESPACE);
	}

	state->starts_with_literal = true;

	return (REG_OK);
}

inline static void free_state(parser_state *state) {

	if (state->heur != NULL)
		free(state->heur);
	if (state->fragments != NULL) {
		for (size_t i = 0; i < state->fragment_no; i++)
			if (state->fragments[i] != NULL)
				free(state->fragments[i]);
		free(state->fragments);
	}
	if (state->bheur != NULL)
		free(state->bheur);
	if (state->wheur != NULL)
		free(state->wheur);
}

/*
 * Parses bracket expression seeking to the end of the enclosed text.
 * The parameters are the opening (oe) and closing elements (ce).
 * Can handle nested bracket expressions.
 */
inline static void parse_unit(parser_state *state, wchar_t oe, wchar_t ce) {
	int level = 0;

	while (state->procidx < state->len) {
		if (state->regex[state->procidx] == oe)
			level++;
		else if (state->regex[state->procidx] == ce)
			level--;
		else if (state->regex[state->procidx] == L'.')
			state->has_dot = true;
		else if (state->regex[state->procidx] == L'\n')
			state->has_lf = true;
		if (level == 0)
			break;
		state->procidx++;
	}
}

inline static int parse_brackets(parser_state *state) {

	state->procidx++;
	if (state->regex[state->procidx] == L'^') {
		state->has_negative_set = true;
		state->procidx++;
	}

	for (; state->procidx < state->len; state->procidx++) {
		if (state->regex[state->procidx] == L'[')
			return (REG_BADPAT);
		if (state->regex[state->procidx] == L']')
			break;
	}
	return (REG_OK);
}

/*
 * Finishes a segment (fixed-length text fragment).
 */
inline static int end_segment(parser_state *state, bool varlen) {

	if ((state->fragment_no == 0) && (state->heurpos == 0))
		state->starts_with_literal = false;
	if (varlen)
		state->tlen = -1;
	state->escaped = false;

	if (state->procidx >= state->len && state->heurpos == 0)
		return (state->fragment_no == 0 ? REG_BADPAT : REG_OK);

	/* Continue if we got some variable-length part */
	if (state->heurpos == 0)
		return (REG_OK);

	/* Too many fragments - should never happen but to be safe */
	if (state->fragment_no == MAX_FRAGMENTS)
		return (REG_BADPAT);

	/* Alloc space for fragment and copy it */
	state->fragments[state->fragment_no] = malloc((state->heurpos + 1) * sizeof(wchar_t));
	if (state->fragments[state->fragment_no] == NULL)
		return (REG_ESPACE);
	state->heur[state->heurpos] = L'\0';
	memcpy(state->fragments[state->fragment_no], state->heur, (state->heurpos + 1) * sizeof(wchar_t));
	state->fragment_lens[state->fragment_no] = state->heurpos;
	state->fragment_no++;
	state->heurpos = 0;

	return (REG_OK);
}

inline static void store_char(parser_state *state) {

	state->heur[state->heurpos++] = state->regex[state->procidx];
	state->escaped = false;
	state->tlen = (state->tlen == -1) ? -1 : state->tlen + 1;
}

inline static void store(parser_state *state, wchar_t c) {

        state->heur[state->heurpos++] = c;
        state->escaped = false;
        state->tlen = (state->tlen == -1) ? -1 : state->tlen + 1;
}

inline static void dec_pos(parser_state *state) {

	state->heurpos = (state->heurpos == 0) ? 0 : state->heurpos - 1;
}

inline static int fill_heuristics(parser_state *state, heur_t *h) {

	h->tlen = state->tlen;

#ifdef WITH_DEBUG
	for (size_t i = 0; i < state->fragment_no; i++)
		DEBUG_PRINTF("Fragment %zu - %ls", i, state->fragments[i]);
	DEBUG_PRINTF("Length limit: %zu", state->tlen);
	DEBUG_PRINTF("May contain lf: %c",
	    state->has_dot || state->has_lf || state->has_negative_set
	    ? 'y' : 'n');
	DEBUG_PRINTF("Starts with literal: %c", state->starts_with_literal
	    ? 'y' : 'n');
#endif


	/* Choose strategy */
	if (state->tlen > 0)
		h->type = HEUR_LONGEST;
	else if (state->newline ||
	    (!state->has_dot && !state->has_lf && !state->has_negative_set))
		h->type = HEUR_LONGEST;
	else if (state->starts_with_literal)
		h->type = HEUR_PREFIX;
	else
		return (REG_BADPAT);

	if (h->type == HEUR_PREFIX) {
		/* Save beginning */
		state->wheur = state->fragments[0];
		state->wlen = state->fragment_lens[0];
		state->fragments[0] = NULL;
	} else {
		size_t m = 0;

		for (size_t i = 1; i < state->fragment_no; i++)
			m = (state->fragment_lens[i] > state->fragment_lens[m]) ? i : m;

		state->wheur = state->fragments[m];
		state->wlen = state->fragment_lens[m];
		state->fragments[m] = NULL;
	}

	state->blen = wcstombs(NULL, state->wheur, 0);
	state->bheur = malloc(state->blen + 1);
	if (state->bheur == NULL)
		return (REG_ESPACE);
	wcstombs(state->bheur, state->wheur, state->blen);
	state->bheur[state->blen] = '\0';

	DEBUG_PRINTF("strategy: %s", h->type == HEUR_LONGEST
	    ? "HEUR_LONGEST" : "HEUR_PREFIX");
	DEBUG_PRINTF("chosen pattern: %ls", state->wheur);

        return frec_proc_literal(h->heur, state->wheur, state->wlen,
	    state->bheur, state->blen, 0);
}

/*
 * Parses a regular expression and constructs a heuristic in heur_t and
 * returns REG_OK if successful or the corresponding error code if a
 * heuristic cannot be constructed.
 */
#define END_SEGMENT							\
if (errcode != REG_OK)							\
	goto err;

int
frec_proc_heur(heur_t *h, const wchar_t *regex, size_t len, int cflags)
{
	int errcode;
	parser_state state;

	errcode = init_state(&state, regex, len, cflags);
	if (errcode != REG_OK)
		goto err;

	DEBUG_PRINT("enter");

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
		for (state.procidx = 0; state.procidx < state.len; state.procidx++) {
			switch (state.regex[state.procidx]) {

			/*
			 * Bracketed expression ends the segment or the
			 * brackets are treated as normal if at least the opening
			 * bracket is escaped.
			 */
			case L'[':
				if (state.escaped) {
					store_char(&state);
					continue;
				} else {
					if (parse_brackets(&state) == REG_BADPAT)
						return REG_BADPAT;
					state.tlen = (state.tlen == -1) ? -1 : state.tlen + 1;
					errcode = end_segment(&state, false);
					END_SEGMENT;
				}
				continue;

			/*
			 * If a repetition marker, erases the repeting character
			 * and terminates the segment, otherwise treated as a normal
			 * character.
			 */
			case L'{':
				if (state.escaped && (state.procidx == 1)) {
					store_char(&state);
					continue;
				} else if (state.procidx == 0) {
					store_char(&state);
					continue;
				}

				if (state.escaped ^ state.ere) {
					dec_pos(&state);
					parse_unit(&state, L'{', L'}');
					errcode = end_segment(&state, true);
					END_SEGMENT;
				} else {
					store_char(&state);
					continue;
				}
				continue;

			/*
			 * Terminates the current segment when escaped,
			 * otherwise treated as a normal character.
			 */
			case L'(':
				if (state.escaped ^ state.ere) {
					parse_unit(&state, L'(', L')');
					errcode = end_segment(&state, true);
					END_SEGMENT;
				} else {
					store_char(&state);
					continue;
				}
				continue;

			/*
			 * Sets escaped flag.
			 * Escaped escape is treated as a normal character.
			 * (This is also the GNU behaviour.)
			 */
			case L'\\':
				if (state.escaped) {
					store_char(&state);
					continue;
				} else
					state.escaped = true;
				continue;

			/*
			 * BRE: If not the first character and not escaped, erases the
			 * last character and terminates the segment.
			 * Otherwise treated as a normal character.
			 * ERE: Skipped if first character (GNU), rest is like in BRE.
			 */
			case L'*':
				if (state.escaped || (!state.ere && (state.procidx == 0))) {
					store_char(&state);
					continue;
				} else {
					dec_pos(&state);
					errcode = end_segment(&state, true);
					END_SEGMENT;
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
				if (state.ere && (state.procidx == 0))
					continue;
				else if (state.ere ^ state.escaped) {
					errcode = end_segment(&state, true);
					END_SEGMENT;
				} else {
					store_char(&state);
					continue;
				}
				continue;

			/*
			 * In BRE, it is a normal character, behavior is undefined
			 * when escaped.
			 * In ERE, it is special unless escaped. Terminate segment
			 * when not escaped. Last character is removed. Skipped when
			 * first character (GNU).
			 */
			case L'?':
				if (state.ere && (state.procidx == 0))
					continue;
				if (state.ere ^ state.escaped) {
					dec_pos(&state);
					errcode = end_segment(&state, true);
					END_SEGMENT;
				} else {
					store_char(&state);
					continue;
				}
				continue;

			/*
			 * Fail if it is an ERE alternation marker.
			 */
			case L'|':
				if (state.ere && !state.escaped) {
					errcode = REG_BADPAT;
					goto err;
				} else if (!state.ere && state.escaped) {
					errcode = REG_BADPAT;
					goto err;
				} else {
					store_char(&state);
					continue;
				}
				continue;

			case L'.':
				if (state.escaped) {
					store_char(&state);
					continue;
				} else {
					state.has_dot = true;
					state.tlen = (state.tlen == -1) ? -1 : state.tlen + 1;
					errcode = end_segment(&state, false);
					END_SEGMENT;
				}
				continue;
			case L'\n':
				state.has_lf = true;
				store_char(&state);
				continue;

			/*
			 * If escaped, terminates segment.
			 * Otherwise adds current character to the current segment
			 * by copying it to the temporary space.
			 */
			default:
				if (state.escaped) {
					if (state.regex[state.procidx] == L'n') {
						state.has_lf = true;
						store(&state, L'\n');
						continue;
					}
					errcode = end_segment(&state, true);
					END_SEGMENT;
				} else {
					store_char(&state);
					continue;
				}
				continue;
			}
		}

	if (state.heurpos > 0) {
		errcode = end_segment(&state, false);
		END_SEGMENT;
	}

	h->heur = malloc(sizeof(fastmatch_t));
	if (!h->heur) {
		errcode = REG_ESPACE;
		goto err;
	}

	errcode = fill_heuristics(&state, h);
	if (errcode != REG_OK)
		goto err;

	errcode = REG_OK;

err:
	if ((errcode != REG_OK) && (h->heur != NULL))
		frec_free_fast(h->heur);

	free_state(&state);
	return (errcode);
}
