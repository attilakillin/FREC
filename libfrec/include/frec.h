#ifndef LIBFREC_H
#define LIBFREC_H 1

#include <wchar.h>

#include "frec-config.h"
#include "frec-match.h"
#include "frec-types.h"

/* Early declaration of the structs used internally for state management. */
struct frec_t;
struct mfrec_t;

/* Single pattern compilation functions. */
int frec_regcomp(struct frec_t *preg, const char *pattern, int cflags);
int frec_regncomp(struct frec_t *preg, const char *pattern, size_t len, int cflags);
int frec_regwcomp(struct frec_t *preg, const wchar_t *pattern, int cflags);
int frec_regwncomp(struct frec_t *preg, const wchar_t *pattern, size_t len, int cflags);

/* Single pattern execution functions. */
int frec_regexec(const struct frec_t *preg, const char *text, size_t nmatch, struct frec_match_t *pmatch, int eflags);
int frec_regnexec(const struct frec_t *preg, const char *text, size_t len, size_t nmatch, struct frec_match_t *pmatch, int eflags);
int frec_regwexec(const struct frec_t *preg, const wchar_t *text, size_t nmatch, struct frec_match_t *pmatch, int eflags);
int frec_regwnexec(const struct frec_t *preg, const wchar_t *text, size_t len, size_t nmatch, struct frec_match_t *pmatch, int eflags);

/* Multi-pattern compilation functions. */
int frec_mregcomp(struct mfrec_t *preg, size_t k, const char **patterns, int cflags);
int frec_mregncomp(struct mfrec_t *preg, size_t k, const char **patterns, size_t *lens, int cflags);
int frec_mregwcomp(struct mfrec_t *preg, size_t k, const wchar_t **patterns, int cflags);
int frec_mregwncomp(struct mfrec_t *preg, size_t k, const wchar_t **patterns, size_t *lens, int cflags);

/* Multi-pattern execution functions. */
int frec_mregexec(const struct mfrec_t *preg, const char *text, size_t nmatch, struct frec_match_t *pmatch, int eflags);
int frec_mregnexec(const struct mfrec_t *preg, const char *text, size_t len, size_t nmatch, struct frec_match_t *pmatch, int eflags);
int frec_mregwexec(const struct mfrec_t *preg, const wchar_t *text, size_t nmatch, struct frec_match_t *pmatch, int eflags);
int frec_mregwnexec(const struct mfrec_t *preg, const wchar_t *text, size_t len, size_t nmatch, struct frec_match_t *pmatch, int eflags);

/* Multi-pattern error reporting function. */
size_t frec_mregerror(int errcode, const struct mfrec_t *preg, int *errpatn, char *errbuf, size_t errbuf_size);

/* Memory deallocation functions. */
void frec_regfree(struct frec_t *preg);
void frec_mregfree(struct mfrec_t *preg);

#endif
