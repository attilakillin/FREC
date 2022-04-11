#ifndef FREC_MATCH_H
#define FREC_MATCH_H

#include "frec-internal.h"
#include "string-type.h"

// Find the previous line feed present in the text, starting from pos.
ssize_t find_lf_backward(string text, ssize_t pos);

// Find the next line feed present in the text, starting from pos.
ssize_t find_lf_forward(string text, ssize_t pos);

// The main regular expression matcher function. Given a pmatch array and an
// nmatch number, finds the first match and store at most n submatch in pmatch.
// The matching is done using the preg preprocessing struct, the given text,
// and the given eflags. Returns REG_OK if a match was found, REG_NOMATCH if
// none were found, and REG_ESPACE on memory allocation errors.
// If nmatch is 0, pmatch isn't modified.
int frec_match(frec_match_t pmatch[], size_t nmatch, const frec_t *preg, string text, int eflags);

int
oldfrec_mmatch(const void *str, size_t len, int type, size_t nmatch,
    frec_match_t pmatch[], int eflags, const mfrec_t *preg);

#endif // FREC_MATCH_H
