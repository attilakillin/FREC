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

#include <stdlib.h>
#include <string-type.h>
#include <frec-config.h>

#include "convert.h"

int
convert_wcs_to_mbs(const wchar_t *wcs, ssize_t wn, char **mbs, ssize_t *mn)
{
	/* Check the validity of the wide char string. */
	ssize_t size = (ssize_t) wcstombs(NULL, wcs, 0);
	if (size == (size_t) - 1) {
		return (REG_BADPAT);
	}

	char *result = malloc(sizeof(char) * (size + 1));
	if (result == NULL) {
		return (REG_ESPACE);
	}

	/* Convert and assign to output variables. */
	wcstombs(result, wcs, size);
	result[size] = '\0';

	*mbs = result;
	*mn = size;
	return (REG_OK);
}

int
convert_mbs_to_wcs(const char *mbs, ssize_t mn, wchar_t **wcs, ssize_t *wn)
{	
	/* Check the validity of the multibyte string. */
	ssize_t size = (ssize_t) mbstowcs(NULL, mbs, 0);
	if (size == (size_t) - 1) {
		return (REG_BADPAT);
	}

	wchar_t *result = malloc(sizeof(wchar_t) * (size + 1));
	if (result == NULL) {
		return (REG_ESPACE);
	}

	/* Convert and assign to output variables. */
	mbstowcs(result, mbs, size);
	result[size] = '\0';

	*wcs = result;
	*wn = size;
	return (REG_OK);
}
