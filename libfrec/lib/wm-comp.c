#include <frec-config.h>
#include "wm-comp.h"
#include "wm-type.h"

#define WM_B 2

// Utility functions
static ssize_t min(ssize_t a, ssize_t b) { return (a < b) ? a : b; }


int
wm_compile(wm_comp *comp, string *patterns, ssize_t count, int cflags)
{
    // Zero-initialize compilation struct.
    wm_comp_init(comp, count, cflags);

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

    // Stores the current entry.
    wm_entry entry;

    // For each pattern string
    for (ssize_t i = 0; i < count; i++) {

        // Process the current string char by char
        for (ssize_t j = 0; j <= len_shortest - WM_B; j++) {
            ssize_t shift = len_shortest - WM_B - j;

            void *curr_char = string_index(&patterns[i], j);
            int ret = hashtable_get(comp->shift, curr_char, &entry);

            switch (ret) {
                case HASH_NOTFOUND:
                    entry.shift = shift;
                    entry.prefix_cnt = 0;
                    entry.suffix_cnt = 0;
                    break;
                case HASH_OK:
                    entry.shift = min(entry.shift, shift);
                    break;
            }

            // If we are at the first or last char, update prefix / suffix list
            if (j == 0) {
                entry.prefix_list[entry.prefix_cnt++] = i;
            } else if (j == len_shortest - WM_B) {
                entry.suffix_list[entry.suffix_cnt++] = i;
            }

            ret = hashtable_put(comp->shift, curr_char, &entry);
            if (ret != HASH_OK && ret != HASH_UPDATED) {
                return (REG_BADPAT);
            }
        }
    }

    return (REG_OK);
}

int
wm_execute(frec_match_t *result, const wm_comp *comp, string text, int eflags)
{
    ssize_t pos = comp->len_shortest;

    wm_entry s_entry, p_entry;

    while (pos < text.len) {
        void *curr_char = string_index(&text, pos - WM_B);
        int ret = hashtable_get(comp->shift, curr_char, &s_entry);

        ssize_t shift = (ret == HASH_OK) ? s_entry.shift : comp->shift_def;

        if (shift != 0) {
            pos += shift;
        } else {
            curr_char = string_index(&text, pos - comp->len_shortest);
            ret = hashtable_get(comp->shift, curr_char, &p_entry);

            if (ret == HASH_NOTFOUND) {
                pos++;
                continue;
            }

            for (ssize_t i = 0; i < p_entry.prefix_cnt; i++) {
                for (ssize_t j = 0; j < s_entry.suffix_cnt; j++) {
                    unsigned char s_id = s_entry.suffix_list[j];
                    unsigned char p_id = p_entry.prefix_list[i];

                    if (s_id < p_id) {
                        pos++;
                        continue;
                    }
                    if (s_id > p_id) {
                        break;
                    }

                    const string *curr_pat = &comp->patterns[s_id];
                    if (text.len - pos >= curr_pat->len - comp->len_shortest) {
                        if (string_compare(
                            curr_pat, 0,
                            &text, pos - curr_pat->len,
                            curr_pat->len
                        )) {
                            if (!(comp->cflags & REG_NOSUB) && result != NULL) {
                                result->soffset = pos - curr_pat->len;
                                result->eoffset = pos;
                                result->pattern_id = s_id;
                            }
                            return (REG_OK);
                        }
                    }
                }
            }
        }
    }

    return (REG_NOMATCH);
}
