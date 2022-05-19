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

// Create a borrowed string into the given str argument.
// Creates a string with the given content and len. If is_wide is set,
// content is assumed to be a wchar_t array, otherwise a char array.
// The content is borrowed: string_free will not deallocate its memory.
void
string_borrow(string *str, const void *content, ssize_t len, bool is_wide);

// Create a new copied string into the given str argument.
// Creates a string with the given content and len. If is_wide is set,
// content is assumed to be a wchar_t array, otherwise a char array.
// The content is copied: string_free will deallocate the internal fields.
bool
string_copy(string *str, const void *content, ssize_t len, bool is_wide);

// References the content from the given src string.
// The underlying content is borrowed, not copied, similar to string_borrow.
// See string_copy and string_borrow for more information.
void
string_reference(string *target, string src);

// Duplicates the given src string into the target string.
// The underlying content is copied, not borrowed, similar to string_copy.
// See string_copy and string_borrow for more information.
bool
string_duplicate(string *target, string src);

// Null-initializes the given string. Either this, borrow, or copy must be
// called in order for the free function to work properly.
void
string_init(string *str);

// Frees the resources associated with the given string. The str pointer
// itself is not freed. If the content was borrowed, it will not be freed.
void
string_free(string *str);

// String modification function: shifts the start pointer of the string with
// the given amount. Will never shift over the total length of the string.
void
string_offset(string *str, ssize_t offset);

// Borrows a specific section of the src string.
void
string_borrow_section(string *target, string src, ssize_t start, ssize_t end);

// Appends the given character to the string (with the assumption that
// there's enough space to do so). Uses stnd or wide respectively.
void
string_append(string *str, char stnd, wchar_t wide);

// Appends the character to target from the given index of src.
void
string_append_from(string *target, string src, ssize_t at);

// Ensures that string is null terminated at its last position.
void
string_null_terminate(string *str);

void *
string_index(string *str, ssize_t at);

bool
string_compare(const string *str_a, ssize_t from_a,
               const string *str_b, ssize_t from_b, ssize_t count);

#endif // FREC_STRING_TYPE_H
