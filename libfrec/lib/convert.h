#ifndef CONVERT_H
#define CONVERT_H 1

#include <stdlib.h>

int frec_convert_wcs_to_mbs(
	const wchar_t *wcs, size_t wcn, char **mbs, size_t *mbn);
int frec_convert_mbs_to_wcs(
	const char *mbs, size_t mbn, wchar_t **wcs, size_t *wcn);

#endif /* CONVERT_H */
