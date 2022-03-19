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
#include <string.h>
#include <wchar.h>

#include "boyer-moore.h"
#include "frec2.h"
#include "mregex.h"
#include "wu-manber.h"

/*
 * Compiles the bm_prep field of the frec struct based on the
 * given pattern with the given length.
 * Additional flags can be specified in the cflags field.
 */
static int
compile_boyer_moore(
    frec_t *frec, const wchar_t *pattern, size_t len, int cflags)
{
    /* If the pattern is too short, literal matching is inefficient. */
    if (len < 2) {
        frec->boyer_moore = NULL;
    }

    bm_preproc_t *prep = bm_create_preproc();
    if (prep == NULL) {
        return (REG_ESPACE);
    }
    /* Based on the literal flag, choose apppropriate preprocessing. */
    int ret = (cflags & REG_LITERAL)
        ? bm_preprocess_literal(prep, pattern, len, cflags)
        : bm_preprocess_full(prep, pattern, len, cflags);
    
    /* If valid, set the relevant return field, else free the struct. */
    if (ret == REG_OK) {
        frec->boyer_moore = prep;
    } else {
        frec->boyer_moore = NULL;
        bm_free_preproc(prep);
    }

    return ret;
}

/*
 * Compiles the heur field of the frec struct based on the
 * given pattern with the given length.
 * Additional flags can be specified in the cflags field.
 */
static int
compile_heuristic(
    frec_t *frec, const wchar_t *pattern, size_t len, int cflags)
{
    heur_t *heur = malloc(sizeof(heur_t));
    if (heur == NULL) {
        return (REG_ESPACE);
    }

    /* Execute heuristic compilation. */
    int ret = frec_preprocess_heur(heur, pattern, len, cflags);
    
    /* If valid, set the relevant return field, else free the struct. */
    if (ret == REG_OK) {
        frec->heuristic = heur;
    } else {
        frec->heuristic = NULL;
        frec_free_heur(heur);
        free(heur);
    }

    return ret;
}

/*
 * Given a frec_t struct and a pattern with a given length, compile a
 * usual NFA struct (supplied by the underlying library), a Boyer-Moore
 * fast text searching struct, and a custom heuristic struct.
 * Additional flags may be supplied using the cflags parameter.
 * 
 * Given a newly allocated frec_t struct, this method fills all
 * its compilation-related fields.
 */
int
frec_compile(frec_t *frec, const wchar_t *pattern, size_t len, int cflags)
{
    /* Compile NFA using our regex library. If we can't optimize, we
     * can still use this original struct, and this way, we validate
     * the pattern automatically. */
    int ret = _dist_regwncomp(&frec->original, pattern, len, cflags);
    if (ret != REG_OK) {
        return ret;
    }

    /* Try and compile BM prep struct irrespective of the REG_LITERAL flag. */
    ret = compile_boyer_moore(frec, pattern, len, cflags);

    /* A heuristic approach is only needed if the pattern is not literal. */
    if (ret != REG_OK && !(cflags & REG_LITERAL)) {
        compile_heuristic(frec, pattern, len, cflags);
    } else {
        frec->heuristic = NULL;
    }

    /* We save the compilation flags. At this point, at least
     * the library-supplied NFA compilation was successful. */
    frec->cflags = cflags;
    return (REG_OK);
}

/*
 * Given an mregex_t struct and k patterns with given lengths, compile a
 * usual NFA struct (supplied by the underlying library), a Boyer-Moore
 * fast text searching struct, and a custom heuristic struct for each pattern.
 * Additional flags may be supplied using the cflags parameter.
 *
 * Given a newly allocated mfrec_t struct, this method fills all
 * its compilation-related fields.
 */
int
frec_mcompile(mregex_t *mfrec, size_t k,
    const wchar_t **patterns, size_t *lens, int cflags)
{
    mfrec->patterns = malloc(sizeof(frec_t) * k);
    if (mfrec->patterns == NULL) {
        return (REG_ESPACE);
    }
    mfrec->k = k;
    mfrec->cflags = cflags;
    mfrec->searchdata = NULL; // TODO Bad name :(

    /* Compile each pattern. */
    for (size_t i = 0; i < k; i++) {
        int ret = frec_compile(
            &mfrec->patterns[i], patterns[i], lens[i], cflags);
        /* On error, we record the index of the bad pattern. */
        if (ret != REG_OK) {
            mfrec->err = i;
            // TODO FREE mfrec
        }
    }

    /* If there's only one pattern, return early. */
    if (k == 1) {
        mfrec->type = MHEUR_SINGLE;
        return (REG_OK);
    }

    /* Set the heuristic type based on the compilation flags. */
    if (cflags & REG_LITERAL) {
        mfrec->type = MHEUR_LITERAL;
    } else if (cflags & REG_NEWLINE) {
        // TODO I Don't yet understand this case.
        mfrec->type = MHEUR_LONGEST;
    } else {
        /* If not explicitly marked, check if all patterns can be
         * matched either literally, or with the HEUR_LONGEST heuristic.
         * If not, we can't speed up the multiple pattern matcher. */
        for (size_t i = 0; i < k; i++) {
            frec_t *curr = &mfrec->patterns[i];
            if (curr->boyer_moore == NULL && 
                (curr->heuristic == NULL || curr->heuristic->heur_type != HEUR_LONGEST)) {
                    mfrec->type = MHEUR_NONE;
                    return (REG_OK);
                }
        }
        mfrec->type = MHEUR_LONGEST;
    }

    /* If we reach this point, we'll definitely need this struct. */
    wmsearch_t *wumanber = malloc(sizeof(wmsearch_t));
    if (wumanber == NULL) {
        // TODO Free mfrec
    }
    wumanber->cflags = cflags;

    /* If the heuristic type was literal, we'll use the patterns as-is. */
    if (mfrec->type = MHEUR_LITERAL) {
        int ret = frec_wmcomp(wumanber, k, patterns, lens, cflags);
        if (ret != REG_OK) {
            // TODO Free mfrec, wumanber
            return ret;
        }
    } else {
        /* Otherwise we'll assemble the patterns from the Boyer-Moore
         * or the heuristic compilation phase. */
        const wchar_t **pat_ptrs = malloc(sizeof(wchar_t *) * k);
        if (pat_ptrs == NULL) {
            // TODO Free mfrec, wumanber
            return (REG_ESPACE);
        }
        size_t *len_ptrs = malloc(sizeof(size_t) * k);
        if (len_ptrs == NULL) {
            // TODO free mfrec, wumanber, pat_ptrs
            return (REG_ESPACE);
        }

        /* Reference values from previous optimalizations. */
        for (size_t i = 0; i < k; i++) {
            frec_t *curr = &mfrec->patterns[i];
            if (curr->boyer_moore != NULL) {
                pat_ptrs[i] = curr->boyer_moore->wide.pattern;
                len_ptrs[i] = curr->boyer_moore->wide.len;
            } else {
                pat_ptrs[i] = curr->heuristic->literal_prep->wide.pattern;
                len_ptrs[i] = curr->heuristic->literal_prep->wide.len;
            }
        }

        /* Execute compilation and free temporary arrays. */
        int ret = frec_wmcomp(wumanber, k, pat_ptrs, len_ptrs, cflags);
        free(pat_ptrs);
        free(len_ptrs);

        if (ret != REG_OK) {
            // TODO Free mfrec, wumanber
            return ret;
        }
    }

    mfrec->searchdata = wumanber; // TODO I don't like this name
    
    return (REG_OK);
}
