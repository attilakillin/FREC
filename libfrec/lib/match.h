#ifndef MATCH_H
#define MATCH_H 1

#include "config.h"
#include "frec-internal.h"
#include "frec-match.h"

extern int	 frec_match(const frec_t *preg, const void *str,
		     size_t len, int type, size_t nmatch,
		     frec_match_t pmatch[], int eflags);
extern int	 frec_match_heur(regex_t *preg, heur_t *heur,
		     const void *str, size_t len, int type, size_t nmatch,
		     frec_match_t pmatch[], int eflags);
extern int	 frec_mmatch(const void *str, size_t len, int type,
		     size_t nmatch, frec_match_t pmatch[], int eflags,
		     const mfrec_t *preg);
#endif
