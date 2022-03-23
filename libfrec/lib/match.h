#ifndef MATCH_H
#define MATCH_H 1

#include "frec-internal.h"
#include "type-unification.h"

/*
 * Given a match array with len elements, return the number of
 * valid matches it contains. In other words, the number of
 * elements before the first match with a -1 start and end offset.
 */
size_t count_matches(frec_match_t matches[], size_t len);

/*
 * Find the previous line feed present in the text, starting from pos.
 */
size_t find_lf_backward(str_t *text, size_t pos);

/*
 * Find the next line feed present in the text, starting from pos.
 */
size_t find_lf_forward(str_t *text, size_t pos);

/*
 * The main regular expression matcher function. Given a pmatch array and an
 * nmatch number, find at most nmatch matches and store them in pmatch using
 * the preg preprocessing struct, the given text, and the given eflags.
 * May return REG_OK if matches were found, REG_NOMATCH if none were found,
 * and REG_ESPACE on memory allocation errors.
 * If nmatch is 0, pmatch isn't modified.
 */
int frec_match(frec_match_t pmatch[], size_t nmatch, const frec_t *preg, const str_t *text, int eflags);

int
oldfrec_mmatch(const void *str, size_t len, int type, size_t nmatch,
    frec_match_t pmatch[], int eflags, const mfrec_t *preg);

#endif
