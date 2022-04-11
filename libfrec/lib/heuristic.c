
#include <stdlib.h>
#include <string.h>
#include <frec-config.h>
#include "heuristic.h"
#include "regex-parser.h"
#include "string-type.h"

typedef struct heur_parser {
    string *fragments;
    ssize_t frag_index;

    ssize_t max_length;
    bool length_known;

    bool reg_newline_set;
    bool may_match_lf;
    bool has_literal_prefix;
} heur_parser;

// Utility function.
static ssize_t max(ssize_t a, ssize_t b) { return (a > b) ? a : b; }

// Utility function. Asserts that str[at] equals stnd or wide in the correct
// sub-array.
static bool
string_has_char_at(string str, ssize_t at, char stnd, wchar_t wide)
{
    if (str.is_wide) {
        return str.wide[at] == wide;
    } else {
        return str.stnd[at] == stnd;
    }
}

// Initialize the heuristic parser.
static bool
heur_parser_init(heur_parser *parser, int cflags)
{
    // Allocate memory for enough strings.
    parser->fragments = malloc(sizeof(string) * MAX_FRAGMENTS);
    if (parser->fragments == NULL) {
        return false;
    }

    // Set fragment index and max length information.
    parser->frag_index = 0;
    parser->max_length = 0;
    parser->length_known = true;

    // And set the remaining flags.
    parser->reg_newline_set = cflags & REG_NEWLINE;
    parser->may_match_lf = false;
    parser->has_literal_prefix = true;

    return true;
}

// Free the heuristic parser.
static void
heur_parser_free(heur_parser *parser)
{
    if (parser != NULL) {
        // Free every currently occupied string.
        if (parser->fragments != NULL) {
            for (ssize_t i = 0; i < parser->frag_index; i++) {
                string_free(&parser->fragments[i]);
            }
        }

        // Then free the array itself too.
        free(parser->fragments);
    }
}

// Push a fragment to the storage of parser.
// This method creates a deep copy of fragment.
static int
heur_parser_push(heur_parser *parser, string fragment)
{
    // If the fragment's length is zero, return early.
    if (fragment.len == 0) {
        // If the first fragment had zero length, then the prefix isn't literal.
        if (parser->frag_index == 0) {
            parser->has_literal_prefix = false;
        }
        return (REG_OK);
    }

    // If we used up too many fragments, return an error.
    if (parser->frag_index >= MAX_FRAGMENTS) {
        return (REG_BADPAT);
    }

    // Duplicate the given fragment and write it to the fragment array.
    string *write_ptr = &parser->fragments[parser->frag_index];
    bool success = string_duplicate(write_ptr, fragment);
    if (!success) {
        return (REG_ESPACE);
    }

    parser->frag_index++;
    return (REG_OK);
}

// Handle an opening square bracket in the pattern at the given iter
// position.
static int
handle_square_bracket(heur_parser *parser, string pattern, ssize_t *iter)
{
    ssize_t pos = *iter + 1;

    // Check whether the contents are negated (which may match linefeeds).
    if (pos < pattern.len && string_has_char_at(pattern, pos, '^', L'^')) {
        parser->may_match_lf = true;
        pos++;
    }

    // And then advance up to the closing bracket.
    while(pos < pattern.len) {
        if (string_has_char_at(pattern, pos, '[', L'[')) {
            return (REG_BADPAT);
        } else if (string_has_char_at(pattern, pos, '\n', L'\n')) {
            parser->may_match_lf = true;
        } else if (string_has_char_at(pattern, pos, ']', L']')) {
            break;
        }
        pos++;
    }

    *iter = pos;
    return (REG_OK);
}

// Handle an enclosure marked by the op and cl characters as the opening
// and closing characters respectively. Use a wrapper function instead!
static int
handle_enclosure(
    heur_parser *parser, string pattern, ssize_t *iter,
    char stnd_op, char stnd_cl, wchar_t wide_op, wchar_t wide_cl
) {
    ssize_t pos = *iter;

    int depth = 0;

    // While there's chars to read.
    while (pos < pattern.len) {
        if (string_has_char_at(pattern, pos, stnd_op, wide_op)) {
            // Increase depth on an opening character.
            depth++;
        } else if (string_has_char_at(pattern, pos, stnd_cl, wide_cl)) {
            // Decrease it on a closing one.
            depth--;
        } else if (
            string_has_char_at(pattern, pos, '.', L'.')
            || string_has_char_at(pattern, pos, '\n', L'\n')
        ) {
            // And mark that linefeeds can be matched on a '.' or a '\n'.
            parser->may_match_lf = true;
        }

        // If the current depth is 0, break, otherwise advance.
        if (depth == 0) {
            break;
        }
        pos++;
    }

    // If depth is not 0, the opening character was not closed.
    if (depth != 0) {
        return (REG_BADPAT);
    }

    *iter = pos;
    return (REG_OK);
}

// Wrapper for handle_enclosure.
static int
handle_curlybraces(heur_parser *parser, string pattern, ssize_t *iter)
{
    return handle_enclosure(parser, pattern, iter, '{', '}', L'{', L'}');
}

// Wrapper for handle_enclosure.
static int
handle_parentheses(heur_parser *parser, string pattern, ssize_t *iter)
{
    return handle_enclosure(parser, pattern, iter, '(', ')', L'(', L')');
}

