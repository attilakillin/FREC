#ifndef LIBFREC_MATCH_H
#define LIBFREC_MATCH_H

#include <stdlib.h>

/* The main struct used for representing regular expression matches. */
typedef struct frec_match_t {
	ssize_t soffset; /* Start offset from the beginning of the text. */
	ssize_t eoffset; /* End offset from the beginning of the text. */
	size_t pattern_id; /* The index of the pattern that was matched. */
} frec_match_t;

#endif
