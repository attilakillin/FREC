
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

#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERROR_BUFFER_SIZE 512

int
main(int argc, char *argv[])
{
    char *pattern = NULL;
    int cflags = 0;
    int eflags = 0;

    cflags |= REG_EXTENDED;

    // Process command line arguments.
    bool pattern_set = false;
    int c;
    while ((c = getopt(argc, argv, "e:l")) != -1) {
        switch (c) {
            // Save as the pattern to search for.
            case 'e':
                pattern = malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(pattern, optarg);

                pattern_set = true;
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
    if (!pattern_set || argc == 0) {
        printf("Usage: grep -e PATTERN [-l] INPUT\n");
        exit(2);
    }

    // Compile pattern using variant-specific macros.
    int ret;

    preg_t preg;
    ret = regcomp_func(&preg, pattern, cflags);

    // Handle potential errors.
    if (ret != 0) {
        char buffer[ERROR_BUFFER_SIZE + 1];

        regerror_func(ret, &preg, buffer, ERROR_BUFFER_SIZE);
        errx(2, "%s : %s", pattern, buffer);
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
    ssize_t start = 0;
    while (start < st.st_size) {
        // Only match on the remaining part.
        char *text_offset = &buffer[start];
        ssize_t text_len = st.st_size - start;

        match_t pmatch;

        #ifdef USE_FREC
           ret = frec_regnexec(&preg, text_offset, text_len, 1, &pmatch, eflags);
        #endif

        #ifdef USE_POSIX
            ret = regexec(&preg, text_offset, 1, &pmatch, eflags);
        #endif

        #ifdef USE_TRE
           ret = tre_regnexec(&preg, text_offset, text_len, 1, &pmatch, eflags);
        #endif

        // If no more matches were found, break.
        if (ret == REG_NOMATCH) {
            break;
        }

        // On any other error, print the error then exit.
        if (ret != 0) {
            char buffer[ERROR_BUFFER_SIZE + 1];

            regerror_func(ret, &preg, buffer, ERROR_BUFFER_SIZE);
            errx(2, "%s", buffer);
        }

        // Otherwise we print the matches.
        #ifdef USE_FREC
            printf("(%ld %ld)\n", start + pmatch.soffset, start + pmatch.eoffset);
            start += pmatch.eoffset;
        #else
            printf("(%ld %ld)\n", start + pmatch.rm_so, start + pmatch.rm_eo);
            start += pmatch.rm_eo;
        #endif
    }

    return !(ret > 0);
}
