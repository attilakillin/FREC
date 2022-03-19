#ifndef FREC_MATCH_H
#define FREC_MATCH_H

#include <stdlib.h>

/* The main struct used for representing regular expression matches. */
typedef struct {
	size_t soffset; /* Start offset from the beginning of the text. */
	size_t eoffset; /* End offset from the beginning of the text. */
	size_t p; // TODO Clarify this
} frec_match_t;

#endif
