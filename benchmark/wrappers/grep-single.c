
#ifdef USE_FREC
    #include <frec.h>
#endif

#ifdef USE_POSIX
    #include <regex.h>
#endif

#ifdef USE_TRE
    #include <tre/tre.h>
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

    // Compile pattern.
    int ret;

    #ifdef USE_FREC
        frec_t preg;
        ret = frec_regcomp(&preg, pattern, cflags);
    #endif

    #ifdef USE_POSIX
        regex_t preg;
        ret = regcomp(&preg, pattern, cflags);
    #endif

    #ifdef USE_TRE
        regex_t preg;
        ret = tre_regcomp(&preg, pattern, cflags);
    #endif

    // Handle potential errors.
    if (ret != 0) {
        char buffer[ERROR_BUFFER_SIZE + 1];

        #ifdef USE_FREC
            // TODO No suitable frec_regerror is present currently.
        #endif

        #ifdef USE_POSIX
            regerror(ret, &preg, buffer, ERROR_BUFFER_SIZE);
        #endif

        #ifdef USE_TRE
            tre_regerror(ret, &preg, buffer, ERROR_BUFFER_SIZE);
        #endif

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
    int start = 0;
    while (start < st.st_size) {
        // Only match on the remaining part.
        char *text_offset = &buffer[start];
        ssize_t text_len = st.st_size - start;

        #ifdef USE_FREC
            frec_match_t pmatch;
            ret = frec_regnexec(&preg, text_offset, text_len, 1, &pmatch, eflags);
        #endif

        #ifdef USE_POSIX
            regmatch_t pmatch;
            ret = regexec(&preg, text_offset, 1, &pmatch, eflags);
        #endif

        #ifdef USE_TRE
            regmatch_t pmatch;
            ret = tre_regnexec(&preg, text_offset, text_len, 1, &pmatch, eflags);
        #endif

        // If no more matches were found, break.
        if (ret == REG_NOMATCH) {
            break;
        }

        // On any other error, print the error then exit.
        if (ret != 0) {
            char buffer[ERROR_BUFFER_SIZE + 1];

            #ifdef USE_FREC
                // TODO No suitable frec_regerror is present currently.
            #endif

            #ifdef USE_POSIX
                regerror(ret, &preg, buffer, ERROR_BUFFER_SIZE);
            #endif

            #ifdef USE_TRE
                tre_regerror(ret, &preg, buffer, ERROR_BUFFER_SIZE);
            #endif

            errx(2, "%s", buffer);
        }

        // Otherwise we print the matches.
        #ifdef USE_FREC
            printf("(%ld %ld)\n", start + pmatch.soffset, start + pmatch.eoffset);
            start += pmatch.eoffset;
        #endif

        #ifdef USE_POSIX
            printf("(%d %d)\n", start + pmatch.rm_so, start + pmatch.rm_eo);
            start += pmatch.rm_eo;
        #endif

        #ifdef USE_TRE
            printf("(%d %d)\n", start + pmatch.rm_so, start + pmatch.rm_eo);
            start += pmatch.rm_eo;
        #endif
    }

    return !(ret > 0);
}
