/*-
 * Copyright (C) 2012 Gabor Kovesdan <gabor@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "convert.h"
#include "frec.h"

/*
 * Converts a string in multibyte representation (mbs) with a length
 * of mbn to a string with wide character representation (wcs) and a
 * size of wcn.
 * Returns REG_OK if the conversion is successful, REG_BADPAT if the
 * given mbs string is malformed, and REG_ESPACE on memory errors.
 */
int
frec_convert_mbs_to_wcs(
	const char *mbs, size_t mbn, wchar_t **wcs, size_t *wcn)
{	
	/* Check the validity of the multibyte string. */
	size_t size = mbstowcs(NULL, mbs, 0);
	if (size == (size_t) - 1) {
		return (REG_BADPAT);
	}

	wchar_t *result = malloc(sizeof(wchar_t) * (size + 1));
	if (result == NULL) {
		return (REG_ESPACE);
	}

	mbstowcs(result, mbs, size);
	result[size] = '\0';

	*wcs = result;
	*wcn = size;
	return (REG_OK);
}

/*
 * Converts a string in wide character representation (wcs) with a
 * length of wcn to a string with multibyte representation (mbs)
 * and a size of mbn.
 * Returns REG_OK if the conversion is successful, REG_BADPAT if the
 * given wcs string is malformed, and REG_ESPACE on memory errors.
 */
int
frec_convert_wcs_to_mbs(
	const wchar_t *wcs, size_t wcn, char **mbs, size_t *mbn)
{
	/* Check the validity of the wide char string. */
	size_t size = wcstombs(NULL, wcs, 0);
	if (size == (size_t) - 1) {
		return (REG_BADPAT);
	}

	char *result = malloc(sizeof(char) * (size + 1));
	if (result == NULL) {
		return (REG_ESPACE);
	}

	wcstombs(result, wcs, size);
	result[size] = '\0';

	*mbs = result;
	*mbn = size;
	return (REG_OK);
}
