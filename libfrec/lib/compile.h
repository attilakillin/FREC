#ifndef FREC_COMPILE_H
#define FREC_COMPILE_H

#include <frec-types.h>
#include "string-type.h"

// Given a frec_t struct and a pattern, compile an NFA struct
// (supplied by the underlying library), a Boyer-Moore fast
// text searching struct, and a custom heuristic struct.
// Additional flags may be supplied using the cflags parameter.
//
// Given a newly allocated frec_t struct, this method fills all
// its compilation-related fields.
int
frec_compile(frec_t *frec, string pattern, int cflags);

// Given an mfrec_t struct and n patterns, compile an NFA struct
// (supplied by the underlying library), a Boyer-Moore fast text
// searching struct, and a custom heuristic struct for each pattern.
// Additional flags may be supplied using the cflags parameter.
//
// Given a newly allocated mfrec_t struct, this method fills all
// its compilation-related fields.
int
frec_mcompile(mfrec_t *mfrec, const string *patterns, ssize_t n, int cflags);

#endif // FREC_COMPILE_H
