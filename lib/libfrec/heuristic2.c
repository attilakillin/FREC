
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "heuristic.h"
#include "regex-parser.h"


typedef struct {
    wchar_t **fragments;
    size_t *fragment_lens;
    size_t next_index;

    bool reg_newline_set;
    bool may_contain_lf;
    bool has_literal_prefix;
} heur_parser;

static int
end_segment(heur_parser *state, wchar_t *fragment, size_t len)
{
    if (len == 0) {
        if (state->next_index == 0) {
            state->has_literal_prefix = false;
        }

        return (REG_OK);
    }

    if (state->next_index >= MAX_FRAGMENTS) {
        return (REG_BADPAT);
    }

    /* Store the pointed fragment in storage. */
    wchar_t *storage = malloc(sizeof(wchar_t) * (len + 1));
    if (storage == NULL) {
        return (REG_ESPACE);
    }

    memcpy(storage, fragment, sizeof(wchar_t) * len);
    storage[len] = L'\0';

    state->fragments[state->next_index] = storage;
    state->fragment_lens[state->next_index] = len;
    state->next_index++;

    return (REG_OK);
}

static int
handle_square_bracket(
    heur_parser *state, const wchar_t *pattern, size_t len, size_t *iter)
{
    size_t pos = *iter + 1;

    if (pos < len && pattern[pos] == L'^') {
        state->may_contain_lf = true;
        pos++;
    }

    while(pos < len) {
        wchar_t c = pattern[pos];
        if (c == L'[') {
            return (REG_BADPAT);
        } else if (c == L'\n') {
            state->may_contain_lf = true;
        } else if (c == L']') {
            break;
        }
        pos++;
    }

    *iter = pos + 1;
}

static int
handle_enclosed_area(
    heur_parser *state, const wchar_t *pattern, size_t len, size_t *iter,
    wchar_t opening, wchar_t closing)
{
    size_t pos = *iter;

    int depth = 0;

    while (pos < len) {
        wchar_t c = pattern[pos];
        if (c == opening) {
            depth++;
        } else if (c == closing) {
            depth--;
        } else if (c == L'.' || L'\n') {
            state->may_contain_lf = true;
        }
        
        if (depth == 0) {
            break;
        }
        pos++;
    }

    *iter = pos + 1;
}

static int
build_bm_from_best_fragment(heur_parser *state, heur_t *target)
{
    if (/*tlen > 0 ||*/ state->reg_newline_set || !state->may_contain_lf) {
        target->heur_type = HEUR_LONGEST;
    } else if (state->has_literal_prefix) {
        target->heur_type = HEUR_PREFIX;
    } else {
        return (REG_BADPAT);
    }

    wchar_t *pattern;
    size_t len;

    if (target->heur_type == HEUR_PREFIX) {
        pattern = state->fragments[0];
        len = state->fragment_lens[0];
    } else {
        size_t i = 0;
        for (size_t j = 1; j < state->next_index; j++) {
            if (state->fragment_lens[j] > state->fragment_lens[i]) {
                i = j;
            }
        }

        pattern = state->fragments[i];
        len = state->fragment_lens[i];
    }

    return bm_preprocess_literal(target->literal_prep, pattern, len, 0);
}

/*
 * Preprocess the given pattern with the given length and compilation flags.
 * Outputs the gathered heuristic data into the given heur struct pointer.
 * The struct is fully initialized with this function.
 * 
 * May return REG_BADPAT or REG_ESPACE on failure.
 */
int
frec_preprocess_heur(
    heur_t *heur, const wchar_t *pattern, size_t len, int cflags)
{
    /* Initialize heuristic parser state. */
    heur_parser state;
    state.fragments = malloc(sizeof(wchar_t *) * MAX_FRAGMENTS);
    if (state.fragments == NULL) {
        return (REG_ESPACE);
    }
    state.fragment_lens = malloc(sizeof(size_t) * MAX_FRAGMENTS);
    if (state.fragment_lens == NULL) {
        // TODO FREE
        return (REG_ESPACE);
    }
    state.next_index = 0;
    state.reg_newline_set = cflags & REG_NEWLINE;
    state.may_contain_lf = false;
    state.has_literal_prefix = true;

    long max_length = 0;
    bool variable_len = false;

    /* These will store the current fragment and its length. */
    wchar_t *cfragment = malloc(sizeof(wchar_t) * len);
    if (cfragment == NULL) {
        // TODO FREE
        return (REG_ESPACE);
    }
    size_t cpos = 0;

    /* Initialize regex parser. */
    regex_parser_t parser;
    parser.escaped = false;
    parser.extended = cflags & REG_EXTENDED;

    int ret = REG_OK;
    for (size_t i = 0; i < len; i++) {
        /* Parse each character. */
        wchar_t c = pattern[i];
        parse_result_t result = parse_wchar(&parser, c);

        /* Execute preliminary actions. */
        switch (result) {
        /* Save a normal character or a newline. */
        case NORMAL_CHAR:
            cfragment[cpos++] = c;
            max_length++;
            break;
        case NORMAL_NEWLINE:
            cfragment[cpos++] = L'\n';
            max_length++;
            state.may_contain_lf = true;
            break;
        /* Skip when we should skip any action. */
        case SHOULD_SKIP:
            break;
        /* In case of a bad pattern or a | char (unsupported), return. */
        case BAD_PATTERN:
        case SPEC_PIPE:
            // TODO FREE
            return (REG_BADPAT);
        /* The special characters make the last literal char optional. */
        case SPEC_CURLYBRACE:
        case SPEC_ASTERISK:
        case SPEC_QMARK:
            cpos = (cpos <= 0) ? (0) : (cpos - 1);
        /* The above and these others make the pattern variable length. */
        case SPEC_PLUS:
        case SPEC_PAREN:
            variable_len = true;
        /* In the above cases and for any other special character,
         * we need to end the current literal segment. */
        default:
            ret = end_segment(&state, cfragment, cpos);
            if (ret != REG_OK) {
                // TODO FREE
                return ret;
            }
            cpos = 0;
            break;
        }

        /* Execute extra actions. For a few special characters, we need to
         * do extra work before continuing on to the next character. */
        switch (result) {
        /* A . any character operator may include line feeds. */
        case SPEC_DOT:
            state.may_contain_lf = true;
            max_length++;
            break;
        /* On a [ character, we need to advance our iterator. */
        case SPEC_BRACKET:
            ret = handle_square_bracket(&state, pattern, len, &i);
            max_length++;
            break;
        /* On a ( or a { we advance too, but with a different logic. */
        case SPEC_PAREN:
            ret = handle_enclosed_area(&state, pattern, len, &i, L'(', L')');
            break;
        case SPEC_CURLYBRACE:
            ret = handle_enclosed_area(&state, pattern, len, &i, L'{', L'}');
            break;
        }

        /* If any of the extra actions failed, we return. */
        if (ret != REG_OK) {
            // TODO FREE
            return ret;
        }
    }

    /* If the last segment was not finished, we finish it. */
    if (cpos > 0) {
        ret = end_segment(&state, cfragment, cpos);
        if (ret != REG_OK) {
            // TODO FREE
            return ret;
        }
    }

    heur->literal_prep = bm_create_preproc();
    if (heur->literal_prep == NULL) {
        // TODO FREE
        return (REG_ESPACE);
    }

    heur->max_length = (variable_len) ? -1 : max_length;
    ret = build_bm_from_best_fragment(&state, heur);

    // TODO FREE
    return ret;
}
