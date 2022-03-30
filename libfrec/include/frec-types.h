#ifndef FREC_TYPES_H
#define FREC_TYPES_H 1

#include <tre/regex.h>

typedef struct bm_preproc_t bm_preproc_t;
typedef struct heur_t heur_t;

typedef struct frec_t {
    regex_t original;           /* Compiled automaton data used by TRE. */
    bm_preproc_t *boyer_moore;  /* Compiled Boyer-Moore search data. */
    heur_t *heuristic;          /* Compiled FREC heuristic data. */
    int cflags;                 /* Input compilation flags. */

    const char *re_endp;        /* Optionally marks the end of the pattern. */
	const wchar_t *re_wendp;    /* Optionally marks the end of the pattern. */
} frec_t;

typedef struct mfrec_t {
	size_t k;		/* Number of patterns */
	frec_t *patterns;	/* regex_t structure for each pattern */
	int type;		/* XXX (private) Matching type */
	int err;		/* XXX (private) Which pattern failed */
	int cflags;		/* XXX (private) cflags */
	void *searchdata;	/* Wu-Manber internal data */
} mfrec_t;

#endif