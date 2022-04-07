#ifndef FREC_MEMORY_H
#define FREC_MEMORY_H

#include <stdbool.h>
#include <stddef.h>

void *frec_malloc(size_t size);

bool frec_no_mem();

#endif
