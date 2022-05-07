#include "wm-type.h"

void
wm_comp_init(wm_comp *comp, ssize_t count, int cflags)
{
    comp->count = count;
    comp->cflags = cflags;
}

void
wm_comp_free(wm_comp *comp)
{
    if (comp != NULL) {
        hashtable_free(comp->shift);
    }
}
