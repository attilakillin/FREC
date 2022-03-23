#ifndef TYPE_UNIFICATION_H
#define TYPE_UNIFICATION_H 1

#include <stdbool.h>
#include <wchar.h>

/*
 * Represents a string with a given length. Exactly one of stnd and wide
 * is initialized (as set in the is_wide field), the other is always NULL.
 */
typedef struct str_t {
    char *stnd;        /* A standard character array. */
    wchar_t *wide;     /* A wide character array. */
    bool is_wide;      /* Whether the stnd or the wide field should be used. */
    size_t len;        /* The length of the non-null array field. */
} str_t;

/*
 * Creates a string from the given stnd character array.
 * The created string does not take ownership of stnd!
 * May return NULL if memory allocation failed.
 */
str_t *string_create_stnd(const char *stnd, size_t len);

/*
 * Creates a string from the given wide character array.
 * The created string does not take ownership of wide!
 * May return NULL if memory allocation failed.
 */
str_t *string_create_wide(const wchar_t *wide, size_t len);

/*
 * Creates a shallow copy from the given src string.
 */
void string_copy(str_t *dest, const str_t *src);

/*
 * Creates an excerpt of src between the start and end positions.
 */
void string_excerpt(str_t *dest, const str_t *src, size_t start, size_t end);

/*
 * Given a string, offset the relevant text attribute by the given offset.
 */ 
void string_offset_by(str_t *string, size_t offset);

/*
 * Frees a string created with the string_from_stnd
 * or string_from_wide functions.
 */
void string_free(str_t *string);

#endif