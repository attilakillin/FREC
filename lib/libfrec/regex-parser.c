
#include <wchar.h>

#include "regex-parser.h"

/*
 * Utility function that parses a character from a regular expression
 * pattern (using the given parser state), and returns a value from the
 * parse_result_t enum. This value can be used to determine whether the
 * given character had a special meaning or not.
 * May return BAD_PATTERN upon finding invalid constructions.
 */
parse_result_t
parse_wchar(regex_parser_t *parser, wchar_t c)
{
    switch (c) {
    case L'\\':
        if (parser->escaped) {
            parser->escaped = false;
            return NORMAL_CHAR;
        } else {
            parser->escaped = true;
            return SHOULD_SKIP;
        }
    case L'\n':
        if (parser->escaped) {
            parser->escaped = false;
            return BAD_PATTERN;
        } else {
            return NORMAL_NEWLINE;
        }
    case L'.':
    case L'[':
    case L'^':
    case L'$':
    case L'*':
        if (parser->escaped) {
            parser->escaped = false;
            return NORMAL_CHAR;
        } else {
            switch (c) {
                case L'.': return SPEC_DOT;
                case L'[': return SPEC_BRACKET;
                case L'^': return SPEC_CARET;
                case L'$': return SPEC_DOLLAR;
                case L'*': return SPEC_ASTERISK;
            }
        }
    case L'+':
    case L'?':
    case L'(':
    case L'|':
    case L'{':
        if (parser->escaped ^ parser->extended) {
            parser->escaped = false;
            switch (c) {
                case L'+': return SPEC_PLUS;
                case L'?': return SPEC_QMARK;
                case L'(': return SPEC_PAREN;
                case L'|': return SPEC_PIPE;
                case L'{': return SPEC_CURLYBRACE;
            }
        } else {
            parser->escaped = false;
            return NORMAL_CHAR;
        }
    default:
        if (parser->escaped) {
            parser->escaped = false;
            if (c == L'n') {
                return NORMAL_NEWLINE;
            } else {
                return BAD_PATTERN;
            }
        } else {
            return NORMAL_CHAR;
        }
    }
}
