#ifndef REGFAST_H
#define REGFAST_H 1

#include <sys/param.h>

#include <stdio.h>

#include "config.h"
#include "heuristic.h"

#define STR_BYTE	0
#define STR_MBS		1
#define STR_WIDE	2

#ifdef WITH_DEBUG
#define DEBUG_PRINTF(fmt, ...) fprintf(stderr, "%s: " fmt "\n", __func__, __VA_ARGS__);
#define DEBUG_PRINT(str) fprintf(stderr, "%s: %s\n", __func__, str);
#else
#define DEBUG_PRINTF(fmt, args...) do {} while (0)
#define DEBUG_PRINT(str)  do {} while (0)
#endif


typedef struct {
	regex_t orig;
	fastmatch_t *shortcut;
	heur_t *heur;
	const char *re_endp;
	const wchar_t *re_wendp;	
} frec_t;

#define CALL_WITH_OFFSET(fn)						\
do {									\
	size_t slen = (size_t)(pmatch[0].m.rm_eo - pmatch[0].m.rm_so);	\
	size_t offset = pmatch[0].m.rm_so;				\
	DEBUG_PRINTF("search offset and length %ld - %ld", offset, slen);\
	int ret;							\
									\
	if ((long long)pmatch[0].m.rm_eo - pmatch[0].m.rm_so < 0)	\
		return (REG_NOMATCH);					\
	ret = fn;							\
	for (unsigned i = 0; (!(eflags & REG_NOSUB) && (i < nmatch));	\
	    i++) {							\
		pmatch[i].m.rm_so += offset;				\
		pmatch[i].m.rm_eo += offset;				\
		DEBUG_PRINTF("pmatch[%d] offsets %d - %d", i, pmatch[i].m.rm_so, pmatch[i].m.rm_eo);\
	}								\
	DEBUG_PRINTF("returning %d", ret);				\
	return ret;							\
} while (0 /*CONSTCOND*/)

#endif
