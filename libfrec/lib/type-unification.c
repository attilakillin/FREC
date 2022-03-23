
#include <stdlib.h>

#include "type-unification.h"

str_t *
string_create_stnd(const char *stnd, size_t len)
{
    str_t *string = malloc(sizeof(str_t));
    if (string == NULL) {
        return NULL;
    }

    string->stnd = (char *) stnd;
    string->wide = NULL;
    string->len = len;
    string->is_wide = false;

    return string;
}

str_t *
string_create_wide(const wchar_t *wide, size_t len)
{
    str_t *string = malloc(sizeof(str_t));
    if (string == NULL) {
        return NULL;
    }

    string->stnd = NULL;
    string->wide = (wchar_t *) wide;
    string->len = len;
    string->is_wide = true;
    
    return string;
}

void
string_offset_by(str_t *string, size_t offset)
{
    if (string->len >= offset) {
        string->len -= offset;
        if (string->is_wide) {
            string->wide += offset;
        } else {
            string->stnd += offset;
        }
    } else {
        string->len = 0;
        string->stnd = NULL;
        string->wide = NULL;
    }
}

void
string_copy(str_t *dest, const str_t *src)
{
    dest->is_wide = src->is_wide;
    dest->stnd = src->stnd;
    dest->wide = src->wide;
    dest->len = src->len;
}

void
string_excerpt(str_t *dest, const str_t *src, size_t start, size_t end)
{
    dest->is_wide = src->is_wide;
    dest->len = end - start;
    dest->stnd = (!src->is_wide) ? (src->stnd + start) : NULL;
    dest->wide = (src->is_wide) ? (src->wide + start) : NULL;
}

void
string_free(str_t *string)
{
    if (string != NULL) {
        free(string);
    }
}
