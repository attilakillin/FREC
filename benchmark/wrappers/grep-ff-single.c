/*
 * Grep First From Single: Given properly defined flags, builds an executable
 * that can be used to find the first match in a text that matches any one of
 * the supplied patterns. As such, this is a purposefully simple
 * implementation, intended to demonstrate the strength of
 * multi-pattern matching.
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_FREC
    #include <frec.h>

    #define preg_t frec_t
    #define match_t frec_match_t
    #define regcomp_func frec_regcomp
    #define regerror_func frec_regerror
#endif

#ifdef USE_POSIX
    #include <regex.h>

    #define preg_t regex_t
    #define match_t regmatch_t
    #define regcomp_func regcomp
    #define regerror_func regerror
#endif

#ifdef USE_TRE
    #include <tre/tre.h>

    #define preg_t regex_t
    #define match_t regmatch_t
    #define regcomp_func tre_regcomp
    #define regerror_func tre_regerror
#endif

#define MAX_REGEX_PATTERNS 8
#define ERROR_BUFFER_SIZE 512

int
main(int argc, char *argv[])
{
    char **patterns = malloc(sizeof(char *) * MAX_REGEX_PATTERNS);
    int pattern_cnt = 0;
    int cflags = 0;
    int eflags = 0;

    cflags |= REG_EXTENDED;

    // Process command line arguments.
    int c;
    while ((c = getopt(argc, argv, "e:l")) != -1) {
        switch (c) {
            // Save as the pattern to search for.
            case 'e':
                patterns[pattern_cnt] = malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(patterns[pattern_cnt], optarg);
                
                pattern_cnt++;
                break;
            // This means we have to set REG_NEWLINE.
            case 'l':
                cflags |= REG_NEWLINE;
                break;
        }
    }

    // Shift arguments after we processed every flag.
    argv += optind;
    argc -= optind;

    // If the pattern isn't set, or no argument remains, exit.
    if (pattern_cnt == 0 || argc == 0) {
        printf("Usage: grep -e PATTERN [-e PATTERN...] [-l] INPUT\n");
        exit(2);
    }

    // Compile pattern using variant-specific macros.
    int ret;

    preg_t preg;

    match_t best;
    #ifdef USE_FREC
        best.soffset = -1;
        best.eoffset = -1;
    #else
        best.rm_so = -1;
        best.rm_eo = -1;
    #endif

    for (int curr = 0; curr < pattern_cnt; curr++) {
        ret = regcomp_func(&preg, patterns[curr], cflags);

        // Handle potential errors.
        if (ret != 0) {
            char buffer[ERROR_BUFFER_SIZE + 1];

            regerror_func(ret, &preg, buffer, ERROR_BUFFER_SIZE);
            errx(2, "%s : %s", patterns[curr], buffer);
        }

        // Check if file exists.
        struct stat st;
        if (stat(*argv, &st) != 0 || !S_ISREG(st.st_mode)) {
            errx(2, "Invalid file: %s", *argv);
        }

        // Open file and create read buffer.
        int file = open(*argv, O_RDONLY);
        if (file < 0) {
            errx(2, "Invalid file: %s", *argv);
        }

        int map_flags = MAP_PRIVATE;
        char *buffer = mmap(NULL, st.st_size, PROT_READ, map_flags, file, 0);
        if (buffer == MAP_FAILED) {
            errx(2, "Invalid file buffer from file: %s", *argv);
        }

        // Execute regex matching.
        match_t pmatch;

        #ifdef USE_FREC
            ret = frec_regnexec(&preg, buffer, st.st_size, 1, &pmatch, eflags);
        #endif

        #ifdef USE_POSIX
            ret = regexec(&preg, buffer, 1, &pmatch, eflags);
        #endif

        #ifdef USE_TRE
            ret = tre_regnexec(&preg, buffer, st.st_size, 1, &pmatch, eflags);
        #endif

        // If no more matches were found, break.
        if (ret == REG_NOMATCH) {
            continue;
        }

        // On any other error, print the error then exit.
        if (ret != 0) {
            char buffer[ERROR_BUFFER_SIZE + 1];

            regerror_func(ret, &preg, buffer, ERROR_BUFFER_SIZE);
            errx(2, "%s", buffer);
        }

        // Otherwise we check if this match is earlier than our best.
        #ifdef USE_FREC
            if (best.soffset == -1 || pmatch.soffset < best.soffset) {
                best = pmatch;
            }
        #else
            if (best.rm_so == -1 || pmatch.rm_so < best.rm_so) {
                best = pmatch;
            }
        #endif
    }

    #ifdef USE_FREC
        if (best.soffset != -1) printf("(%ld %ld)\n", best.soffset, best.eoffset);
        return !(best.soffset == -1);
    #else
        if (best.rm_so != -1) printf("(%d %d)\n", best.rm_so, best.rm_eo);
        return !(best.rm_so == -1);
    #endif
}
