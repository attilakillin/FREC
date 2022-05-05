
#include <frec-config.h>
#include <frec-match.h>

#include "heuristic.h"
#include "match.h"
#include "string-type.h"

/* Utility functions. */
static ssize_t max(ssize_t a, ssize_t b) { return (a > b) ? a : b; }
static ssize_t min(ssize_t a, ssize_t b) { return (a < b) ? a : b; }

// Use the original library-supplied matcher on the given text.
static int
match_original(
    frec_match_t result[], size_t nmatch,
    const regex_t *orig, string text, int eflags
) {
    // Allocate temporary storage for the pmatch.
    regmatch_t *pmatch = malloc(sizeof(regmatch_t) * nmatch);
    if (pmatch == NULL) {
        return (REG_ESPACE);
    }

    // Call the correct library function.
    int ret = (text.is_wide)
        ? _dist_regwnexec(orig, text.wide, text.len, nmatch, pmatch, eflags)
        : _dist_regnexec(orig, text.stnd, text.len, nmatch, pmatch, eflags);

    // Copy results into own frec_match_t array.
    if (ret == REG_OK) {
        for (size_t i = 0; i < nmatch; i++) {
            result[i].soffset = pmatch[i].rm_so;
            result[i].eoffset = pmatch[i].rm_eo;
        }
    }

    free(pmatch);
    return ret;
}

// Use compiled heuristics to find matches.
static int
match_heuristic(
        frec_match_t result[], size_t nmatch,
        const heur *heur, const regex_t *orig, string text, int eflags
) {
    int ret;

    if (heur->heur_type == HEUR_LONGEST) {
        // This heuristic type means that we either have a maximum possible
        // match size, or if we don't, no line feed can occur in a match.

        frec_match_t candidate; // We'll store our potential candidate here.
        ssize_t glob_offset = 0; // Global offset from the start of input.

        // While we have text to read.
        while (text.len > 0) {

            // Find candidate match.
            ret = bm_execute(&candidate, &heur->literal_comp, text, eflags);

            // If no candidates were found, return as such.
            if (ret != REG_OK) {
                return (REG_NOMATCH);
            }

            ssize_t start = candidate.soffset;
            ssize_t end = candidate.eoffset;

            if (heur->max_length != -1) {
                // If we know the max length of a match, set start
                // and end to have exactly that much wiggle room.
                ssize_t delta = heur->max_length - (end - start);

                start = max(0, start - delta);
                end = min(text.len, end + delta);
            } else {
                // If we don't know its max length, we know that a
                // match never overlaps multiple lines. As such, we
                // set start and end to the nearest line breaks.

                start = find_lf_backward(text, start);
                end = find_lf_forward(text, end);
            }

            // Create a text excerpt from this section, and call the
            // library supplied matcher for at most one match.
            string section;
            string_borrow_section(&section, text, start, end);

            ret = match_original(result, nmatch, orig, section, eflags);

            // If we found a match, break out of the while loop.
            // The match was found relative to glob_offset + start.
            if (ret == REG_OK) {
                glob_offset += start;
                break;
            }

            // Else modify the offsets and continue with the rest of the text.
            // No match was found before end, so we can shift the offset by it.
            string_offset(&text, end);
            glob_offset += end;
        }

        // If we found a match, we'll fix the offsets in all its submatches.
        if (ret == REG_OK) {
            for (size_t i = 0; i < nmatch && result[i].soffset != -1; i++) {
                result[i].soffset += glob_offset;
                result[i].eoffset += glob_offset;
            }
        }

        return ret;
    } else {
        // This heuristic type means that we don't know the maximum match size,
        // nor can we separate the input text by line feeds. As such, we search
        // for the first possible candidate, and call the original matcher from
        // the start of this candidate to the end of the original text.

        frec_match_t candidate;
        ret = bm_execute(&candidate, &heur->literal_comp, text, eflags);

        // If not even a candidate was found, we'll return early.
        if (ret != REG_OK) {
            return ret;
        }

        // Run the original matcher on this subtext.
        string_offset(&text, candidate.soffset);
        ret = match_original(result, nmatch, orig, text, eflags);

        // Fix offsets that we messed up above, and return.
        if (nmatch > 0 && ret == REG_OK) {
            for (size_t i = 0; i < nmatch && result[i].soffset != -1; i++) {
                result[i].soffset += candidate.soffset;
                result[i].eoffset += candidate.soffset;
            }
        }
        return ret;
    }
}

int
frec_match(
    frec_match_t pmatch[], size_t nmatch,
    const frec_t *preg, string text, int eflags
) {
    const regex_t *orig = &preg->original;
    bm_comp *bm = preg->boyer_moore;
    heur *hr = preg->heuristic;

    if (bm != NULL) {
        frec_match_t *result = (nmatch == 0) ? NULL : &pmatch[0];
        return bm_execute(result, bm, text, eflags);
    } else if (hr != NULL) {
        return match_heuristic(pmatch, nmatch, hr, orig, text, eflags);
    } else {
        return match_original(pmatch, nmatch, orig, text, eflags);
    }
}
