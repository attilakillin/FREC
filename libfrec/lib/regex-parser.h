#ifndef FREC_REGEX_PARSER_H
#define FREC_REGEX_PARSER_H

#include <stdbool.h>

// An enum representing the possible return values of the regex parser.
typedef enum parse_result {
	NORMAL_CHAR,     // A normal character or an escaped special character.
	NORMAL_NEWLINE,  // A normal '\n' newline character.
	SHOULD_SKIP,     // A (state-modifying) character that can be skipped.
	BAD_PATTERN,     // The given pattern was invalid. Must be handled!
	SPEC_DOT,        // A '.' character with special meaning.
	SPEC_BRACKET,    // A '[' character with special meaning.
	SPEC_CARET,      // A '^' character with special meaning.
	SPEC_DOLLAR,     // A '$' character with special meaning.
	SPEC_PAREN,      // A '(' character with special meaning.
	SPEC_ASTERISK,   // A '*' character with special meaning.
	SPEC_PLUS,       // A '+' character with special meaning.
	SPEC_QMARK,      // A '?' character with special meaning.
	SPEC_PIPE,       // A '|' character with special meaning.
	SPEC_CURLYBRACE  // A '{' character with special meaning.
} parse_result;

// The internal state struct of the regex parser.
typedef struct regex_parser {
	bool escaped;    // Whether the next character will be escaped or not.
	bool extended;   // Whether ERE or BRE should be used. If set, use ERE.
} regex_parser;

// Utility function that parses a character from a regular expression
// pattern sequence (using the given parser state), and returns a value from
// the parse_result enum. This value can be used to determine whether the
// given character had a special meaning or not.
// May return BAD_PATTERN if given an invalid pattern sequence.
parse_result
parse_wchar(regex_parser *parser, wchar_t c);

// Utility function that parses a character from a regular expression
// pattern sequence (using the given parser state), and returns a value from
// the parse_result enum. This value can be used to determine whether the
// given character had a special meaning or not.
// May return BAD_PATTERN if given an invalid pattern sequence.
parse_result
parse_char(regex_parser *parser, char c);

#endif // FREC_REGEX_PARSER_H
