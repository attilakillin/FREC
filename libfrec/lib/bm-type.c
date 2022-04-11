#include <frec-config.h>
#include <malloc.h>
#include "bm-type.h"

void
bm_comp_init(bm_comp *comp, int cflags)
{
    string_init(&comp->pattern);
    comp->good_shifts = NULL;

    comp->bad_shifts_wide = NULL;

    comp->has_bol_anchor = false;
    comp->has_eol_anchor = false;
    comp->has_glob_match = false;

    comp->is_icase_set = cflags & REG_ICASE;
    comp->is_nosub_set = cflags & REG_NOSUB;
    comp->is_nline_set = cflags & REG_NEWLINE;
}

void
bm_comp_free(bm_comp *comp)
{
    if (comp != NULL) {
        string_free(&comp->pattern);
        free(comp->good_shifts);
        hashtable_free(comp->bad_shifts_wide);
    }
}
