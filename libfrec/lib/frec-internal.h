#ifndef FREC_INTERNAL_H
#define FREC_INTERNAL_H 1

#include <tre/regex.h>
#include <wchar.h>
#include <frec.h>

#include "boyer-moore.h"
#include "heuristic.h"

// TODO I really wish to remove these
#define STR_BYTE	0
#define STR_MBS		1
#define STR_WIDE	2

// TODO and these
#ifdef WITH_DEBUG
	#define DEBUG_PRINTF(fmt, ...) fprintf(stderr, "%s: " fmt "\n", __func__, __VA_ARGS__);
	#define DEBUG_PRINT(str) fprintf(stderr, "%s: %s\n", __func__, str);
#else
	#define DEBUG_PRINTF(fmt, ...) do {} while (0)
	#define DEBUG_PRINT(str)  do {} while (0)
#endif

#endif
