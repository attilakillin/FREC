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
    str->len = len;
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
    void *content = ((src.is_wide) ? src.wide : src.stnd);
    return string_copy(target, content, src.len, src.is_wide);
}

void
string_init(string *str)
{
    str->wide = NULL;
    str->stnd = NULL;
    str->len = 0;
    str->owned = false;
    str->is_wide = false;
}

void
string_free(string *str)
{
    if (str != NULL && str->owned) {
        free(str->stnd);
        free(str->wide);
    }
}

void
string_offset(string *str, ssize_t offset)
{
    if (offset < 0) {
        return;
    }

    if (offset > str->len) {
        offset = str->len;
    }

    if (str->is_wide) {
        str->wide += offset;
    } else {
        str->stnd += offset;
    }
    str->len -= offset;
}

void
string_borrow_section(string *target, string src, ssize_t start, ssize_t end)
{
    target->is_wide = src.is_wide;
    target->len = end - start;
    target->owned = false;

    if (target->is_wide) {
        target->wide = src.wide + start;
        target->stnd = NULL;
    } else {
        target->wide = NULL;
        target->stnd = src.stnd + start;
    }
}
