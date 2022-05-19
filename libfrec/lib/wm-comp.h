#ifndef FREC_WM_COMP_H
#define FREC_WM_COMP_H

#include <frec-match.h>
#include "wm-type.h"

#define MHEUR_NONE 0
#define MHEUR_SINGLE 1
#define MHEUR_LITERAL 2
#define MHEUR_LONGEST 3

// Implements the Wu-Manber algorithm for multiple pattern matching.
// Even if it is not the best performing algorithm for a low number
// of patterns, it still scales well, and it is very simple compared
// to other automaton-based multi-pattern algorithms.
//
// Given a pattern array with count elements, and global compilation flags,
// fills the given comp compilation struct.
int
wm_compile(wm_comp *comp, const string *patterns, ssize_t count, int cflags);

int
wm_execute(frec_match_t *result, const wm_comp *comp, string text, int eflags);

#endif //FREC_WM_COMP_H
