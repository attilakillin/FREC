#ifndef MREGEX_H
#define MREGEX_H 1

#include <sys/types.h>
#include <wchar.h>

#include "frec.h"

/* Isn't mfrec_t a better name? */
typedef struct {
	size_t k;		/* Number of patterns */
	frec_t *patterns;	/* regex_t structure for each pattern */
	int type;		/* XXX (private) Matching type */
	int err;		/* XXX (private) Which pattern failed */
	int cflags;		/* XXX (private) cflags */
	void *searchdata;	/* Wu-Manber internal data */
} mregex_t;

extern int	frec_regncomp(frec_t *preg, const char *regex,
		    size_t n, int cflags);
extern int	frec_regcomp(frec_t *preg, const char *regex, int cflags);
extern int	frec_regwncomp(frec_t *preg, const wchar_t *regex,
		    size_t n, int cflags);
extern int	frec_regwcomp(frec_t *preg, const wchar_t *regex,
		    int cflags);
extern void	frec_regfree(frec_t *preg);

extern int	frec_regnexec(const frec_t *preg, const char *str,
		    size_t len, size_t nmatch, frec_match_t pmatch[],
		    int eflags);
extern int	frec_regexec(const frec_t *preg, const char *str,
		    size_t nmatch, frec_match_t pmatch[], int eflags);
extern int	frec_regwnexec(const frec_t *preg, const wchar_t *str,
		    size_t len, size_t nmatch, frec_match_t pmatch[],
		    int eflags);
extern int	frec_regwexec(const frec_t *preg, const wchar_t *str,
		    size_t nmatch, frec_match_t pmatch[], int eflags);

extern int	frec_mregncomp(mregex_t *preg, size_t nr,
		    const char **regex, size_t *n, int cflags);
extern int	frec_mregcomp(mregex_t *preg, size_t nr,
		    const char **regex, int cflags);
extern int	frec_mregnexec(const mregex_t *preg, const char *str,
		    size_t len, size_t nmatch, frec_match_t pmatch[],
		    int eflags);
extern int	frec_mregexec(const mregex_t *preg, const char *str,
		    size_t nmatch, frec_match_t pmatch[], int eflags);
extern void	frec_mregfree(mregex_t *preg);
extern size_t	frec_mregerror(int errcode, const mregex_t *preg,
		    int *errpatn, char *errbuf, size_t errbuf_size);

extern int	frec_mregwncomp(mregex_t *preg, size_t nr,
		    const wchar_t **regex, size_t *n, int cflags);
extern int	frec_mregwcomp(mregex_t *preg, size_t nr,
		    const wchar_t **regex, int cflags);
extern int	frec_mregwnexec(const mregex_t *preg,
		    const wchar_t *str, size_t len, size_t nmatch,
		    frec_match_t pmatch[], int eflags);
extern int	frec_mregwexec(const mregex_t *preg, const wchar_t *str,
		    size_t nmatch, frec_match_t pmatch[], int eflags);
#endif
