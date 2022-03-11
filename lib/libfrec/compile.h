#ifndef COMPILE_H
#define COMPILE_H 1

#include <sys/types.h>
#include <wchar.h>

#include "mregex.h"
#include "frec.h"

int
frec_compile(frec_t *frec, const wchar_t *pattern, size_t len, int cflags);

int
frec_mcompile(mregex_t *mfrec, size_t k,
	const wchar_t **patterns, size_t *lens, int cflags);

#endif
