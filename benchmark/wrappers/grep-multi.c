/*
 * Grep Multi: Builds an executable that can be used to find every match
 * in a text with multiple given patterns.
 */

#include <frec.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    // Compile pattern.
    int ret;

    mfrec_t preg;
    ret = frec_mregcomp(&preg, pattern_cnt, (const char **) patterns, cflags);

    // Handle potential errors.
    if (ret != 0) {
        char buffer[ERROR_BUFFER_SIZE + 1];

        int which = 0;
        frec_mregerror(ret, &preg, &which, buffer, ERROR_BUFFER_SIZE);
        errx(2, "%s : %s", patterns[which], buffer);
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

        frec_match_t pmatch;

        ret = frec_mregnexec(&preg, text_offset, text_len, 1, &pmatch, eflags);

        // If no more matches were found, break.
        if (ret == REG_NOMATCH) {
            break;
        }

        // On any other error, print the error then exit.
        if (ret != 0) {
            char buffer[ERROR_BUFFER_SIZE + 1];

            frec_mregerror(ret, &preg, NULL, buffer, ERROR_BUFFER_SIZE);
            errx(2, "%s", buffer);
        }

        // Otherwise we print the matches.
        printf("%ld (%ld %ld)\n", pmatch.pattern_id, start + pmatch.soffset, start + pmatch.eoffset);
        start += pmatch.eoffset;
    }

    return !(ret > 0);
}
