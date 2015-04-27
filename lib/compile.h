#ifndef COMPILE_H
#define COMPILE_H 1

#include <sys/types.h>
#include <wchar.h>

#include "mregex.h"
#include "frec.h"

extern int	 frec_compile(frec_t *preg, const wchar_t *wregex,
		     size_t wn,	const char *regex, size_t n, int cflags);
extern int	 frec_compile_bm(frec_t *preg, const wchar_t *wregex,
		     size_t wn, const char *regex, size_t n, int cflags);
extern int	 frec_compile_heur(frec_t *preg, const wchar_t *regex,
		     size_t n, int cflags);
extern int	 frec_mcompile(mregex_t *preg, size_t nr,
		     const wchar_t **wregex, size_t *wn, const char **regex,
		     size_t *n, int cflags);
#endif
