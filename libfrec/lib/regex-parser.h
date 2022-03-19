#ifndef REGEX_PARSER_H
#define REGEX_PARSER_H 1

#include <stdbool.h>

typedef enum parse_result_t {
    NORMAL_CHAR,
    NORMAL_NEWLINE,
    SHOULD_SKIP,
    BAD_PATTERN,
    SPEC_DOT,
    SPEC_BRACKET,
    SPEC_CARET,
    SPEC_DOLLAR,
    SPEC_PAREN,
    SPEC_ASTERISK,
    SPEC_PLUS,
    SPEC_QMARK,
    SPEC_PIPE,
    SPEC_CURLYBRACE
} parse_result_t;

typedef struct {
    bool escaped;
    bool extended;
} regex_parser_t;

parse_result_t parse_wchar(regex_parser_t *parser, wchar_t c);

#endif
