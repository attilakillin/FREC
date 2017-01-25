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

int
frec_convert_pattern_to_wcs(const char *regex, size_t n, wchar_t **w,
    size_t *wn) {
	wchar_t *wregex;
	size_t wlen;

	wregex = malloc(sizeof(wchar_t) * (n + 1));
	if (wregex == NULL)
		return (REG_ESPACE);

  /* If the current locale uses the standard single byte encoding of
     characters, we don't do a multibyte string conversion.  If we did,
     many applications which use the default locale would break since
     the default "C" locale uses the 7-bit ASCII character set, and
     all characters with the eighth bit set would be considered invalid. */
	if (MB_CUR_MAX == 1) {
		unsigned int i;
		const unsigned char *str = (const unsigned char *)regex;
		wchar_t *wstr = wregex;

		for (i = 0; i < n; i++)
			*(wstr++) = *(str++);
		wlen = n;
	} else {
		int consumed;
		wchar_t *wcptr = wregex;
		mbstate_t state;
		memset(&state, '\0', sizeof(state));
		while (n > 0) {
			consumed = mbrtowc(wcptr, regex, n, &state);

			switch (consumed) {
			case 0:
				if (*regex == '\0')
					consumed = 1;
				else {
					free(wregex);
					return (REG_BADPAT);
				}
				break;
			case -1:
				free(wregex);
				return (REG_BADPAT);
			case -2:
			/* The last character wasn't complete.  Let's not call it a
			   fatal error. */
				consumed = n;
				break;
			}
			regex += consumed;
			n -= consumed;
			wcptr++;
		}
		wlen = wcptr - wregex;
	}

	wregex[wlen] = L'\0';
	*w = wregex;
	*wn = wlen;
	return (REG_OK);
}

int
frec_convert_pattern_to_mbs(const wchar_t *wregex, size_t n __unused,
    char **s, size_t *sn) {
	size_t siz;
	char *mbs;

	siz = wcstombs(NULL, wregex, 0);
	if (siz == (size_t)-1)
		return (REG_BADPAT);

	mbs = malloc(siz + 1);
	if (mbs != NULL)
		return (REG_ESPACE);

	wcstombs(mbs, wregex, siz);
	mbs[siz] = '\0';
	*s = mbs;
	*sn = siz;
	return (REG_OK);
}
