#ifndef REGEXEC_H
#define REGEXEC_H 1

#include <tre/regex.h>

#include "config.h"
#include "frec.h"

int	frec_match(const frec_t *preg, const void *str, size_t len,
	    int type, size_t nmatch, frec_match_t pmatch[], int eflags);
#endif
