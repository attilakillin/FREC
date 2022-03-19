#ifndef FREC_H
#define FREC_H 1

#include <sys/param.h>
#include <stdio.h>

#include "boyer-moore.h"
#include "config.h"
#include "frec-match.h"
#include "heuristic.h"

#define STR_BYTE	0
#define STR_MBS		1
#define STR_WIDE	2

#ifdef WITH_DEBUG
	#define DEBUG_PRINTF(fmt, ...) fprintf(stderr, "%s: " fmt "\n", __func__, __VA_ARGS__);
	#define DEBUG_PRINT(str) fprintf(stderr, "%s: %s\n", __func__, str);
#else
	#define DEBUG_PRINTF(fmt, ...) do {} while (0)
	#define DEBUG_PRINT(str)  do {} while (0)
#endif

// TODO Document this struct
typedef struct {
	regex_t orig;
	bm_preproc_t *shortcut;
	bm_preproc_t *bm_prep;
	heur_t *heur;
	const char *re_endp;
	const wchar_t *re_wendp;
	int cflags;
} frec_t;

#endif /* FREC_H */
