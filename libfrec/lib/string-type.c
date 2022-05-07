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

void
string_reference(string *target, string src)
{
    void *content = ((src.is_wide) ? src.wide : src.stnd);
    return string_borrow(target, content, src.len, src.is_wide);
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

void
string_append(string *str, char stnd, wchar_t wide)
{
    if (str->is_wide) {
        str->wide[str->len] = wide;
    } else {
        str->stnd[str->len] = stnd;
    }
    str->len++;
}

void
string_append_from(string *target, string src, ssize_t at)
{
    if (src.is_wide) {
        string_append(target, 'x', src.wide[at]);
    } else {
        string_append(target, src.stnd[at], L'x');
    }
}

void
string_null_terminate(string *str)
{
    if (str->is_wide) {
        str->wide[str->len] = L'\0';
    } else {
        str->stnd[str->len] = '\0';
    }
}

void *
string_index(string *str, ssize_t at)
{
    if (str->is_wide) {
        return &str->wide[at];
    } else {
        return &str->stnd[at];
    }
}

int
string_compare(
    const string *str_a, ssize_t from_a,
    const string *str_b, ssize_t from_b, ssize_t count
) {
    if (str_a->is_wide != str_b->is_wide) {
        return -1;
    }

    if (str_a->is_wide) {
        return memcmp(&str_a->wide[from_a], &str_b->wide[from_b], count);
    } else {
        return memcmp(&str_a->stnd[from_a], &str_b->stnd[from_b], count);
    }
}
