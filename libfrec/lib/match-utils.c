
#include <stdbool.h>

#include "match.h"

size_t
count_matches(frec_match_t matches[], size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (matches[i].soffset == -1 && matches[i].eoffset == -1) {
            return i;
        }
    }

    return len;
}

static bool
is_linefeed(str_t *text, size_t pos)
{
    return (text->is_wide)
        ? (text->wide[pos] == L'\n')
        : (text->stnd[pos] == '\n');
}

size_t
find_lf_backward(str_t *text, size_t pos)
{
    while (pos >= 0) {
        if (is_linefeed(text, pos)) {
            return pos;
        }
        pos--;
    }
    return 0;
}

size_t
find_lf_forward(str_t *text, size_t pos)
{
    while (pos < text->len) {
        if (is_linefeed(text, pos)) {
            return pos;
        }
        pos++;
    }
    return text->len;
}
