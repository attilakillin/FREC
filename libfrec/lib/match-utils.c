#include <stdbool.h>

#include "match.h"

static bool
is_linefeed(string text, size_t pos)
{
    return (text.is_wide)
        ? (text.wide[pos] == L'\n')
        : (text.stnd[pos] == '\n');
}

ssize_t
find_lf_backward(string text, ssize_t pos)
{
    while (pos >= 0) {
        if (is_linefeed(text, pos)) {
            return pos;
        }
        pos--;
    }
    return 0;
}

ssize_t
find_lf_forward(string text, ssize_t pos)
{
    while (pos < text.len) {
        if (is_linefeed(text, pos)) {
            return pos;
        }
        pos++;
    }
    return text.len;
}
