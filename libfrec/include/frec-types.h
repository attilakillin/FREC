#ifndef LIBFREC_TYPES_H
#define LIBFREC_TYPES_H 1

#include <tre/regex.h>
#include <stdbool.h>

typedef struct bm_comp bm_comp;
typedef struct heur heur;
typedef struct wm_comp wm_comp;

typedef struct frec_t {
    regex_t original;           /* Compiled automaton data used by TRE. */
    bm_comp *boyer_moore;       /* Compiled Boyer-Moore search data. */
    heur *heuristic;            /* Compiled FREC heuristic data. */
    int cflags;                 /* Input compilation flags. */
    bool is_literal;            /* Whether or not the pattern is literal. */

    const char *re_endp;        /* Optionally marks the end of the pattern. */
	const wchar_t *re_wendp;    /* Optionally marks the end of the pattern. */
} frec_t;

typedef struct mfrec_t {
    wm_comp *wu_manber;	/* Wu-Manber common compiled structure. */
	frec_t *patterns;	/* Separate compiled structure for each pattern. */
	ssize_t count;	    /* Number of patterns. */
    int cflags;		    /* Input compilation flags. */
    bool are_literal;   /* Whether or not all patterns are literal. */

	int type;		    /* XXX (private) Matching type */
	ssize_t err;		/* XXX (private) Which pattern failed */
} mfrec_t;

#endif
