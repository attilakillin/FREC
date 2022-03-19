#ifndef LIBFREC_H
#define LIBFREC_H

#include <tre/regex.h>
#include <wchar.h>

struct bm_preproc_t;
struct heur_t;

typedef struct frec_t {
    regex_t original;
    struct bm_preproc_t *boyer_moore;
    struct heur_t *heuristic;
    int cflags;

    const char *re_endp;
	const wchar_t *re_wendp;
} frec_t;

int frec_regcomp(frec_t *prep, const char* pattern, int cflags);
int frec_regncomp(frec_t *prep, const char* pattern, size_t len, int cflags);
int frec_regwcomp(frec_t *prep, const wchar_t* pattern, int cflags);
int frec_regwncomp(frec_t *prep, const wchar_t* pattern, size_t len, int cflags);

#endif
