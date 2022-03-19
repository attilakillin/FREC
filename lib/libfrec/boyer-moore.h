#ifndef BOYER_MOORE_H
#define BOYER_MOORE_H 1

#include <limits.h>
#include <stdbool.h>
#include <wchar.h>

#include "config.h"
#include "frec-match.h"
#include "hashtable.h"

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


bm_preproc_t *bm_create_preproc();
void bm_free_preproc(bm_preproc_t *prep);

int bm_preprocess_literal(
	bm_preproc_t *result, const wchar_t *pattern, size_t len, int cflags);
int bm_preprocess_full(
	bm_preproc_t *result, const wchar_t *pattern, size_t len, int cflags);

int bm_execute_stnd(
    frec_match_t result[], size_t nmatch, bm_preproc_t *prep, 
	const char *text, size_t len, int eflags);
int bm_execute_wide(
    frec_match_t result[], size_t nmatch, bm_preproc_t *prep, 
	const wchar_t *text, size_t len, int eflags);

#endif
