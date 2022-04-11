#include <wchar.h>
#include "regex-parser.h"

parse_result
parse_wchar(regex_parser *parser, wchar_t c)
{
    switch (c) {
        // A '\' negates the escaped property.
        case L'\\':
            if (parser->escaped) {
                parser->escaped = false;
                return NORMAL_CHAR;
            } else {
                parser->escaped = true;
                return SHOULD_SKIP;
            }
        // A '\n' can only be parsed if it isn't currently escaped.
        case L'\n':
            if (parser->escaped) {
                parser->escaped = false;
                return BAD_PATTERN;
            } else {
                return NORMAL_NEWLINE;
            }
        // These characters behave the same in both BRE and ERE:
        // If they aren't escaped, they have special meaning.
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
        // These have their behaviour flipped in BRE:
        // In BRE, they only have special meaning if escaped.
        // In ERE, they only have special meaning if not escaped.
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
        // Any other character causes an error if escaped,
        // except an explicit "\n" character sequence.
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

parse_result
parse_char(regex_parser *parser, char c)
{
    return parse_wchar(parser, (wchar_t) c);
}
