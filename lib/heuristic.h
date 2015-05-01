#ifndef HEURISTIC_H
#define HEURISTIC_H 1

#include <stdbool.h>

#include "boyer-moore.h"

#define MAX_FRAGMENTS 32

#define HEUR_PREFIX		0
#define HEUR_LONGEST		1

typedef struct {
	char **arr;
	size_t *siz;

	wchar_t **warr;
	size_t *wsiz;

	fastmatch_t **heurs;
	ssize_t tlen;
	int type;
} heur_t;

extern int	frec_proc_heur(heur_t *h, const wchar_t *regex,
		    size_t len, int cflags);
extern void	frec_free_heur(heur_t *h);

#endif
