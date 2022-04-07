#include <malloc.h>
#include <string.h>

#include "string-type.h"

void
string_borrow(string *str, const void *content, ssize_t len, bool is_wide)
{
    str->is_wide = is_wide;
    str->len = len;
    str->owned = false;

    if (is_wide) {
        str->wide = (wchar_t *) content;
        str->stnd = NULL;
    } else {
        str->wide = NULL;
        str->stnd = (char *) content;
    }
}

bool
string_copy(string *str, const void *content, ssize_t len, bool is_wide)
{
    str->is_wide = is_wide;
    str->len = 0;
    str->owned = true;

    if (is_wide) {
        wchar_t *target = malloc(sizeof(wchar_t) * (len + 1));
        if (target == NULL) {
            return false;
        }

        memcpy(target, content, sizeof(wchar_t) * len);
        target[len] = L'\0';

        str->wide = target;
        str->stnd = NULL;
    } else {
        char *target = malloc(sizeof(char) * (len + 1));
        if (target == NULL) {
            return false;
        }

        memcpy(target, content, sizeof(char) * len);
        target[len] = '\0';

        str->wide = NULL;
        str->stnd = target;
    }

    return true;
}

bool
string_duplicate(string *target, string src)
{
    void *content = (src.is_wide) ? (src.wide) : (src.stnd);
    return string_copy(target, content, src.len, src.is_wide);
}

void
string_free(string *str)
{
    if (str != NULL && str->owned) {
        free(str->stnd);
        free(str->wide);
    }
}

/*
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
*/
