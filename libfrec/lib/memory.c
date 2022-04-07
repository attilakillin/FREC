#include <malloc.h>
#include "memory.h"

static bool FREC_OUT_OF_MEMORY = false;

void *
frec_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL) {
        FREC_OUT_OF_MEMORY = true;
        return NULL;
    }

    return ptr;
}

bool
frec_no_mem()
{
    return FREC_OUT_OF_MEMORY;
}
