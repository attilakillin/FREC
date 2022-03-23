
#include "config.h"
#include "match.h"
#include "type-unification.h"

/* Utility functions. */
static int max(long a, long b) { return (a > b) ? a : b; }
static int min(long a, long b) { return (a < b) ? a : b; }

/*
 * Use the original library-supplied matcher on the given text. 
 */
static int
match_original(
    frec_match_t pmatch[], size_t nmatch,
    const regex_t *preg, const str_t *text, int eflags
) {
    /* Allocate temporary storage for the matches. */
    regmatch_t *matches = malloc(sizeof(regmatch_t) * nmatch);
    if (matches == NULL) {
        return (REG_ESPACE);
    }

    /* Call the correct library function. */
    int ret = (text->is_wide)
        ? _dist_regnexec(preg, text->stnd, text->len, nmatch, matches, eflags)
        : _dist_regwnexec(preg, text->wide, text->len, nmatch, matches, eflags);

    /* Copy results into own frec_match_t array. */
    if (ret == REG_OK) {
        for (size_t i = 0; i < nmatch; i++) {
            pmatch[i].soffset = matches[i].rm_so;
            pmatch[i].eoffset = matches[i].rm_eo;
        }
    }

    free(matches);
    return ret;
}

/*
 * Use compiled heuristics to find matches.
 */
static int
match_heuristic(
    frec_match_t pmatch[], size_t nmatch, const heur_t *heur,
    const regex_t *orig, const str_t *in_text, int eflags
) {
    int ret;
    str_t text;
    string_copy(&text, in_text);

    if (heur->heur_type == HEUR_LONGEST) {
        /* This heuristic type means that we either have a maximum possible
         * match size, or if we don't, no line feed can occur in a match. */

        /* Allocate space for at least one candidate match. */
        size_t ncand = max(1, nmatch);
        frec_match_t *cands = malloc(sizeof(frec_match_t) * ncand);
        if (cands == NULL) {
            return (REG_ESPACE);
        }

        size_t next_offset = 0; /* Offset from the current start of text. */
        size_t glob_offset = 0; /* Global offset from the start of in_text. */
        size_t match_idx = 0;   /* Current write index of pmatch. */

        /* While we have text to read, offset the text with next_offset. */
        while (text.len > 0) {
            string_offset_by(&text, next_offset);
            glob_offset += next_offset;

            /* Find candidate matches (at least one). */
            ret = bm_execute(cands, ncand, heur->literal_prep, &text, eflags);

            /* If no candidates were found in the text, break out. */
            if (ret != REG_OK) {
                break;
            }

            size_t count = count_matches(cands, ncand);

            /* The next offset value will be the end of the last candidate. */
            next_offset = cands[count - 1].eoffset;

            /* For each candidate match: */
            for (size_t i = 0; i < count; i++) {
                size_t start = cands[i].soffset;
                size_t end = cands[i].eoffset;

                if (heur->max_length != -1) {
                    /* If we know the max length of a match, set start
                     * and end to have exactly that much wiggle room. */

                    long delta = heur->max_length - (end - start);

                    start = max(0, start - delta);
                    end = min(text.len, end + delta);
                } else {
                    /* If we don't know its max length, we know that a
                     * match never overlaps multiple lines. As such, we
                     * set start and end to the nearest line breaks. */

                    start = find_lf_backward(&text, start);
                    end = find_lf_forward(&text, end);
                }

                /* If we already found matches, we need to set start and end
                 * to at least the end of the previous match (otherwise, two
                 * matches on the same line may have the same start and end
                 * values). */
                if (match_idx > 0) {
                    size_t prev_end = pmatch[match_idx - 1].eoffset;

                    start = max(start, prev_end - glob_offset);
                    end = max(end, start);
                }

                /* Create a text excerpt from this section, and call the
                 * library supplied matcher for at most one match. */
                str_t section;
                string_excerpt(&section, &text, start, end);

                ret = match_original(pmatch + match_idx, min(nmatch, 1),
                    orig, &section, eflags);
                
                /* If no match was found, continue with the next candidate. */
                if (ret != REG_OK) {
                    continue;
                }

                /* If nmatch is not 0, fix the offsets of the new match. */
                if (nmatch != 0) {
                    pmatch[match_idx].soffset += glob_offset;
                    pmatch[match_idx].eoffset += glob_offset;
                }

                /* If we found enough matches, return. */
                match_idx++;
                if (match_idx >= nmatch) {
                    free(cands);
                    return REG_OK;
                }

                /* Continue with the next candidate. */
            }

            /* After we looked through every candidate:
             * If we are here, the at most nmatch candidates we found were not
             * all proper matches. */
        }
        
        /* If we are here, we reached the end of the text, and didn't find
         * nmatch different matches. If we found none, return NOMATCH, else
         * mark the boundary match, and return OK. */

        free(cands);

        if (match_idx == 0) {
            return (REG_NOMATCH);
        } else {
            pmatch[match_idx].soffset = -1;
            pmatch[match_idx].eoffset = -1;
            return (REG_OK);
        }
    } else {
        /* This heuristic type means that we don't know the maximum match size,
         * nor can we separate the input text by line feeds. As such, we search
         * for the first possible candidate, and call the original matcher from
         * the start of this candidate to the end of the original text. */

        frec_match_t first;
        ret = bm_execute(&first, 1, heur->literal_prep, &text, eflags);

        /* If not even a candidate was found, we'll return early. */
        if (ret != REG_OK) {
            return ret;
        }

        /* Run the original matcher on this subtext. */
        string_offset_by(&text, first.soffset);
        ret = match_original(pmatch, nmatch, orig, &text, eflags);

        /* Fix offsets that we messed up above, and return. */
        if (nmatch > 0 && ret == REG_OK) {
            for (size_t i = 0; i < nmatch && pmatch[i].soffset != -1; i++) {
                pmatch[i].soffset += first.soffset;
                pmatch[i].eoffset += first.soffset;
            }
        }
        return ret;
    }
}

int
frec_match(
    frec_match_t pmatch[], size_t nmatch,
    const frec_t *preg, const str_t *text, int eflags
) {
    const regex_t *orig = &preg->original;
    bm_preproc_t *bm = preg->boyer_moore;
    heur_t *heur = preg->heuristic;

    if (bm != NULL) {
        return bm_execute(pmatch, nmatch, bm, text, eflags);
    } else if (heur != NULL) {
        return match_heuristic(pmatch, nmatch, heur, orig, text, eflags);
    } else {
        return match_original(pmatch, nmatch, orig, text, eflags);
    }
}
