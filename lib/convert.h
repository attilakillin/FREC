#ifndef CONVERT_H
#define CONVERT_H 1

#include "frec.h"

extern int	 frec_convert_pattern_to_mbs(const wchar_t *wregex,
		  size_t n, char **s, size_t *sn);
extern int	 frec_convert_pattern_to_wcs(const char *regex,
		     size_t n, wchar_t **w, size_t *wn);
#endif
