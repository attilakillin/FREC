#ifndef HEURISTIC_H
#define HEURISTIC_H 1

#include <stdbool.h>

#include "boyer-moore.h"

#define MAX_FRAGMENTS 32

#define HEUR_PREFIX		0
#define HEUR_LONGEST		1

typedef struct {
	bm_preproc_t *heur;
	ssize_t tlen;
	int type;

	bm_preproc_t *literal_prep;	/* BM prep struct for the longest literal fragment of the pattern. */
	long extra_len;				/* The maximum possible extra length of the pattern. */
	int heur_type;				/* The type of the heuristic. */
} heur_t;

int frec_proc_heur(
	heur_t *heur, const wchar_t *pattern, size_t len, int cflags);
extern void	frec_free_heur(heur_t *h);

#endif
