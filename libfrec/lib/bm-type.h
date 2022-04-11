#ifndef FREC_BM_TYPE_H
#define FREC_BM_TYPE_H

#include <limits.h>
#include <stdbool.h>

#include "hashtable.h"
#include "string-type.h"

// Contains the complete compiled output of the Boyer-Moore compilation phase.
typedef struct bm_comp {
    string pattern;      // The processed literal pattern to use for matches.
    unsigned int *good_shifts;   // The Boyer-Moore good shifts table.

    unsigned int bad_shifts_stnd[UCHAR_MAX + 1]; // The Boyer-Moore bad shifts
    hashtable *bad_shifts_wide;                  // table, separate versions.

    bool has_bol_anchor; // The pattern starts with a ^ anchor.
    bool has_eol_anchor; // The pattern ends with a $ anchor.
    bool has_glob_match; // The pattern matches anything.

    bool is_icase_set; // Ignore text case when matching. Set by REG_ICASE.
    bool is_nosub_set; // Do not save result when matching. Set by REG_NOSUB.
    bool is_nline_set; // Handle newlines differently. Set by REG_NEWLINE.
} bm_comp;

// Initialize the given compilation struct. Must point to valid memory.
void
bm_comp_init(bm_comp *comp, int cflags);

// Free the given compilation struct. Does not free the struct itself.
void
bm_comp_free(bm_comp *comp);

#endif //FREC_BM_TYPE_H
