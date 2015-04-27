#ifndef BOYER_MOORE_H
#define BOYER_MOORE_H 1

#include <limits.h>
#include <stdbool.h>
#include <wchar.h>

#include "config.h"
#include "hashtable.h"
#include "frec.h"

typedef struct {
	regmatch_t m;
	size_t p;
} frec_match_t;

typedef struct {
  size_t wlen;
  size_t len;
  wchar_t *wpattern;
  bool *wescmap;
  unsigned int qsBc[UCHAR_MAX + 1];
  unsigned int *bmGs;
  char *pattern;
  bool *escmap;
  unsigned int defBc;
  void *qsBc_table;
  unsigned int *sbmGs;
  const char *re_endp;

  /* flags */
  bool hasdot;
  bool bol;
  bool eol;
  bool word;
  bool icase;
  bool newline;
  bool nosub;
  bool matchall;
  bool reversed;
} fastmatch_t;

int	frec_proc_literal(fastmatch_t *, const wchar_t *, size_t,
	    const char *, size_t, int);
int	frec_proc_fast(fastmatch_t *, const wchar_t *, size_t,
	    const char *, size_t, int);
int	frec_match_fast(const fastmatch_t *fg, const void *data,
	    size_t len, int type, int nmatch, frec_match_t pmatch[],
	    int eflags);
void	frec_free_fast(fastmatch_t *preg);

#endif
