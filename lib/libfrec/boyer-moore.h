#ifndef BOYER_MOORE_H
#define BOYER_MOORE_H 1

#include <limits.h>
#include <stdbool.h>
#include <wchar.h>

#include "config.h"
#include "hashtable.h"

typedef struct {
	regmatch_t m;
	size_t p; // Seems to be the match index
} frec_match_t; // No place here?

/* 
 * Contains data compiled during the Boyer-Moore preprocessing phase.
 * Can be used with standard character arrays.
 */
typedef struct {
	size_t len; /* The length of the stored pattern. */
	char *pattern; /* Pointer to the stored pattern. */
	unsigned int badc_shifts[UCHAR_MAX + 1]; /* Bad character shift table. */
	unsigned int *goods_shifts; /* Good suffix shift table. */
} bm_preproc_ct;

/* 
 * Contains data compiled during the Boyer-Moore preprocessing phase.
 * Can be used with wide character arrays.
 */
typedef struct {
	size_t len; /* The length of the stored pattern. */
	wchar_t *pattern; /* Pointer to the stored pattern. */
	hashtable *badc_shifts; /* Bad character shift table. */
	unsigned int *goods_shifts; /* Good suffix shift table. */
} bm_preproc_wt;

/*
 * Contains the complete compiled output of the Boyer-Moore preprocessing
 * phase. After a full compilation, both primary data members are filled.
 */
typedef struct {
	bm_preproc_ct stnd; /* Can be used with standard character arrays. */
	bm_preproc_wt wide; /* Can be used with wide character arrays. */

	bool f_linebegin;   /* The match needs to be at the start of a line. */
	bool f_lineend;     /* The match needs to be at the end of a line. */
	bool f_matchall;    /* The pattern matches everything. */

	bool f_wholewords;  /* Only match whole words. */
	bool f_ignorecase;  /* Ignore case when matching. */
	bool f_newline;     /* Unused, line breaks can be matched by ^ or $. */
	bool f_nosub;		/* Don't record matches, only that they exist. */
} bm_preproc_t;


typedef struct {
	int soffset;
	int eoffset;
} bm_match_t;


typedef struct {
	size_t wlen;
	size_t len;

	wchar_t *wpattern;
	char *pattern;

	void *qsBc_table;
	unsigned int qsBc[UCHAR_MAX + 1];

	unsigned int *sbmGs;
	unsigned int *bmGs;

	unsigned int defBc; // Seems redundant

	bool *wescmap; // Unused
	bool *escmap; // Unused
	const char *re_endp; // Unused

  /* flags */
	bool hasdot; // Unused
	bool bol;
	bool eol;
	bool word;
	bool icase;
	bool newline;
	bool nosub;
	bool matchall;
	bool reversed;
} fastmatch_t;

bm_preproc_t *bm_create_preproc();
void bm_free_preproc(bm_preproc_t *prep);
int bm_preprocess_literal(
	bm_preproc_t *result, wchar_t *pattern, size_t len, int cflags);
int bm_preprocess_full(
	bm_preproc_t *result, wchar_t *pattern, size_t len, int cflags);

int bm_execute_stnd(
    bm_match_t result[], size_t nmatch, bm_preproc_t *prep, 
	const char *text, size_t len, int eflags);
int bm_execute_wide(
    bm_match_t result[], size_t nmatch, bm_preproc_t *prep, 
	const wchar_t *text, size_t len, int eflags);

int	frec_proc_literal(fastmatch_t *, const wchar_t *, size_t,
		const char *, size_t, int);
int	frec_proc_fast(fastmatch_t *, const wchar_t *, size_t,
		const char *, size_t, int);
int	frec_match_fast(const fastmatch_t *fg, const void *data,
		size_t len, int type, int nmatch, frec_match_t pmatch[],
		int eflags);
void	frec_free_fast(fastmatch_t *preg);

#endif
