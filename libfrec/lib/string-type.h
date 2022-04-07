#ifndef FREC_STRING_TYPE_H
#define FREC_STRING_TYPE_H

#include <stdbool.h>
#include <wchar.h>

// Represents a string with a given length.
typedef struct string {
    bool is_wide;  // If set, use the wide element. Otherwise, use stnd.
    char *stnd;    // A standard character array, or NULL.
    wchar_t *wide; // A wide character array, or NULL.
    ssize_t len;    // The length of the relevant array element.

    bool owned;    // Whether we own the encapsulated string or not.
} string;

void
string_borrow(string *str, const void *content, ssize_t len, bool is_wide);

bool
string_copy(string *str, const void *content, ssize_t len, bool is_wide);

bool
string_duplicate(string *target, string src);

void
string_free(string *str);

//str_t *string_create_stnd(const char *stnd, size_t len);

/*
 * Creates a string from the given wide character array.
 * The created string does not take ownership of wide!
 * May return NULL if memory allocation failed.
 */
//str_t *string_create_wide(const wchar_t *wide, size_t len);

/*
 * Creates a shallow copy from the given src string.
 */
//void string_copy(str_t *dest, const str_t *src);

/*
 * Creates an excerpt of src between the start and end positions.
 */
//void string_excerpt(str_t *dest, const str_t *src, size_t start, size_t end);

/*
 * Given a string, offset the relevant text attribute by the given offset.
 */ 
//void string_offset_by(str_t *string, size_t offset);

/*
 * Frees a string created with the string_from_stnd
 * or string_from_wide functions.
 */
//void string_free(str_t *string);

#endif // FREC_STRING_TYPE_H
