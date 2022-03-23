#ifndef FREC_CONFIG_H
#define FREC_CONFIG_H 1

#include <tre/regex.h>

#define _dist_regcomp	tre_regcomp
#define _dist_regncomp	tre_regncomp
#define _dist_regwcomp	tre_regwcomp
#define _dist_regwncomp	tre_regwncomp

#define _dist_regexec	tre_regexec
#define _dist_regnexec	tre_regnexec
#define _dist_regwexec	tre_regwexec
#define _dist_regwnexec	tre_regwnexec

#define _dist_regfree	tre_regfree

#define _dist_regerror	tre_regerror

#define REG_OK 0

#ifndef REG_LITERAL
    #define REG_LITERAL REG_NOSPEC
#endif

#define _REGCOMP_LAST REG_UNGREEDY

#ifndef REG_WORD
    #define REG_WORD (_REGCOMP_LAST << 1)
#endif

#ifndef REG_GNU
    #define REG_GNU (_REGCOMP_LAST << 2)
#endif

#define _REGEXEC_LAST REG_BACKTRACKING_MATCHER

#ifndef REG_STARTEND
    #define REG_STARTEND (_REGEXEC_LAST << 1)
#endif

#ifndef REG_PEND
    #define REG_PEND (_REGEXEC_LAST << 2)
#endif

#ifndef __unused
    #define __unused
#endif

#ifndef __DECONST
    #define __DECONST(type, var)	((type)(uintptr_t)(const void *)(var))
#endif

#endif
