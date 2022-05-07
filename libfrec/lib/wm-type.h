#ifndef FREC_WM_TYPE_H
#define FREC_WM_TYPE_H

#include <sys/types.h>
#include "hashtable.h"
#include "string-type.h"

#define WM_MAXPAT 64

typedef struct wm_comp {
    const string *patterns;  // Pattern array.
    ssize_t count;           // Number of patterns.

    ssize_t len_shortest;    // Length of the shortest pattern.
    ssize_t shift_def;       // Default WM shift.

    hashtable *shift;        // WM shift table.

    int cflags;              // Compilation flags.
} wm_comp;

typedef struct wm_entry {
    ssize_t shift;

    unsigned char suffix_list[WM_MAXPAT];
    ssize_t suffix_cnt;

    unsigned char prefix_list[WM_MAXPAT];
    ssize_t prefix_cnt;
} wm_entry;

void
wm_comp_init(wm_comp *comp, ssize_t count, int cflags);

void
wm_comp_free(wm_comp *comp);

#endif //FREC_WM_TYPE_H
