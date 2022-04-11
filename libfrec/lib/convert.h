#ifndef CONVERT_H
#define CONVERT_H 1

#include <stdlib.h>

/*
 * Converts a string in wide character representation (wcs) with a
 * length of wn to a string with multibyte representation (mbs)
 * and a size of mn.
 * Returns REG_OK if the conversion is successful, REG_BADPAT if the
 * given wcs string is malformed, and REG_ESPACE on memory errors.
 */
int convert_wcs_to_mbs(const wchar_t *wcs, ssize_t wn, char **mbs, ssize_t *mn);

/*
 * Converts a string in multibyte representation (mbs) with a length
 * of mn to a string with wide character representation (wcs) and a
 * size of wn.
 * Returns REG_OK if the conversion is successful, REG_BADPAT if the
 * given mbs string is malformed, and REG_ESPACE on memory errors.
 */
int convert_mbs_to_wcs(const char *mbs, ssize_t mn, wchar_t **wcs, ssize_t *wn);

#endif
