#ifndef COMPILE_H
#define COMPILE_H 1

#include <wchar.h>

#include "frec-internal.h"

int frec_compile(frec_t *frec, const wchar_t *pattern, size_t len, int cflags);
int frec_mcompile(mfrec_t *mfrec, size_t k, const wchar_t **patterns, size_t *lens, int cflags);

#endif
