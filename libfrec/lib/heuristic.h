#ifndef HEURISTIC_H
#define HEURISTIC_H 1

#include <stdbool.h>
#include "bm.h"
#include "string-type.h"

#define MAX_FRAGMENTS 32

#define HEUR_PREFIX 0
#define HEUR_LONGEST 1

typedef struct heur {
	bm_comp literal_comp;	/* BM prep struct for the longest literal fragment of the pattern. */
	ssize_t max_length;			/* The maximum possible length of the pattern. -1 if not bound. */
	int heur_type;				/* The type of the heuristic. */
} heur;

heur *frec_create_heur();
void frec_free_heur(heur *h);

int
frec_preprocess_heur(heur *heur, string pattern, int cflags);

#endif
