#include <stdlib.h>

#include "bm-type.h"
#include "frec-internal.h"
#include "wm-type.h"

void
frec_regfree(frec_t *preg)
{
    bm_comp_free(preg->boyer_moore);
    free(preg->boyer_moore);
    frec_free_heur(preg->heuristic);
    _dist_regfree(&preg->original);
}

void
frec_mregfree(mfrec_t *preg)
{
    if (preg != NULL) {
        ssize_t until = preg->count;
        if (preg->err != -1) {
            until = preg->err;
        }

        for (ssize_t i = 0; i < until; i++) {
            frec_regfree(&preg->patterns[i]);
        }

        wm_comp_free(preg->wu_manber);
    }
}
