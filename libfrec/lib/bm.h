#ifndef FREC_BM_H
#define FREC_BM_H

#include <frec-match.h>
#include "bm-type.h"
#include "string-type.h"

// Execute the Boyer-Moore compilation on the given patt pattern as if it was
// a literal string. Use the provided cflags. Returns REG_OK if compilation
// succeeded and REG_ESPACE on memory errors.
int
bm_compile_literal(bm_comp *comp, string patt, int cflags);

// Execute the Boyer-Moore compilation on the given patt pattern as if it was
// a full regex string. Use the provided cflags. Returns REG_OK if compilation
// succeeded, REG_BADPAT on a bad pattern and REG_ESPACE on memory errors.
int
bm_compile_full(bm_comp *comp, string patt, int cflags);

// Given a comp precompiled Boyer-Moore struct, a text, and given execution
// flags, find the first match and store its start and end in result.
// A result pointer of NULL, or an is_nosub_set in comp means the result will
// not be substituted into result.
int
bm_execute(frec_match_t *result, const bm_comp *comp, string text, int eflags);

#endif // FREC_BM_H
