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

#include <sys/types.h>
#include <stdlib.h>
#include <string-type.h>
#include <wchar.h>

#include "bm.h"
#include "frec-internal.h"
#include "regex-parser.h"
#include "wm-comp.h"

// Compiles the bm_prep field of the frec struct based on the given pattern.
// Additional flags can be specified in the cflags field.
static int
compile_boyer_moore(frec_t *frec, string pattern, int cflags)
{
    // If the pattern is too short, literal matching is inefficient.
    if (pattern.len < 2) {
        frec->boyer_moore = NULL;
    }

    bm_comp *comp = malloc(sizeof(bm_comp));
    if (comp == NULL) {
        return (REG_ESPACE);
    }
    bm_comp_init(comp, cflags);
    
    // Based on the literal flag, choose apppropriate preprocessing.
    int ret = (cflags & REG_LITERAL)
        ? bm_compile_literal(comp, pattern, cflags)
        : bm_compile_full(comp, pattern, cflags);
    
    // If valid, set the relevant return field, else free the struct.
    if (ret == REG_OK) {
        frec->boyer_moore = comp;
    } else {
        frec->boyer_moore = NULL;
        bm_comp_free(comp);
        free(comp);
    }

    return ret;
}

// Compiles the heur field of the frec struct based on the given pattern.
// Additional flags can be specified in the cflags field.
static int
compile_heuristic(frec_t *frec, string pattern, int cflags)
{
    heur *heur = frec_create_heur();
    if (heur == NULL) {
        return (REG_ESPACE);
    }

    // Execute heuristic compilation.
    int ret = frec_preprocess_heur(heur, pattern, cflags);
    
    // If valid, set the relevant return field, else free the struct.
    if (ret == REG_OK) {
        frec->heuristic = heur;
    } else {
        frec->heuristic = NULL;
        frec_free_heur(heur);
    }

    return ret;
}

static bool
is_pattern_literal(string pattern, int in_flags)
{
    regex_parser parser;
	parser.escaped = false;
	parser.extended = in_flags & REG_EXTENDED;

    // Traverse the pattern:
	for (ssize_t i = 0; i < pattern.len; i++) {
        parse_result result;
        if (pattern.is_wide) {
            result = parse_wchar(&parser, pattern.wide[i]);
        } else {
            result = parse_char(&parser, pattern.stnd[i]);
        }

		switch (result) {
			case NORMAL_CHAR:
			case NORMAL_NEWLINE:
			case SHOULD_SKIP:
				break;
			/* If any special character was found, we know the pattern.
             * isn't literal. */            
			default:
				return false;
		}
	}

    return true;
}

int
frec_compile(frec_t *frec, string pattern, int cflags)
{
    // Compile NFA using our regex library. If we can't optimize, we
    // can still use this original struct, and this way, we validate
    // the pattern automatically.
    int ret = (pattern.is_wide)
        ? _dist_regwncomp(&frec->original, pattern.wide, pattern.len, cflags)
        : _dist_regncomp(&frec->original, pattern.stnd, pattern.len, cflags);
    if (ret != REG_OK) {
        return ret;
    }

    /* Check if pattern is literal. */
    bool is_literal = (cflags & REG_LITERAL) || is_pattern_literal(pattern, cflags);
    frec->is_literal = is_literal;

    // Try and compile BM prep struct. Modify the REG_LITERAL flag if needed.
    int flags = cflags;
    if (is_literal) {
        cflags |= REG_LITERAL;
    }
    ret = compile_boyer_moore(frec, pattern, cflags);

    // A heuristic approach is only needed if the pattern is not literal.
    if (ret != REG_OK && !(cflags & REG_LITERAL)) {
        compile_heuristic(frec, pattern, cflags);
    } else {
        frec->heuristic = NULL;
    }

    // We save the compilation flags. At this point, at least
    // the library-supplied NFA compilation was successful.
    frec->cflags = cflags;
    return (REG_OK);
}

int
frec_mcompile(mfrec_t *mfrec, const string *patterns, ssize_t n, int cflags) {
    mfrec->patterns = malloc(sizeof(frec_t) * n);
    if (mfrec->patterns == NULL) {
        return (REG_ESPACE);
    }

    mfrec->err = -1;
    mfrec->count = n;
    mfrec->cflags = cflags;

    bool are_literal = true;

    // Compile each pattern.
    for (ssize_t i = 0; i < n; i++) {
        int ret = frec_compile(&mfrec->patterns[i], patterns[i], cflags);
        // On error, we record the index of the bad pattern.
        if (ret != REG_OK) {
            mfrec->err = i;
            frec_mregfree(mfrec);
            return ret;
        }

        /* If any one of the patterns wasn't literal, set this flag to false */
        if (!mfrec->patterns[i].is_literal) {
            are_literal = false;
        }
    }

    mfrec->are_literal = are_literal;

    // If there's only one pattern, return early.
    if (n == 1) {
        mfrec->type = MHEUR_SINGLE;
        return (REG_OK);
    }

    // Set the heuristic type based on the compilation flags.
    if (cflags & REG_LITERAL || are_literal) {
        // If the REG_LITERAL flag is set, use literal heuristics.

        mfrec->type = MHEUR_LITERAL;
    } else {
        // Else we'll use longest heuristics with one exception.
        // If there's a pattern with an empty Boyer Moore and an empty
        // heuristic struct, we can't use any multi-pattern heuristics.

        for (ssize_t i = 0; i < n; i++) {
            frec_t *curr = &mfrec->patterns[i];
            bool no_heur = curr->boyer_moore == NULL && curr->heuristic == NULL;
            if (no_heur) {
                mfrec->type = MHEUR_NONE;
                return (REG_OK);
            }
        }

        mfrec->type = MHEUR_LONGEST;
    }

    // If we reach this point, we'll definitely need this struct.
    wm_comp *comp = malloc(sizeof(wm_comp));
    if (comp == NULL) {
        frec_mregfree(mfrec);
        return (REG_ESPACE);
    }
    mfrec->wu_manber = comp;

    // If the heuristic type was literal, we'll use the patterns as-is.
    if (mfrec->type == MHEUR_LITERAL) {
        int ret = wm_compile(comp, patterns, n, cflags);
        if (ret != REG_OK) {
            frec_mregfree(mfrec);
            return ret;
        }
    } else {
        // Otherwise, we'll assemble the patterns from the Boyer-Moore
        // or the heuristic compilation phase.
        string *pat_refs = malloc(sizeof(string) * n);
        if (pat_refs == NULL) {
            frec_mregfree(mfrec);
            return (REG_ESPACE);
        }

        // Reference values from previous optimalizations.
        for (size_t i = 0; i < n; i++) {
            frec_t *curr = &mfrec->patterns[i];
            if (curr->boyer_moore != NULL) {
                string_reference(&pat_refs[i], curr->boyer_moore->pattern);
            } else {
                string_reference(&pat_refs[i], curr->heuristic->literal_comp.pattern);
            }
        }

        // Execute compilation and free temporary arrays.
        int ret = wm_compile(comp, pat_refs, n, cflags);
        free(pat_refs);

        if (ret != REG_OK) {
            frec_mregfree(mfrec);
            return ret;
        }
    }

    return (REG_OK);
}
