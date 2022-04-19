#include <frec-config.h>
#include "wm-comp.h"

#define WM_B 2

static int
unnamed_function()
{

}

int
wm_compile(wm_comp *comp, string *patterns, ssize_t count, int cflags)
{
    // Zero-initialize compilation struct.
    wm_comp_init(comp, cflags);

    // Find and set the shortest pattern length.
    ssize_t len_shortest = patterns[0].len;
    for (ssize_t i = 1; i < count; i++) {
        if (patterns[i].len < len_shortest) {
            len_shortest = patterns[i].len;
        }
    }
    comp->len_shortest = len_shortest;

    // Initialize shift table.
    size_t elem_count = (len_shortest - WM_B + 1) * count * 2;
    size_t size = (patterns[0].is_wide) ? sizeof(wchar_t) : sizeof(char);
    comp->shift = hashtable_init(elem_count, size, sizeof(wm_entry));
    if (comp->shift == NULL) {
        return (REG_ESPACE);
    }
}
