#include <malloc.h>
#include "wm-type.h"

bool
wm_comp_init(wm_comp *comp, ssize_t count, int cflags)
{
    comp->count = count;
    comp->cflags = cflags;

    comp->patterns = malloc(sizeof(string) * count);
    if (comp->patterns == NULL) {
        return false;
    }

    return true;
}

void
wm_comp_free(wm_comp *comp)
{
    if (comp != NULL) {
        hashtable_free(comp->shift);

        for (int i = 0; i < comp->count; i++) {
            string_free(&comp->patterns[i]);
        }
        free(comp->patterns);
    }
}
