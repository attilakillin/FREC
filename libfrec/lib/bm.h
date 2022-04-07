#ifndef FREC_BM_H
#define FREC_BM_H

#include <limits.h>
#include <stdbool.h>
#include <wchar.h>

#include <frec-match.h>
#include "bm-type.h"
#include "hashtable.h"
#include "string-type.h"

int
bm_compile_literal(bm_comp *comp, string patt, int cflags);
int
bm_compile_full(bm_comp *comp, string patt, int cflags);

int
bm_execute(frec_match_t result[], size_t nmatch, bm_comp *comp, string text, int eflags);



#endif // FREC_BM_H