// Build final heuristic output from the given parser.
static int
build_heuristic(heur *heuristic, heur_parser parser)
{
    // Set the maximum length.
    heuristic->max_length = (parser.length_known) ? (parser.max_length) : (-1);

    // Set heuristic type.
    if (parser.length_known || parser.reg_newline_set || !parser.may_match_lf) {
        heuristic->heur_type = HEUR_LONGEST;
    } else if (parser.has_literal_prefix) {
        heuristic->heur_type = HEUR_PREFIX;
    } else {
        // We can't use either one of the above heuristics, return early.
        return (REG_BADPAT);
    }

    string best_pattern;

    if (heuristic->heur_type == HEUR_PREFIX) {
        // If prefix heuristics is used, we'll use the first fragment.
        string_reference(&best_pattern, parser.fragments[0]);
    } else {
        // Otherwise, we find the longest fragment.
        size_t i = 0;
        for (size_t j = 1; j < parser.frag_index; j++) {
            if (parser.fragments[j].len > parser.fragments[i].len) {
                i = j;
            }
        }

        // And use that.
        string_reference(&best_pattern, parser.fragments[i]);
    }

    // Compile final Boyer-Moore literal field.
    int ret = bm_compile_literal(&heuristic->literal_comp, best_pattern, 0);
    return ret;
}

/*
 * Preprocess the given pattern with the given length and compilation flags.
 * Outputs the gathered heuristic data into the given heur struct pointer.
 * The struct is fully initialized with this function.
 * 
 * May return REG_BADPAT or REG_ESPACE on failure.
 */
int
frec_preprocess_heur(heur *heuristic, string pattern, int cflags)
{
    // Initialize heuristic parser.
    heur_parser parser;
    bool success = heur_parser_init(&parser, cflags);
    if (!success) {
        return (REG_ESPACE);
    }

    // This fragment will be the one we modify. Its length is set to 0
    // so that it can be filled with the parsed characters.
    string fragment;
    success = string_duplicate(&fragment, pattern);
    if (!success) {
        heur_parser_free(&parser);
        return (REG_ESPACE);
    }
    fragment.len = 0;

    /* Initialize regex parser. */
    regex_parser reg_parser;
    reg_parser.escaped = false;
    reg_parser.extended = cflags & REG_EXTENDED;

    int ret = REG_OK;
    for (ssize_t i = 0; i < pattern.len; i++) {
        // Parse each character.
        parse_result result;
        if (pattern.is_wide) {
            result = parse_wchar(&reg_parser, pattern.wide[i]);
        } else {
            result = parse_char(&reg_parser, pattern.stnd[i]);
        }

        // Execute preliminary actions.
        switch (result) {
            // Save a normal character or a newline.
            case NORMAL_CHAR:
                string_append_from(&fragment, pattern, i);
                parser.max_length++;
                break;
            case NORMAL_NEWLINE:
                string_append(&fragment, '\n', L'\n');
                parser.max_length++;
                parser.may_match_lf = true;
                break;
            // Skip when we should skip any action.
            case SHOULD_SKIP:
                break;
            // In case of a bad pattern or a | char (unsupported),
            // signal an error (handled below the switch).
            case BAD_PATTERN:
            case SPEC_PIPE:
                ret = (REG_BADPAT);
                break;
            // These special characters make the last literal char optional.
            case SPEC_CURLYBRACE:
            case SPEC_ASTERISK:
            case SPEC_QMARK:
                fragment.len = max(0, fragment.len - 1);
            // The above and these others make the pattern variable length.
            case SPEC_PLUS:
            case SPEC_PAREN:
                parser.length_known = false;
            // In the above cases and for any other special character,
            // we need to end the current literal segment.
            default:
                string_null_terminate(&fragment);
                // Return failures are handled below the switch.
                ret = heur_parser_push(&parser, fragment);
                // Reset fragment position for the next fragment.
                fragment.len = 0;
                break;
        }

        if (ret != REG_OK) {
            string_free(&fragment);
            heur_parser_free(&parser);
            return ret;
        }

        // Execute extra actions. For a few special characters, we need to
        // do extra work after finishing the segment.
        switch (result) {
            // A . any character operator may include line feeds, and also
            // modifies the maximum length.
            case SPEC_DOT:
                parser.may_match_lf = true;
                parser.max_length++;
                break;
            /* On a '[' character, we need to advance our iterator. */
            case SPEC_BRACKET:
                ret = handle_square_bracket(&parser, pattern, &i);
                parser.max_length++;
                break;
            /* On a '(' or a '{' we advance too, but with a different logic. */
            case SPEC_PAREN:
                ret = handle_parentheses(&parser, pattern, &i);
                break;
            case SPEC_CURLYBRACE:
                ret = handle_curlybraces(&parser, pattern, &i);
                break;
            default:
                break;
        }

        // If any of the extra actions failed, we return.
        if (ret != REG_OK) {
            string_free(&fragment);
            heur_parser_free(&parser);
            return ret;
        }
    }

    // We exited the loop, which means we read the whole pattern.
    // If the last segment was not finished, we finish it.
    if (fragment.len > 0) {
        string_null_terminate(&fragment);
        ret = heur_parser_push(&parser, fragment);
        if (ret != REG_OK) {
            string_free(&fragment);
            heur_parser_free(&parser);
            return ret;
        }
    }

    // Initialize and fill Boyer-Moore compilation data for the best fragment.
    bm_comp_init(&heuristic->literal_comp, cflags);
    ret = build_heuristic(heuristic, parser);

    // Free Boyer-Moore data if compilation failed.
    if (ret != REG_OK) {
        bm_comp_free(&heuristic->literal_comp);
    }

    // And free the temporary fragment, as well as the heuristic parser.
    string_free(&fragment);
    heur_parser_free(&parser);
    return ret;
}

heur *
frec_create_heur()
{
    heur *heuristic = malloc(sizeof(heur));
    if (heuristic == NULL) {
        return NULL;
    }

    return heuristic;
}

void
frec_free_heur(heur *heuristic)
{
    if (heuristic != NULL) {
        bm_comp_free(&heuristic->literal_comp);
        free(heuristic);
    }
}
