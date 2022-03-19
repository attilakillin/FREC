#ifndef FREC_INTERNAL_H
#define FREC_INTERNAL_H

#include <tre/regex.h>
#include <frec.h>
#include <wchar.h>

#include "boyer-moore.h"
#include "heuristic.h"

typedef struct frec_t {
    regex_t original;
    bm_preproc_t *boyer_moore;
    heur_t *heuristic;
    int cflags;

    const char *re_endp;
	const wchar_t *re_wendp;
} frec_t;

typedef struct mfrec_t {
	size_t k;		/* Number of patterns */
	frec_t *patterns;	/* regex_t structure for each pattern */
	int type;		/* XXX (private) Matching type */
	int err;		/* XXX (private) Which pattern failed */
	int cflags;		/* XXX (private) cflags */
	void *searchdata;	/* Wu-Manber internal data */
} mfrec_t;

// TODO I really wish to remove these
#define STR_BYTE	0
#define STR_MBS		1
#define STR_WIDE	2

// TODO and these
#ifdef WITH_DEBUG
	#define DEBUG_PRINTF(fmt, ...) fprintf(stderr, "%s: " fmt "\n", __func__, __VA_ARGS__);
	#define DEBUG_PRINT(str) fprintf(stderr, "%s: %s\n", __func__, str);
#else
	#define DEBUG_PRINTF(fmt, ...) do {} while (0)
	#define DEBUG_PRINT(str)  do {} while (0)
#endif

#endif
