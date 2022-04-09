#ifndef FREC_COMPILE_H
#define FREC_COMPILE_H

#include <wchar.h>
#include "frec-internal.h"
#include "string-type.h"

// Given a frec_t struct and a pattern with a given length, compile a
// usual NFA struct (supplied by the underlying library), a Boyer-Moore
// fast text searching struct, and a custom heuristic struct.
// Additional flags may be supplied using the cflags parameter.
//
// Given a newly allocated frec_t struct, this method fills all
// its compilation-related fields.
int frec_compile(frec_t *frec, string pattern, int cflags);


int frec_mcompile(mfrec_t *mfrec, size_t k, const string *patterns, int cflags);

#endif
